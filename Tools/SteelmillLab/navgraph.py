#!/usr/bin/env python3
"""BotNav graph export: validator, offline analysis and fixture generator (stdlib only).

The format is specified in Docs/Research/SteelmillLab/BOTNAV_EXPORT_CONTRACT.md
(schema "warsbots/botnav-graph/1"). No such export exists in the game yet; until Codex adds
it, this tool runs on FIXTURES only, which are always flagged `"fixture": true`.

Commands
  validate GRAPH                 structural checks of an export (ids, ranges, edges, build params)
  analyze  GRAPH [--movement M]  components, degree, vertical links; optionally re-runs the
                                 logged failed A* queries offline (approximation: no physics rays)
  fixture  OUT                   writes a small synthetic graph for tests and viewer demos
"""
import argparse
import heapq
import json
import math
import sys
from collections import Counter, deque
from pathlib import Path

SCHEMA = "warsbots/botnav-graph/1"
NODE_COLUMNS = ["id", "x", "y", "z", "ix", "iy", "layer"]
EDGE_COLUMNS = ["from", "to", "cost"]
MAX_NODES = 2_000_000
MAX_EDGES = 20_000_000
EXPANSION_CAP = 6000      # BotNav::FindPath (BotNav.cpp:225)


class GraphError(ValueError):
    pass


def load(path):
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    return data


def fingerprint(g):
    """SHA-256 over the canonical node/edge text of BOTNAV_EXPORT_CONTRACT.md section 5."""
    import hashlib
    h = hashlib.sha256()
    for n in g["nodes"]:
        h.update(("%d %.3f %.3f %.3f %d %d %d\n" % tuple(n)).encode())
    for a, c, cost in g["edges"]:
        h.update(("%d %d %.4f\n" % (a, c, cost)).encode())
    return h.hexdigest()


