"""BotNav graph export reader, schema `warsbots/botnav-graph/1`.

Contract: Docs/Research/SteelmillLab/BOTNAV_EXPORT_CONTRACT.md (branch warsbots/research/steelmill-lab).
Codex's `wb_nav_export` (e4eb64ee) writes the older Triage schema `wbnav/1` (CSR edges, nodes
[x, y, z, iy]). `read_json` converts it losslessly into botnav-graph/1: ix = trunc((x-min_x)/cell)
as in BotNav, layer = position in the column by node id (scan order, top first), fingerprint
recomputed. `check()` on a raw wbnav/1 dict still reports it, so nothing slips through unconverted.

What this module does offline, without the game:
  * validates an export (contract section 4, plus the section 5 fingerprint and the
    `memory_compat` tuple that BotNav::Load compares against a .mem file);
  * reproduces BotNav::Nearest (minus its physics ray) and BotNav::FindPath exactly as written in
    Code/CryGame/Bots/BotNav.cpp (truncating column index, strict `<` tie order, 6000-expansion
    budget, success = the goal got a parent);
  * connected components, one-way edges, dead ends, vertical edge classes;
  * replays logged `NAV fallback` queries and says whether each one is projection, budget or topology.

Nothing here sees geometry: a missing node means "the 3 m column scan produced no floor there",
a missing edge means "BotNav's Walkable() said no", not that a human cannot pass.
"""
from __future__ import annotations

import gzip
import hashlib
import heapq
import json
import math
from collections import Counter, defaultdict
from pathlib import Path

SCHEMA = "warsbots/botnav-graph/1"
LEGACY_SCHEMAS = {"wbnav/1"}
NODE_COLUMNS = ["id", "x", "y", "z", "ix", "iy", "layer"]
EDGE_COLUMNS = ["from", "to", "cost"]

# Contract section 6 hard caps (the exporter refuses above these; so do we).
MAX_NODES = 2_000_000
MAX_EDGES = 20_000_000
MAX_BYTES = 64 * 1024 * 1024

# Defaults equal BotNav.cpp; `build.params` in an export overrides them.
A_STAR_BUDGET = 6000
NEAREST_RADIUS = 2
NEAREST_MAX_DZ = 2.5
MAX_LAYERS = 5
XY_TOLERANCE = 0.01   # node x/y vs column centre (downward ray, so it should be exact)
COST_TOLERANCE = 0.05  # stored Edge::cost vs 3D distance
MAX_REPORTED = 50


class GraphError(ValueError):
    """Export is unusable: wrong schema, broken structure, or memory from a different graph."""

    def __init__(self, message, errors=None):
        super().__init__(message)
        self.errors = errors or [message]


# --------------------------------------------------------------------------------------------
# Fingerprint (contract section 5)
# --------------------------------------------------------------------------------------------

def fingerprint(d: dict) -> str:
    h = hashlib.sha256()
    for n in d["nodes"]:
        h.update(("%d %.3f %.3f %.3f %d %d %d\n" % tuple(n)).encode())
    for a, b, cost in d["edges"]:
        h.update(("%d %d %.4f\n" % (a, b, cost)).encode())
    return h.hexdigest()


# --------------------------------------------------------------------------------------------
# Validation (contract section 4)
# --------------------------------------------------------------------------------------------

