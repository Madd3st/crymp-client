"""BotNav graph export (schema wbnav/1, see Docs/Research/SteelmillLab/NAV_EXPORT_CONTRACT.md).

Loads and validates an export, reproduces BotNav's projection and A* offline (without
the physics ray that Nearest() also requires), finds connected components and replays
logged NAV fallback queries to say which failures are projection, budget or topology.
Nothing here can see geometry; "no node" means "the 3 m scan produced no node".
"""
from __future__ import annotations

import gzip
import heapq
import json
import math
from collections import defaultdict
from pathlib import Path

SCHEMA = "wbnav/1"
A_STAR_BUDGET = 6000      # BotNav::FindPath expanded limit
NEAREST_RADIUS = 2        # +-2 columns
NEAREST_MAX_DZ = 2.5


class GraphError(ValueError):
    pass


class NavGraph:
    def __init__(self, data: dict):
        self.data = data
        g = data["grid"]
        self.cell = float(g["cell"]); self.min_x = float(g["min_x"]); self.min_y = float(g["min_y"])
        self.nx = int(g["nx"]); self.ny = int(g["ny"])
        self.nodes = [tuple(n[:3]) for n in data["nodes"]]
        e = data["edges"]
        self.first, self.count, self.to, self.cost = e["first"], e["count"], e["to"], e["cost"]
        self.columns = defaultdict(list)
        for i, (x, y, z) in enumerate(self.nodes):
            self.columns[self.col(x, y)].append(i)

    def col(self, x, y):
        return (int((x - self.min_x) / self.cell), int((y - self.min_y) / self.cell))

    def out(self, i):
        f = self.first[i]
        return range(f, f + self.count[i])

    # --- BotNav::Nearest without the physics visibility ray -------------------------
    def nearest(self, p):
        cx, cy = self.col(p[0], p[1])
        best, score = -1, 1e18
        for dx in range(-NEAREST_RADIUS, NEAREST_RADIUS + 1):
            for dy in range(-NEAREST_RADIUS, NEAREST_RADIUS + 1):
                for i in self.columns.get((cx + dx, cy + dy), ()):
                    x, y, z = self.nodes[i]
                    dz = abs(z - p[2])
                    if dz > NEAREST_MAX_DZ:
                        continue
                    s = (x - p[0]) ** 2 + (y - p[1]) ** 2 + dz * dz * 4
                    if s < score:
                        best, score = i, s
        return best

    def find_path(self, a, b, stuck=None, budget=A_STAR_BUDGET):
        """Returns (status, node_path, expanded). status: ok|start_unprojected|goal_unprojected|budget|unreachable."""
        s, t = self.nearest(a), self.nearest(b)
        if s < 0: return "start_unprojected", [], 0
        if t < 0: return "goal_unprojected", [], 0
        if s == t: return "ok", [s], 0
        tx, ty, tz = self.nodes[t]
        h = lambda i: math.dist(self.nodes[i], (tx, ty, tz))
        g = {s: 0.0}; parent = {}; closed = set(); open_ = [(h(s), s)]; expanded = 0
        while open_ and expanded < budget:
            _, cur = heapq.heappop(open_)
            if cur in closed: continue
            closed.add(cur); expanded += 1
            if cur == t: break
            for e in self.out(cur):
                nxt = self.to[e]
                pen = 1.0 + 0.6 * min(stuck[nxt], 6.0) if stuck else 1.0
                c = g[cur] + self.cost[e] * pen
                if c < g.get(nxt, 1e30):
                    g[nxt] = c; parent[nxt] = cur
                    heapq.heappush(open_, (c + h(nxt), nxt))
        if t not in parent:
            return ("budget" if expanded >= budget else "unreachable"), [], expanded
        path = [t]
        while path[-1] != s: path.append(parent[path[-1]])
        return "ok", path[::-1], expanded

    def components(self, undirected=False):
        """Weakly (undirected=True) or strongly connected components, largest first."""
        n = len(self.nodes)
        if undirected:
            adj = defaultdict(set)
            for i in range(n):
                for e in self.out(i):
                    adj[i].add(self.to[e]); adj[self.to[e]].add(i)
            seen = [-1] * n; comps = []
            for i in range(n):
                if seen[i] >= 0: continue
                stack = [i]; seen[i] = len(comps); comp = []
                while stack:
                    v = stack.pop(); comp.append(v)
                    for w in adj[v]:
                        if seen[w] < 0: seen[w] = len(comps); stack.append(w)
                comps.append(comp)
            return sorted(comps, key=len, reverse=True)
        # iterative Tarjan
        index = [0]; idx = [-1] * n; low = [0] * n; on = [False] * n; st = []; comps = []
        for root in range(n):
            if idx[root] >= 0: continue
            work = [(root, iter(self.out(root)))]
            idx[root] = low[root] = index[0]; index[0] += 1; st.append(root); on[root] = True
            while work:
                v, it = work[-1]
                for e in it:
                    w = self.to[e]
                    if idx[w] < 0:
                        idx[w] = low[w] = index[0]; index[0] += 1; st.append(w); on[w] = True
                        work.append((w, iter(self.out(w)))); break
                    elif on[w]:
                        low[v] = min(low[v], idx[w])
                else:
                    work.pop()
                    if work: low[work[-1][0]] = min(low[work[-1][0]], low[v])
                    if low[v] == idx[v]:
                        comp = []
                        while True:
                            w = st.pop(); on[w] = False; comp.append(w)
                            if w == v: break
                        comps.append(comp)
        return sorted(comps, key=len, reverse=True)

    def one_way_edges(self):
        rev = set()
        for i in range(len(self.nodes)):
            for e in self.out(i): rev.add((i, self.to[e]))
        return sum(1 for a, b in rev if (b, a) not in rev)