def validate(g):
    """Return (errors, warnings). Errors make the graph unusable for memory linking."""
    err, warn = [], []
    if g.get("schema") != SCHEMA:
        err.append(f"schema must be {SCHEMA!r}, got {g.get('schema')!r}")
    b = g.get("build", {})
    for k in ("nx", "ny", "cell", "min_x", "min_y", "min_z", "max_z", "max_layers"):
        if k not in b:
            err.append(f"build.{k} missing")
    if g.get("node_columns") != NODE_COLUMNS:
        err.append(f"node_columns must be {NODE_COLUMNS}")
    if g.get("edge_columns") != EDGE_COLUMNS:
        err.append(f"edge_columns must be {EDGE_COLUMNS}")
    if err:
        return err, warn
    nodes, edges = g.get("nodes", []), g.get("edges", [])
    if len(nodes) > MAX_NODES or len(edges) > MAX_EDGES:
        err.append("graph exceeds size limits")
        return err, warn
    nx, ny, cell = b["nx"], b["ny"], b["cell"]
    layers = Counter()
    for i, n in enumerate(nodes):
        if len(n) != 7:
            err.append(f"node {i}: expected 7 fields")
            continue
        nid, x, y, z, ix, iy, layer = n
        if nid != i:
            err.append(f"node {i}: id {nid} != index (ids must follow BotNav order)")
        if not all(isinstance(v, (int, float)) and math.isfinite(v) for v in (x, y, z)):
            err.append(f"node {i}: non-finite position")
            continue
        if not (0 <= ix < nx and 0 <= iy < ny):
            err.append(f"node {i}: column ({ix},{iy}) outside {nx}x{ny}")
        cx, cy = b["min_x"] + (ix + .5) * cell, b["min_y"] + (iy + .5) * cell
        if abs(x - cx) > .01 or abs(y - cy) > .01:
            warn.append(f"node {i}: position not at column centre ({x:.2f},{y:.2f}) vs ({cx:.2f},{cy:.2f})")
        if not (b["min_z"] - 1 <= z <= b["max_z"] + 1):
            err.append(f"node {i}: z {z} outside build z range")
        if not (0 <= layer < b["max_layers"]):
            err.append(f"node {i}: layer {layer} outside 0..{b['max_layers'] - 1}")
        layers[(ix, iy, layer)] += 1
        if len(err) > 50:
            err.append("too many errors; stopping")
            return err, warn
    dup = [k for k, c in layers.items() if c > 1]
    if dup:
        err.append(f"{len(dup)} duplicate (column, layer) pairs, e.g. {dup[0]}")
    seen = set()
    n = len(nodes)
    for j, e in enumerate(edges):
        if len(e) != 3:
            err.append(f"edge {j}: expected 3 fields")
            continue
        a, c, cost = e
        if not (isinstance(a, int) and isinstance(c, int) and 0 <= a < n and 0 <= c < n):
            err.append(f"edge {j}: endpoint out of range")
            continue
        if a == c:
            err.append(f"edge {j}: self loop")
        if (a, c) in seen:
            warn.append(f"edge {j}: duplicate {a}->{c}")
        seen.add((a, c))
        if not (isinstance(cost, (int, float)) and cost > 0 and math.isfinite(cost)):
            err.append(f"edge {j}: bad cost {cost}")
            continue
        pa, pc = nodes[a][1:4], nodes[c][1:4]
        d = math.dist(pa, pc)
        if abs(d - cost) > 0.05:
            warn.append(f"edge {j}: cost {cost:.3f} != euclidean {d:.3f}")
        if abs(nodes[a][4] - nodes[c][4]) > 1 or abs(nodes[a][5] - nodes[c][5]) > 1:
            err.append(f"edge {j}: connects non-adjacent columns")
        if len(err) > 50:
            err.append("too many errors; stopping")
            return err, warn
    fp = g.get("fingerprint")
    if fp and len(fp) == 64 and fp != fingerprint(g):
        err.append("fingerprint does not match nodes/edges")
    counts = g.get("counts")
    if counts and (counts.get("nodes") != n or counts.get("edges") != len(edges)):
        err.append(f"counts header {counts} disagrees with arrays ({n}, {len(edges)})")
    return err, warn[:200]