def check(d: dict, memory=None):
    """Return (errors, warnings). Never raises on bad content."""
    err, warn = [], []

    def e(msg):
        err.append(msg)
        return len(err) > MAX_REPORTED

    schema = d.get("schema")
    if schema in LEGACY_SCHEMAS:
        try:
            upgraded = from_wbnav1(d)
        except (GraphError, KeyError, TypeError, IndexError) as ex:
            return [f"wbnav/1 export could not be converted: {ex}"], warn
        d.clear(); d.update(upgraded)
        schema = SCHEMA
    if schema != SCHEMA:
        return [f"schema {schema!r} != {SCHEMA!r}"], warn
    b = d.get("build")
    if not isinstance(b, dict):
        return ["build block missing"], warn
    for k in ("nx", "ny", "cell", "min_x", "min_y", "min_z", "max_z", "max_layers"):
        if k not in b:
            err.append(f"build.{k} missing")
    if d.get("node_columns") != NODE_COLUMNS:
        err.append(f"node_columns must be {NODE_COLUMNS}")
    if d.get("edge_columns") != EDGE_COLUMNS:
        err.append(f"edge_columns must be {EDGE_COLUMNS}")
    nodes, edges = d.get("nodes"), d.get("edges")
    if not isinstance(nodes, list) or not isinstance(edges, list):
        err.append("nodes and edges must be arrays")
    if err:
        return err, warn
    if len(nodes) > MAX_NODES or len(edges) > MAX_EDGES:
        return [f"graph exceeds size caps ({len(nodes)} nodes, {len(edges)} edges)"], warn
    if not d.get("level"):
        warn.append("level missing")

    nx, ny, cell = int(b["nx"]), int(b["ny"]), float(b["cell"])
    max_layers = int(b["max_layers"])
    n = len(nodes)
    seen_slot = {}
    for i, row in enumerate(nodes):
        if not isinstance(row, list) or len(row) != 7:
            if e(f"node {i}: expected 7 fields"): break
            continue
        nid, x, y, z, ix, iy, layer = row
        if nid != i:
            if e(f"node {i}: id {nid} != index (ids must be m_nodes order = .mem record order)"): break
        if not all(isinstance(v, (int, float)) and math.isfinite(v) for v in (x, y, z)):
            if e(f"node {i}: non-finite position"): break
            continue
        if not all(isinstance(v, int) for v in (ix, iy, layer)):
            if e(f"node {i}: ix/iy/layer must be integers"): break
            continue
        if not (0 <= ix < nx and 0 <= iy < ny):
            if e(f"node {i}: column ({ix},{iy}) outside {nx}x{ny}"): break
        cx, cy = b["min_x"] + (ix + 0.5) * cell, b["min_y"] + (iy + 0.5) * cell
        if abs(x - cx) > XY_TOLERANCE or abs(y - cy) > XY_TOLERANCE:
            warn.append(f"node {i}: ({x:.3f},{y:.3f}) not at column centre ({cx:.3f},{cy:.3f})")
        if not (b["min_z"] - 1 <= z <= b["max_z"] + 1):
            if e(f"node {i}: z={z} outside build z {b['min_z']}..{b['max_z']}"): break
        if not (0 <= layer < max_layers):
            if e(f"node {i}: layer {layer} outside 0..{max_layers - 1}"): break
        slot = (ix, iy, layer)
        if slot in seen_slot:
            if e(f"node {i}: duplicate (ix,iy,layer) {slot}, also node {seen_slot[slot]}"): break
        seen_slot[slot] = i
    if err:
        return err, warn[:200]

    # Layers inside a column must be dense 0..k-1 and go downwards (scan order top to bottom).
    cols = defaultdict(list)
    for nid, x, y, z, ix, iy, layer in nodes:
        cols[(ix, iy)].append((layer, z, nid))
    for key, items in cols.items():
        items.sort()
        if [l for l, _, _ in items] != list(range(len(items))):
            if e(f"column {key}: layers {[l for l, _, _ in items]} are not 0..{len(items) - 1}"): break
        zs = [z for _, z, _ in items]
        if any(a <= c for a, c in zip(zs, zs[1:])):
            warn.append(f"column {key}: z does not decrease with layer {zs}")

    last_from, pairs = -1, set()
    for j, row in enumerate(edges):
        if not isinstance(row, list) or len(row) != 3:
            if e(f"edge {j}: expected 3 fields"): break
            continue
        a, c, cost = row
        if not (isinstance(a, int) and isinstance(c, int) and 0 <= a < n and 0 <= c < n):
            if e(f"edge {j}: endpoint out of range"): break
            continue
        if a < last_from:
            if e(f"edge {j}: from={a} after from={last_from}; edges must be m_edges order (grouped by node)"): break
        last_from = a
        if a == c:
            if e(f"edge {j}: self loop on {a}"): break
            continue
        if (a, c) in pairs:
            warn.append(f"edge {j}: duplicate {a}->{c}")
        pairs.add((a, c))
        if not (isinstance(cost, (int, float)) and math.isfinite(cost) and cost > 0):
            if e(f"edge {j}: bad cost {cost!r}"): break
            continue
        na, nc = nodes[a], nodes[c]
        cheb = max(abs(na[4] - nc[4]), abs(na[5] - nc[5]))
        if cheb != 1:
            if e(f"edge {j}: {a}->{c} columns at Chebyshev distance {cheb}, BotNav links only the 8 neighbours"): break
        dist = math.dist(na[1:4], nc[1:4])
        if abs(dist - cost) > COST_TOLERANCE:
            warn.append(f"edge {j}: cost {cost:.4f} != 3D distance {dist:.4f}")
    if err:
        return err, warn[:200]

    counts = d.get("counts")
    if counts is None:
        warn.append("counts block missing")
    else:
        if counts.get("nodes") != n or counts.get("edges") != len(edges):
            err.append(f"counts {counts} disagree with arrays ({n} nodes, {len(edges)} edges)")
        if counts.get("truncated_edge_lists", 0):
            warn.append(f"{counts['truncated_edge_lists']} nodes had >65535 edges; BotNav dropped the rest")

    fp = d.get("fingerprint")
    if fp is None:
        warn.append("fingerprint missing; memory linking relies on counts only")
    elif not (isinstance(fp, str) and len(fp) == 64):
        err.append(f"fingerprint must be 64 hex chars, got {fp!r}")
    elif fp != fingerprint(d):
        err.append("fingerprint does not match nodes/edges (export changed or wrong formatting)")

    mc = d.get("memory_compat")
    if mc is None:
        warn.append("memory_compat missing")
    else:
        mine = dict(nx=nx, ny=ny, cell=cell, min_x=b["min_x"], min_y=b["min_y"], count=n)
        bad = [k for k in mine if k not in mc or abs(float(mc[k]) - float(mine[k])) > 1e-4]
        if bad:
            err.append(f"memory_compat disagrees with build/nodes on {bad}")

    if memory is not None:
        fp_mem = memory.fingerprint()
        ref = mc or dict(nx=nx, ny=ny, cell=cell, min_x=b["min_x"], min_y=b["min_y"], count=n)
        bad = [k for k in fp_mem if abs(float(fp_mem[k]) - float(ref.get(k, math.nan))) > 1e-4
               or math.isnan(float(ref.get(k, math.nan)))]
        if bad:
            err.append(f".mem {fp_mem} is not from this graph (differs on {bad}); BotNav::Load would reject it")

    if d.get("fixture"):
        warn.append("fixture=true: synthetic graph, not a game export")
    if d.get("converted_from"):
        warn.append(f"converted from {d['converted_from']}; ask for a native {SCHEMA} export eventually")
    return err, warn[:200]