def load(path) -> NavGraph:
    p = Path(path)
    raw = p.read_bytes()
    if raw[:2] == b"\x1f\x8b": raw = gzip.decompress(raw)
    data = json.loads(raw)
    validate(data)
    return NavGraph(data)


def validate(d: dict, memory=None):
    if d.get("schema") != SCHEMA:
        raise GraphError(f"schema {d.get('schema')!r} != {SCHEMA}")
    for k in ("level", "grid", "nodes", "edges", "build"):
        if k not in d: raise GraphError(f"missing {k}")
    n = len(d["nodes"]); e = d["edges"]
    for k in ("first", "count", "to", "cost"):
        if k not in e: raise GraphError(f"edges.{k} missing")
    if len(e["first"]) != n or len(e["count"]) != n:
        raise GraphError("edges.first/count must have one entry per node")
    if len(e["to"]) != len(e["cost"]):
        raise GraphError("edges.to/cost length mismatch")
    total = 0
    for i in range(n):
        if e["first"][i] != total: raise GraphError(f"node {i}: CSR first={e['first'][i]} expected {total}")
        total += e["count"][i]
    if total != len(e["to"]): raise GraphError("CSR does not cover edges.to")
    if any(not (0 <= t < n) for t in e["to"]): raise GraphError("edge target out of range")
    if "counts" in d and (d["counts"].get("nodes") != n or d["counts"].get("edges") != total):
        raise GraphError("counts block disagrees with arrays")
    if memory is not None:
        fp = memory.fingerprint(); g = d["grid"]
        mine = dict(nx=g["nx"], ny=g["ny"], cell=g["cell"], min_x=g["min_x"], min_y=g["min_y"], count=n)
        if any(abs(float(fp[k]) - float(mine[k])) > 1e-4 for k in fp):
            raise GraphError(f"memory fingerprint {fp} does not match export {mine}")
    return True


def replay_fallbacks(graph: NavGraph, run, stuck=None, limit=None):
    """Re-run logged failed queries offline and classify them."""
    from collections import Counter
    c = Counter(); rows = []
    for k, t, d in run.events:
        if k != "nav_fallback": continue
        status, path, expanded = graph.find_path(d["start"], d["goal"], stuck)
        # Offline "ok" means the in-game failure needs the ray check / memory / budget to explain it.
        c[status if status != "ok" else "ok_offline_ray_or_memory_suspect"] += 1
        rows.append(dict(t=t, entity=d.get("entity"), start=d["start"], goal=d["goal"], offline=status, expanded=expanded))
        if limit and len(rows) >= limit: break
    return dict(counts=dict(c)), rows