class Graph:
    def __init__(self, g):
        self.g = g
        self.b = g["build"]
        self.nodes = g["nodes"]
        self.adj = [[] for _ in self.nodes]
        self.radj = [[] for _ in self.nodes]
        for a, c, cost in g["edges"]:
            self.adj[a].append((c, cost))
            self.radj[c].append(a)
        self.columns = {}
        for n in self.nodes:
            self.columns.setdefault((n[4], n[5]), []).append(n[0])

    def pos(self, i):
        return self.nodes[i][1:4]

    def nearest(self, p):
        """BotNav::Nearest without its physics LOS ray (so it can only be MORE permissive)."""
        cell = self.b["cell"]
        cx, cy = int((p[0] - self.b["min_x"]) // cell), int((p[1] - self.b["min_y"]) // cell)
        best, score = -1, 1e18
        for dx in range(-2, 3):
            for dy in range(-2, 3):
                for i in self.columns.get((cx + dx, cy + dy), ()):
                    q = self.pos(i)
                    dz = abs(q[2] - p[2])
                    if dz > 2.5:
                        continue
                    s = (q[0] - p[0]) ** 2 + (q[1] - p[1]) ** 2 + dz * dz * 4
                    if s < score:
                        best, score = i, s
        return best

    def astar(self, a, b, cap=EXPANSION_CAP, memory=None):
        g = {a: 0.0}
        parent = {}
        closed = set()
        goal = self.pos(b)
        heap = [(math.dist(self.pos(a), goal), a)]
        expanded = 0
        while heap and expanded < cap:
            _, cur = heapq.heappop(heap)
            if cur in closed:
                continue
            closed.add(cur)
            expanded += 1
            if cur == b:
                break
            for to, cost in self.adj[cur]:
                pen = 1.0 + 0.6 * min(memory[to][0], 6.0) if memory else 1.0
                c = g[cur] + cost * pen
                if c < g.get(to, 1e30):
                    g[to] = c
                    parent[to] = cur
                    heapq.heappush(heap, (c + math.dist(self.pos(to), goal), to))
        return (b in parent or a == b), expanded

    def components(self):
        """Weakly connected components and strongly connected component count (Kosaraju)."""
        n = len(self.nodes)
        comp = [-1] * n
        k = 0
        for s in range(n):
            if comp[s] >= 0:
                continue
            q = deque([s])
            comp[s] = k
            while q:
                u = q.popleft()
                for v, _ in self.adj[u]:
                    if comp[v] < 0:
                        comp[v] = k
                        q.append(v)
                for v in self.radj[u]:
                    if comp[v] < 0:
                        comp[v] = k
                        q.append(v)
            k += 1
        order, seen = [], [False] * n
        for s in range(n):
            if seen[s]:
                continue
            stack = [(s, iter(self.adj[s]))]
            seen[s] = True
            while stack:
                u, it = stack[-1]
                nxt = next(it, None)
                if nxt is None:
                    stack.pop()
                    order.append(u)
                elif not seen[nxt[0]]:
                    seen[nxt[0]] = True
                    stack.append((nxt[0], iter(self.adj[nxt[0]])))
        scc = [-1] * n
        c = 0
        for s in reversed(order):
            if scc[s] >= 0:
                continue
            q = [s]
            scc[s] = c
            while q:
                u = q.pop()
                for v in self.radj[u]:
                    if scc[v] < 0:
                        scc[v] = c
                        q.append(v)
            c += 1
        return comp, scc


def analyze(g, movement=None, mem=None):
    G = Graph(g)
    comp, scc = G.components()
    sizes = Counter(comp)
    scc_sizes = Counter(scc)
    deg = [len(a) for a in G.adj]
    vertical = Counter()
    for a, c, _ in g["edges"]:
        dz = G.pos(c)[2] - G.pos(a)[2]
        vertical["up>0.75" if dz > .75 else "down<-0.75" if dz < -.75 else "level"] += 1
    largest = sizes.most_common(1)[0][0] if sizes else None
    rep = dict(fixture=bool(g.get("fixture")), nodes=len(g["nodes"]), edges=len(g["edges"]),
               weak_components=len(sizes), largest_component=sizes.most_common(1)[0][1] if sizes else 0,
               small_components=sum(1 for s in sizes.values() if s < 10),
               strong_components=len(scc_sizes), largest_strong=scc_sizes.most_common(1)[0][1] if scc_sizes else 0,
               dead_ends=sum(1 for d in deg if d == 0), max_out_degree=max(deg) if deg else 0,
               layers=dict(Counter(n[6] for n in g["nodes"])), edge_vertical=dict(vertical),
               )
    rep["node_component"] = comp
    if movement:
        q = Counter()
        examples = []
        for ev in movement.get("events", []):
            if ev["type"] != "astar_failed":
                continue
            a, b = G.nearest(ev["pos"]), G.nearest(ev["goal"])
            if a < 0 or b < 0:
                reason = "start_off_graph" if a < 0 else "goal_off_graph"
            elif comp[a] != comp[b]:
                reason = "different_component"
            else:
                ok, expanded = G.astar(a, b)
                reason = "found_offline (LOS/memory/version differ)" if ok else (
                    "expansion_cap" if expanded >= EXPANSION_CAP else "no_directed_route")
            q[reason] += 1
            if len(examples) < 30:
                examples.append(dict(t=ev["t"], start=ev["pos"], goal=ev["goal"], reason=reason))
        rep["failed_query_replay"] = dict(reasons=dict(q), examples=examples,
                                          note="offline approximation: Nearest without LOS ray, no stuck penalty")
    return rep


def fixture():
    """Synthetic 2-floor building with stairs, a roof island and a gap. NOT Steelmill."""
    b = dict(nx=8, ny=6, cell=3.0, min_x=0.0, min_y=0.0, min_z=-5.0, max_z=20.0, max_layers=5)
    nodes, edges, index = [], [], {}
    def add(ix, iy, layer, z):
        nid = len(nodes)
        nodes.append([nid, round(b["min_x"] + (ix + .5) * 3, 3), round(b["min_y"] + (iy + .5) * 3, 3), z, ix, iy, layer])
        index[(ix, iy, layer)] = nid
    for iy in range(6):                     # scan order of BotNav::ScanColumn: column major, top layer first
        for ix in range(8):
            if 2 <= ix <= 4 and 2 <= iy <= 3:
                add(ix, iy, 0, 4.0)        # upper floor
            if (ix, iy) == (7, 5):
                add(ix, iy, 0, 9.0)        # unreachable roof island
            if ix != 6 or iy < 2:          # a hole in column ix=6 except two cells
                add(ix, iy, 1 if (2 <= ix <= 4 and 2 <= iy <= 3) or (ix, iy) == (7, 5) else 0, 0.0)
    for (ix, iy, layer), a in index.items():
        za = nodes[a][3]
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                if dx == dy == 0:
                    continue
                for l2 in range(5):
                    c = index.get((ix + dx, iy + dy, l2))
                    if c is None:
                        continue
                    zc = nodes[c][3]
                    stairs = {(1, 2), (2, 2)}  # stairs between (1,2) ground and (2,2) upper floor
                    if abs(zc - za) < .1 or ((ix, iy) in stairs and (ix + dx, iy + dy) in stairs):
                        edges.append([a, c, round(math.dist(nodes[a][1:4], nodes[c][1:4]), 4)])
    g = dict(schema=SCHEMA, fixture=True, level="FIXTURE/two_floor_building", source="navgraph.py fixture()",
             build=b, node_columns=NODE_COLUMNS, edge_columns=EDGE_COLUMNS, nodes=nodes, edges=edges,
             counts=dict(nodes=len(nodes), edges=len(edges), truncated_edge_lists=0),
             memory_compat=dict(nx=b["nx"], ny=b["ny"], cell=b["cell"], min_x=b["min_x"], min_y=b["min_y"],
                                count=len(nodes)))
    g["fingerprint"] = fingerprint(g)
    return g


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    v = sub.add_parser("validate"); v.add_argument("graph", type=Path)
    an = sub.add_parser("analyze"); an.add_argument("graph", type=Path)
    an.add_argument("--movement", type=Path); an.add_argument("--json", type=Path)
    f = sub.add_parser("fixture"); f.add_argument("out", type=Path)
    a = ap.parse_args(argv)
    if a.cmd == "fixture":
        a.out.parent.mkdir(parents=True, exist_ok=True)
        a.out.write_text(json.dumps(fixture()) + "\n", encoding="utf-8")
        print(f"fixture written: {a.out}")
        return 0
    g = load(a.graph)
    err, warn = validate(g)
    tag = " [FIXTURE]" if g.get("fixture") else ""
    if a.cmd == "validate" or err:
        print(f"{a.graph.name}{tag}: {'VALID' if not err else 'INVALID'}; {len(err)} errors, {len(warn)} warnings")
        for e in err[:20]:
            print("  error:", e)
        return 0 if not err else 1
    rep = analyze(g, json.loads(a.movement.read_text(encoding="utf-8")) if a.movement else None)
    comp = rep.pop("node_component")
    print(json.dumps({k: v for k, v in rep.items() if k != "failed_query_replay"}, indent=1))
    if "failed_query_replay" in rep:
        print("failed query replay:", rep["failed_query_replay"]["reasons"])
    if a.json:
        rep["node_component"] = comp
        a.json.write_text(json.dumps(rep) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