def validate(d: dict, memory=None) -> dict:
    """Raise GraphError on errors; otherwise return {'ok': True, 'warnings': [...], 'fixture': bool}."""
    err, warn = check(d, memory)
    if err:
        raise GraphError(f"{len(err)} error(s): {err[0]}", err)
    return dict(ok=True, warnings=warn, fixture=bool(d.get("fixture")))


# --------------------------------------------------------------------------------------------
# Graph model
# --------------------------------------------------------------------------------------------

class NavGraph:
    """In-memory mirror of BotNav after Phase::Ready.

    Public surface used by render.py / steelmill_triage.py: `nodes` (x, y, z tuples),
    `out(i)` (edge index range), `to`, `cost`, `components()`, `one_way_edges()`, `find_path()`.
    """

    def __init__(self, data: dict):
        self.data = data
        b = data["build"]
        p = b.get("params") or {}
        self.level = data.get("level")
        self.fixture = bool(data.get("fixture"))
        self.cell = float(b["cell"]); self.min_x = float(b["min_x"]); self.min_y = float(b["min_y"])
        self.nx = int(b["nx"]); self.ny = int(b["ny"])
        self.budget = int(p.get("astar_expansion_cap", A_STAR_BUDGET))
        self.radius = int(p.get("nearest_radius_columns", NEAREST_RADIUS))
        self.max_dz = float(p.get("nearest_max_dz", NEAREST_MAX_DZ))
        rows = data["nodes"]
        self.nodes = [(float(r[1]), float(r[2]), float(r[3])) for r in rows]
        self.ix = [r[4] for r in rows]; self.iy = [r[5] for r in rows]; self.layer = [r[6] for r in rows]
        n = len(rows)
        # CSR exactly like Node::firstEdge/edgeCount (edges arrive in m_edges order).
        self.count = [0] * n
        self.to, self.cost = [], []
        for a, c, cost in data["edges"]:
            self.count[a] += 1; self.to.append(c); self.cost.append(float(cost))
        self.first, acc = [0] * n, 0
        for i in range(n):
            self.first[i] = acc; acc += self.count[i]
        # m_columns: column vector holds node ids in scan order, i.e. by layer.
        cols = defaultdict(list)
        for i in range(n):
            cols[(self.ix[i], self.iy[i])].append((self.layer[i], i))
        self.columns = {k: [i for _, i in sorted(v)] for k, v in cols.items()}

    # --- geometry helpers --------------------------------------------------------------------
    def col(self, x, y):
        # C++ int() truncates toward zero; keep that, it matters for points just west/south of min.
        return int((x - self.min_x) / self.cell), int((y - self.min_y) / self.cell)

    def out(self, i):
        f = self.first[i]
        return range(f, f + self.count[i])

    # --- BotNav::Nearest without the physics visibility ray ----------------------------------
    def nearest(self, p):
        """Can only be MORE permissive than the game: the game also needs Clear() at 0.9 m."""
        cx, cy = self.col(p[0], p[1])
        best, score = -1, 1e9
        for dx in range(-self.radius, self.radius + 1):
            for dy in range(-self.radius, self.radius + 1):
                for i in self.columns.get((cx + dx, cy + dy), ()):
                    x, y, z = self.nodes[i]
                    dz = abs(z - p[2])
                    if dz > self.max_dz:
                        continue
                    s = (x - p[0]) ** 2 + (y - p[1]) ** 2 + dz * dz * 4.0
                    if s < score:
                        best, score = i, s
        return best

    # --- BotNav::FindPath ---------------------------------------------------------------------
    def find_path(self, a, b, stuck=None, budget=None):
        """(status, node_path, expanded); status: ok|same_node|start_unprojected|goal_unprojected|budget|unreachable."""
        budget = self.budget if budget is None else budget
        s, t = self.nearest(a), self.nearest(b)
        if s < 0: return "start_unprojected", [], 0
        if t < 0: return "goal_unprojected", [], 0
        if s == t: return "same_node", [s], 0
        goal = self.nodes[t]
        g = {s: 0.0}; parent = {}; closed = set()
        heap = [(math.dist(self.nodes[s], goal), s)]
        expanded = 0
        while heap and expanded < budget:
            _, cur = heapq.heappop(heap)
            if cur in closed: continue
            closed.add(cur); expanded += 1
            if cur == t: break
            gc = g[cur]
            for e in self.out(cur):
                nxt = self.to[e]
                pen = 1.0 + 0.6 * min(stuck[nxt], 6.0) if stuck else 1.0
                c = gc + self.cost[e] * pen
                if c < g.get(nxt, 1e30):
                    g[nxt] = c; parent[nxt] = cur
                    heapq.heappush(heap, (c + math.dist(self.nodes[nxt], goal), nxt))
        # BotNav succeeds as soon as the goal has a parent, even if the budget ran out first.
        if t not in parent:
            return ("budget" if expanded >= budget else "unreachable"), [], expanded
        path = [t]
        while path[-1] != s: path.append(parent[path[-1]])
        return "ok", path[::-1], expanded

    # --- topology -----------------------------------------------------------------------------
    def components(self, undirected=False):
        """Weakly (undirected=True) or strongly connected components, largest first."""
        n = len(self.nodes)
        if undirected:
            adj = defaultdict(set)
            for i in range(n):
                for e in self.out(i):
                    adj[i].add(self.to[e]); adj[self.to[e]].add(i)
            seen = [False] * n; comps = []
            for i in range(n):
                if seen[i]: continue
                stack = [i]; seen[i] = True; comp = []
                while stack:
                    v = stack.pop(); comp.append(v)
                    for w in adj[v]:
                        if not seen[w]: seen[w] = True; stack.append(w)
                comps.append(comp)
            return sorted(comps, key=len, reverse=True)
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
        pairs = {(i, self.to[e]) for i in range(len(self.nodes)) for e in self.out(i)}
        return sum(1 for a, b in pairs if (b, a) not in pairs)

    def vertical_classes(self):
        """Edges by rise b.z - a.z, in the bands Walkable() distinguishes (rise<=1.9, drop>=-3.2)."""
        c = Counter()
        for i in range(len(self.nodes)):
            for e in self.out(i):
                dz = self.nodes[self.to[e]][2] - self.nodes[i][2]
                c["flat(|dz|<0.3)" if abs(dz) < 0.3 else
                  "step_up(<=0.75)" if 0 < dz <= 0.75 else
                  "climb_up(>0.75)" if dz > 0.75 else
                  "step_down(>=-0.75)" if dz >= -0.75 else
                  "drop(<-0.75)"] += 1
        return dict(c)

    def scc_profile(self):
        """SCC condensation facts: sinks (enter, no exit), no-entry islands, spawn classes."""
        scc = self.components(); sid = [0] * len(self.nodes)
        for k, c in enumerate(scc):
            for v in c: sid[v] = k
        leave = Counter(); enter = Counter(); between = Counter()
        for i in range(len(self.nodes)):
            for e in self.out(i):
                j = self.to[e]
                if sid[i] != sid[j]:
                    leave[sid[i]] += 1; enter[sid[j]] += 1
                    if {sid[i], sid[j]} == {0, 1}: between[(sid[i], sid[j])] += 1
        def kind(k):
            if k in (0, 1): return f"main{k}"
            if leave[k] == 0 and enter[k] > 0: return "sink"
            if enter[k] == 0 and leave[k] > 0: return "no_entry"
            if leave[k] == 0 and enter[k] == 0: return "isolated"
            return "other"
        sinks = [k for k in range(len(scc)) if kind(k) == "sink"]
        prof = dict(scc_count=len(scc), main_sizes=[len(c) for c in scc[:2]],
                    main0_main1_edges=sum(between.values()),
                    sink_sccs=len(sinks), nodes_in_sinks=sum(len(scc[k]) for k in sinks),
                    largest_sinks=sorted((len(scc[k]) for k in sinks), reverse=True)[:8])
        spawns = self.data.get("spawns") or []
        if spawns:
            rows = []
            for r in spawns:
                n = self.nearest(r[1:4]); k = sid[n] if n >= 0 else None
                rows.append(dict(spawn=r[0], node=n, kind=kind(k) if k is not None else "unprojected",
                                 scc_size=len(scc[k]) if k is not None else 0))
            prof["spawns"] = dict(Counter(x["kind"] for x in rows))
            prof["spawns_outside_main"] = [x for x in rows if not x["kind"].startswith("main")]
        self._scc = (sid, kind)
        return prof

    def classify_native_routes(self, lines):
        """Native `reason=no_route from=A to=B` lines → start/goal component kinds."""
        import re
        if not hasattr(self, "_scc"): self.scc_profile()
        sid, kind = self._scc
        st = Counter(); gl = Counter(); pr = Counter(); reasons = Counter()
        for line in lines:
            m = re.search(r"NAV fallback .*reason=(\w+)(?: from=(-?\d+) to=(-?\d+))?", line)
            if not m: continue
            reasons[m.group(1)] += 1
            if m.group(1) != "no_route" or m.group(2) is None: continue
            a, b = int(m.group(2)), int(m.group(3))
            if not (0 <= a < len(sid) and 0 <= b < len(sid)): continue
            ka, kb = kind(sid[a]), kind(sid[b]); st[ka] += 1; gl[kb] += 1; pr[f"{ka}->{kb}"] += 1
        return dict(native_reasons=dict(reasons), no_route_start=dict(st), no_route_goal=dict(gl),
                    top_pairs=dict(pr.most_common(6)))

    def summary(self):
        strong = self.components(); weak = self.components(undirected=True)
        indeg = Counter(self.to)
        n = len(self.nodes)
        return dict(
            level=self.level, fixture=self.fixture, nodes=n, edges=len(self.to),
            columns=len(self.columns), max_layer=max(self.layer) if n else 0,
            one_way_edges=self.one_way_edges(),
            dead_ends=sum(1 for i in range(n) if self.count[i] == 0 and indeg[i] > 0),
            sources_only=sum(1 for i in range(n) if self.count[i] > 0 and indeg[i] == 0),
            isolated_nodes=sum(1 for c in weak if len(c) == 1),
            strong_components=len(strong), largest_strong=len(strong[0]) if strong else 0,
            weak_components=len(weak), largest_weak=len(weak[0]) if weak else 0,
            vertical=self.vertical_classes(),
            fingerprint=self.data.get("fingerprint"),
        )


def from_wbnav1(d: dict) -> dict:
    """Lossless upgrade of a Codex wbnav/1 export to warsbots/botnav-graph/1."""
    g = d["grid"]; e = d["edges"]; p = d.get("params") or {}
    cell, mx, my = float(g["cell"]), float(g["min_x"]), float(g["min_y"])
    rank = defaultdict(int); nodes = []
    for i, row in enumerate(d["nodes"]):
        x, y, z = row[0], row[1], row[2]
        ix = int((x - mx) / cell); iy = int((y - my) / cell)
        if len(row) > 3 and row[3] != iy:
            raise GraphError(f"wbnav/1 node {i}: stored column {row[3]} != iy {iy} from position")
        nodes.append([i, x, y, z, ix, iy, rank[(ix, iy)]]); rank[(ix, iy)] += 1
    edges = []
    for a in range(len(nodes)):
        f, c = e["first"][a], e["count"][a]
        for k in range(f, f + c):
            edges.append([a, e["to"][k], e["cost"][k]])
    out = dict(
        schema=SCHEMA, fixture=False, level=d.get("level"), source="wbnav/1 via navgraph.from_wbnav1",
        build_version=(d.get("build") or {}).get("describe"),
        build=dict(nx=g["nx"], ny=g["ny"], cell=cell, min_x=mx, min_y=my,
                   min_z=g.get("min_z"), max_z=g.get("max_z"), max_layers=int(p.get("layers", MAX_LAYERS)),
                   water_level=d.get("water_level"),
                   params=dict(astar_expansion_cap=p.get("astar_budget", A_STAR_BUDGET),
                               nearest_radius_columns=p.get("nearest_radius_columns", NEAREST_RADIUS),
                               nearest_max_dz=p.get("nearest_max_dz", NEAREST_MAX_DZ))),
        counts=dict(nodes=len(nodes), edges=len(edges), truncated_edge_lists=0),
        node_columns=list(NODE_COLUMNS), nodes=nodes, edge_columns=list(EDGE_COLUMNS), edges=edges,
        memory_compat=dict(nx=g["nx"], ny=g["ny"], cell=cell, min_x=mx, min_y=my, count=len(nodes)),
        converted_from="wbnav/1",
    )
    if "spawns" in d:
        out["spawns"] = d["spawns"]
    if "counts" in d and (d["counts"].get("nodes") != len(nodes) or d["counts"].get("edges") != len(edges)):
        raise GraphError(f"wbnav/1 counts {d['counts']} disagree with arrays ({len(nodes)}, {len(edges)})")
    out["fingerprint"] = fingerprint(out)
    return out


def read_json(path) -> dict:
    p = Path(path)
    if p.stat().st_size > MAX_BYTES:
        raise GraphError(f"{p.name}: {p.stat().st_size} bytes exceeds the 64 MiB cap")
    raw = p.read_bytes()
    if raw[:2] == b"\x1f\x8b":
        raw = gzip.decompress(raw)
        if len(raw) > MAX_BYTES:
            raise GraphError(f"{p.name}: decompressed size exceeds the 64 MiB cap")
    try:
        d = json.loads(raw.decode("utf-8-sig"))
    except (UnicodeDecodeError, json.JSONDecodeError) as ex:
        raise GraphError(f"{p.name}: not valid UTF-8 JSON ({ex})")
    if d.get("schema") in LEGACY_SCHEMAS:
        d = from_wbnav1(d)
    return d


def load(path, memory=None) -> NavGraph:
    data = read_json(path)
    report = validate(data, memory)
    g = NavGraph(data)
    g.warnings = report["warnings"]
    return g


def replay_fallbacks(graph: NavGraph, run, stuck=None, limit=None):
    """Re-run logged failed queries offline and classify them.

    offline ok/same_node for an in-game failure → the physics ray in Nearest, the memory penalty
    or a stale graph explains it, not topology. Use stuck=memory.stuck to include the penalty.
    """
    c = Counter(); rows = []
    for k, t, d in run.events:
        if k != "nav_fallback": continue
        status, path, expanded = graph.find_path(d["start"], d["goal"], stuck)
        label = "ok_offline_ray_or_memory_suspect" if status in ("ok", "same_node") else status
        c[label] += 1
        rows.append(dict(t=t, entity=d.get("entity"), start=d["start"], goal=d["goal"],
                         offline=status, expanded=expanded, hops=max(len(path) - 1, 0)))
        if limit and len(rows) >= limit: break
    c["total"] = sum(v for k, v in c.items() if k != "total")
    return dict(counts=dict(c)), rows
