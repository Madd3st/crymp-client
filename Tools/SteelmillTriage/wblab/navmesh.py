"""Recast-style walkability model built offline from exported collision geometry (C-019, D-006).

Not a navmesh generator for the game (Codex links real Recast/Detour there). This is the *evaluation* model:
same steps as Recast (rasterise triangles into a 2.5D span heightfield, mark walkable tops by slope, require
agent clearance, connect neighbours within the climb height, erode by the agent radius), then the metrics the
BotNav graph is judged by: strongly connected components, sinks, spawns outside the main part, items, and how
much of the owner's MOVE TRACE route the model can reproduce.

Input: OBJ (`v x y z`, `f a b c ...`), Z-up (CryEngine world coordinates) unless `up='y'`.
"""
import math
from collections import deque

import numpy as np
from scipy.sparse import csr_matrix
from scipy.sparse.csgraph import connected_components

DEFAULTS = dict(
    cs=0.3,           # cell size, m
    agent_h=1.8,      # standing clearance, m
    agent_r=0.4,      # capsule radius, m
    climb=0.5,        # max step up/down walked without jumping, m
    max_drop=4.0,     # one-way drop the agent survives / takes deliberately, m
    max_slope=45.0,   # degrees
    merge_gap=0.25,   # vertical gap below which samples in a column belong to one solid span, m
)


# ----------------------------------------------------------------------------------------------- geometry
def load_obj(path, up='z'):
    vs, fs = [], []
    import gzip
    opener = gzip.open if str(path).endswith('.gz') else open
    with opener(path, 'rt', encoding='utf-8', errors='replace') as f:
        for line in f:
            if line.startswith('v '):
                p = line.split()
                vs.append((float(p[1]), float(p[2]), float(p[3])))
            elif line.startswith('f '):
                idx = [int(t.split('/')[0]) for t in line.split()[1:]]
                idx = [i - 1 if i > 0 else len(vs) + i for i in idx]
                for k in range(1, len(idx) - 1):          # fan-triangulate polygons
                    fs.append((idx[0], idx[k], idx[k + 1]))
    v = np.asarray(vs, dtype=np.float64).reshape(-1, 3)
    if up == 'y':
        v = v[:, [0, 2, 1]] * np.array([1, -1, 1])
    return v, np.asarray(fs, dtype=np.int64).reshape(-1, 3)


def _samples(v, f, spacing, cos_slope, bounds=None, zmax=None, zmin=None):
    """Barycentric point samples of every triangle; returns xyz (n,3) float32 and walkable flag per sample."""
    a, b, c = v[f[:, 0]], v[f[:, 1]], v[f[:, 2]]
    if bounds is not None:
        (x0, y0), (x1, y1) = bounds
        lo = np.minimum(np.minimum(a, b), c); hi = np.maximum(np.maximum(a, b), c)
        keep = (hi[:, 0] >= x0) & (lo[:, 0] <= x1) & (hi[:, 1] >= y0) & (lo[:, 1] <= y1)
        a, b, c = a[keep], b[keep], c[keep]
    if zmax is not None:                                       # drop level ceilings / sky colliders
        keep = np.minimum(np.minimum(a[:, 2], b[:, 2]), c[:, 2]) <= zmax
        a, b, c = a[keep], b[keep], c[keep]
    if zmin is not None:                                       # drop sea bed / under-water terrain (players swim there)
        keep = np.maximum(np.maximum(a[:, 2], b[:, 2]), c[:, 2]) >= zmin
        a, b, c = a[keep], b[keep], c[keep]
    # split huge triangles (level planes, terrain skirts) and drop pieces outside the bounds before sampling
    for _ in range(12):
        e = np.maximum(np.maximum(np.linalg.norm(b - a, axis=1), np.linalg.norm(c - b, axis=1)), np.linalg.norm(a - c, axis=1))
        big = e > 24.0
        if not big.any():
            break
        A, B, C = a[big], b[big], c[big]
        ab, bc, ca = (A + B) / 2, (B + C) / 2, (C + A) / 2
        na = np.concatenate([A, ab, ca, ab]); nb = np.concatenate([ab, B, bc, bc]); nc = np.concatenate([ca, bc, C, ca])
        if bounds is not None:
            (x0, y0), (x1, y1) = bounds
            lo = np.minimum(np.minimum(na, nb), nc); hi = np.maximum(np.maximum(na, nb), nc)
            k = (hi[:, 0] >= x0) & (lo[:, 0] <= x1) & (hi[:, 1] >= y0) & (lo[:, 1] <= y1)
            na, nb, nc = na[k], nb[k], nc[k]
        a = np.concatenate([a[~big], na]); b = np.concatenate([b[~big], nb]); c = np.concatenate([c[~big], nc])
    n = np.cross(b - a, c - a)
    ln = np.linalg.norm(n, axis=1)
    ok = ln > 1e-9
    a, b, c, n, ln = a[ok], b[ok], c[ok], n[ok], ln[ok]
    walk = (n[:, 2] / ln) >= cos_slope                       # upward-facing and not too steep
    edge = np.maximum(np.maximum(np.linalg.norm(b - a, axis=1), np.linalg.norm(c - b, axis=1)),
                      np.linalg.norm(a - c, axis=1))
    steps = np.maximum(1, np.ceil(edge / spacing)).astype(np.int64)
    pts, flags = [], []
    for k in np.unique(steps):
        sel = steps == k
        i, j = np.meshgrid(np.arange(k + 1), np.arange(k + 1), indexing='ij')
        m = (i + j) <= k
        u = (i[m] / k)[None, :, None]; w = (j[m] / k)[None, :, None]
        A, B, C = a[sel][:, None, :], b[sel][:, None, :], c[sel][:, None, :]
        p = A + u * (B - A) + w * (C - A)
        pts.append(p.reshape(-1, 3).astype(np.float32))
        flags.append(np.repeat(walk[sel], p.shape[1]))
    if not pts:
        return np.zeros((0, 3), np.float32), np.zeros(0, bool)
    return np.concatenate(pts), np.concatenate(flags)


# ----------------------------------------------------------------------------------------------- model
class WalkModel:
    """Spans = solid vertical intervals per (ix, iy) column; open spans = walkable tops with clearance."""

    def __init__(self, v, f, bounds=None, zmax=None, zmin=None, **kw):
        self.p = dict(DEFAULTS, **kw)
        cs = self.p['cs']
        pts, wk = _samples(v, f, cs * 0.7, math.cos(math.radians(self.p['max_slope'])), bounds, zmax, zmin)
        if bounds is None:
            lo, hi = pts[:, :2].min(0), pts[:, :2].max(0)
            bounds = ((float(lo[0]), float(lo[1])), (float(hi[0]), float(hi[1])))
        (x0, y0), (x1, y1) = bounds
        self.origin = np.array([x0, y0]); self.nx = int((x1 - x0) / cs) + 1; self.ny = int((y1 - y0) / cs) + 1
        ix = np.floor((pts[:, 0] - x0) / cs).astype(np.int64); iy = np.floor((pts[:, 1] - y0) / cs).astype(np.int64)
        inb = (ix >= 0) & (ix < self.nx) & (iy >= 0) & (iy < self.ny)
        col = (iy * self.nx + ix)[inb]; z = pts[inb, 2]; wk = wk[inb]
        order = np.lexsort((z, col)); col, z, wk = col[order], z[order], wk[order]
        brk = np.ones(len(z), bool)
        brk[1:] = (col[1:] != col[:-1]) | ((z[1:] - z[:-1]) > self.p['merge_gap'])
        starts = np.flatnonzero(brk)
        self.col = col[starts]
        self.zmin = z[starts]
        self.zmax = np.maximum.reduceat(z, starts)
        # walkable if a walkable sample lies within one merge gap of the span top (Recast: top surface flags win)
        topwk = np.where(wk, z, -np.inf)
        self.topwalk = np.maximum.reduceat(topwk, starts) >= self.zmax - self.p['merge_gap']
        nxt = np.full(len(starts), np.inf)
        same = self.col[1:] == self.col[:-1]
        nxt[:-1][same] = self.zmin[1:][same]
        self.clear = nxt - self.zmax
        self.open = self.topwalk & (self.clear >= self.p['agent_h'])
        # column -> span range
        self.cstart = np.searchsorted(self.col, np.arange(self.nx * self.ny))
        self.cend = np.searchsorted(self.col, np.arange(self.nx * self.ny), side='right')
        self._neighbours()
        self._erode()
        self._graph()

    # -- neighbour relation between open spans: walk (|dz| <= climb) both ways, drop one way
    def _neighbours(self):
        nspan = len(self.col)
        cx = self.col % self.nx; cy = self.col // self.nx
        self.walk_nb = []   # per direction: index of walkable neighbour span or -1
        self.drops = []     # (src, dst) one-way drops
        top = self.zmax; clear_top = self.zmax + self.clear
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            ncx, ncy = cx + dx, cy + dy
            valid = self.open & (ncx >= 0) & (ncx < self.nx) & (ncy >= 0) & (ncy < self.ny)
            ncol = np.where(valid, ncy * self.nx + ncx, 0)
            s0 = np.where(valid, self.cstart[ncol], 0); s1 = np.where(valid, self.cend[ncol], 0)
            best = np.full(nspan, -1); bestdz = np.full(nspan, np.inf)
            maxk = int((s1 - s0).max()) if nspan else 0
            src_all = np.arange(nspan)
            for k in range(maxk):
                j = s0 + k; ok = valid & (j < s1)
                jj = np.where(ok, j, 0)
                ok &= self.open[jj]
                dz = top[jj] - top
                # the opening shared by both columns must fit the agent
                gap = np.minimum(clear_top, clear_top[jj]) - np.maximum(top, top[jj])
                fits = ok & (gap >= self.p['agent_h'])
                w = fits & (np.abs(dz) <= self.p['climb']) & (np.abs(dz) < bestdz)
                best[w] = jj[w]; bestdz[w] = np.abs(dz[w])
                d = fits & (dz < -self.p['climb']) & (dz >= -self.p['max_drop'])
                if d.any():
                    self.drops.append(np.stack([src_all[d], jj[d]], 1))
            self.walk_nb.append(best)
        self.drops = np.concatenate(self.drops) if self.drops else np.zeros((0, 2), np.int64)

    def _erode(self):
        """Remove open spans closer than the agent radius to a wall/edge (Recast walkable-area erosion)."""
        r = int(math.ceil(self.p['agent_r'] / self.p['cs']))
        alive = self.open.copy()
        for _ in range(r):
            full = alive.copy()
            for nb in self.walk_nb:
                full &= (nb >= 0) & alive[np.maximum(nb, 0)]
            alive = full
        self.alive = alive

    def _graph(self):
        n = len(self.col)
        src, dst = [], []
        for nb in self.walk_nb:
            m = self.alive & (nb >= 0)
            m[m] &= self.alive[nb[m]]
            src.append(np.flatnonzero(m)); dst.append(nb[m])
        if len(self.drops):
            # erosion removes the ledge cells a drop starts/ends on; attach each end to the nearest alive span
            rep = np.where(self.alive, np.arange(n), -1)
            for _ in range(int(math.ceil(self.p['agent_r'] / self.p['cs'])) + 2):
                for nb in self.walk_nb:
                    take = (rep < 0) & self.open & (nb >= 0)
                    take[take] &= rep[nb[take]] >= 0
                    rep[take] = rep[nb[take]]
            a, b = rep[self.drops[:, 0]], rep[self.drops[:, 1]]
            keep = (a >= 0) & (b >= 0) & (a != b)
            src.append(a[keep]); dst.append(b[keep])
        self.base_src = np.concatenate(src) if src else np.zeros(0, np.int64)
        self.base_dst = np.concatenate(dst) if dst else np.zeros(0, np.int64)
        self.links = {}
        self._rebuild()

    def _rebuild(self):
        n = len(self.col)
        src = [self.base_src] + [l[0] for l in self.links.values()]
        dst = [self.base_dst] + [l[1] for l in self.links.values()]
        src, dst = np.concatenate(src), np.concatenate(dst)
        self.adj = csr_matrix((np.ones(len(src), np.int8), (src, dst)), shape=(n, n))
        _, self.scc = connected_components(self.adj, directed=True, connection='strong')
        self.scc = np.where(self.alive, self.scc, -1)

    # -- jump links (Crysis is vertical: players and bots must jump)
    def add_jumps(self, name, v0, vh, energy_note='', stride=2, dirs=16, step=0.6, apex_margin=0.3):
        """One-way ballistic links between alive spans in different SCCs.

        v0: take-off vertical speed (normal jump 4.43 m/s -> 1.0 m apex; Strength 9.60 m/s -> 4.7 m apex,
        PlayerMovement.cpp:1134,1150-1170); vh: horizontal speed at take-off (run 4.5, sprint 6.75 m/s).
        A link exists when the landing is below the apex minus a margin, within the horizontal reach for that
        height, and no solid span blocks the capsule along the arc."""
        g = 9.81; cs = self.p['cs']; H = self.p['agent_h']
        apex = v0 * v0 / (2 * g)
        cx_all = self.col % self.nx; cy_all = self.col // self.nx
        take = np.flatnonzero(self.alive & (cx_all % stride == 0) & (cy_all % stride == 0))
        # longest reach: landing max_drop below take-off
        tmax = (v0 + math.sqrt(v0 * v0 + 2 * g * self.p['max_drop'])) / g
        rmax = vh * tmax
        src_out, dst_out = [], []
        seen_keys = np.zeros(0, np.int64)
        z0 = self.zmax[take]; x0 = cx_all[take]; y0 = cy_all[take]; scc0 = self.scc[take]
        ang = np.arange(dirs) * 2 * math.pi / dirs
        for a in ang:
            ux, uy = math.cos(a), math.sin(a)
            for dist in np.arange(step, rmax + 1e-6, step):
                tx = np.round(x0 + ux * dist / cs).astype(np.int64); ty = np.round(y0 + uy * dist / cs).astype(np.int64)
                inb = (tx >= 0) & (tx < self.nx) & (ty >= 0) & (ty < self.ny)
                tcol = np.where(inb, ty * self.nx + tx, 0)
                s0 = self.cstart[tcol]; s1 = self.cend[tcol]
                maxk = int((s1 - s0)[inb].max()) if inb.any() else 0
                for k in range(maxk):
                    j = s0 + k; ok = inb & (j < s1)
                    jj = np.where(ok, j, 0)
                    ok &= self.alive[jj] & (self.scc[jj] != scc0)
                    if not ok.any():
                        continue
                    dz = self.zmax[jj] - z0
                    ok &= dz <= apex - apex_margin
                    ok &= dz >= -self.p['max_drop']
                    # flight time to reach dz on the descending branch; horizontal reach must cover dist
                    disc = np.maximum(v0 * v0 - 2 * g * dz, 0)
                    T = (v0 + np.sqrt(disc)) / g
                    ok &= vh * T >= dist
                    if not ok.any():
                        continue
                    idx = np.flatnonzero(ok)
                    # arc clearance: sample the straight horizontal path at the time the agent passes it
                    clear = np.ones(len(idx), bool)
                    nsamp = max(2, int(math.ceil(dist / (cs * 0.7))))
                    for frac in (np.arange(1, nsamp) / nsamp):
                        d = dist * frac; t = d / vh
                        zf = z0[idx] + v0 * t - 0.5 * g * t * t
                        px = np.round(x0[idx] + ux * d / cs).astype(np.int64); py = np.round(y0[idx] + uy * d / cs).astype(np.int64)
                        c = py * self.nx + px
                        a0 = self.cstart[c]; a1 = self.cend[c]
                        for kk in range(int((a1 - a0).max()) if len(c) else 0):
                            q = a0 + kk; qq = np.where(q < a1, q, 0); has = q < a1
                            # a solid span overlapping the capsule [zf+0.1, zf+H-0.4] blocks the arc
                            hit = has & (self.zmin[qq] < zf + H - 0.4) & (self.zmax[qq] > zf + 0.1)
                            clear &= ~hit
                    idx = idx[clear]
                    if len(idx):
                        # evaluation needs component connectivity only: keep one link per (src SCC, dst SCC) pair
                        key = scc0[idx].astype(np.int64) * (1 << 32) + self.scc[jj[idx]].astype(np.int64)
                        key, first = np.unique(key, return_index=True)
                        fresh = ~np.isin(key, seen_keys) if len(seen_keys) else np.ones(len(key), bool)
                        sel = idx[first[fresh]]
                        if len(sel):
                            seen_keys = np.concatenate([seen_keys, key[fresh]])
                            src_out.append(take[sel]); dst_out.append(jj[sel])
        src = np.concatenate(src_out) if src_out else np.zeros(0, np.int64)
        dst = np.concatenate(dst_out) if dst_out else np.zeros(0, np.int64)
        self.links[name] = (src, dst)
        self._rebuild()
        return len(src)

    # -- queries
    def project(self, x, y, z, up=0.6, down=1.2, radius=1):
        """Open (alive) span under a feet position; nearest top within [z-down, z+up], searching `radius` cells around."""
        cs = self.p['cs']; ix = int((x - self.origin[0]) / cs); iy = int((y - self.origin[1]) / cs)
        best, bd = -1, 1e9
        rng = range(-radius, radius + 1)
        for ddx in rng:
            for ddy in rng:
                cx, cy = ix + ddx, iy + ddy
                if not (0 <= cx < self.nx and 0 <= cy < self.ny):
                    continue
                c = cy * self.nx + cx
                for s in range(self.cstart[c], self.cend[c]):
                    if self.alive[s] and z - down <= self.zmax[s] <= z + up:
                        d = abs(self.zmax[s] - z) + 0.5 * (abs(ddx) + abs(ddy))
                        if d < bd:
                            best, bd = s, d
        return best

    def condensation(self):
        """SCC -> set of SCCs it has edges to."""
        coo = self.adj.tocoo()
        a, b = self.scc[coo.row], self.scc[coo.col]
        m = (a >= 0) & (b >= 0) & (a != b)
        out = {}
        for x, y in zip(a[m].tolist(), b[m].tolist()):
            out.setdefault(x, set()).add(y)
        return out

    def reach(self, comp, cond, cache):
        if comp in cache:
            return cache[comp]
        seen = {comp}; q = deque([comp])
        while q:
            for y in cond.get(q.popleft(), ()):
                if y not in seen:
                    seen.add(y); q.append(y)
        cache[comp] = seen
        return seen


# ----------------------------------------------------------------------------------------------- metrics
def profile(model, spawns=(), items=(), route=(), route_max_step=15.0):
    """Same questions as navgraph.scc_profile, answered on the walk model.

    spawns/items: iterables of (id, x, y, z); route: list of (t, x, y, z) owner positions (MOVE TRACE kind=human).
    """
    m = model; cs2 = m.p['cs'] ** 2
    alive = m.alive
    sizes = np.bincount(m.scc[alive]) if alive.any() else np.zeros(1, int)
    cond = m.condensation(); cache = {}
    comps = np.flatnonzero(sizes)
    main = int(np.argmax(sizes)) if alive.any() else -1
    if spawns:                                   # main part = the component holding most spawns, not the biggest area
        votes = {}
        for _, x, y, z in spawns:
            sp = m.project(x, y, z)
            if sp >= 0:
                votes[int(m.scc[sp])] = votes.get(int(m.scc[sp]), 0) + 1
        if votes:
            main = max(votes, key=votes.get)
    sinks = [c for c in comps if not cond.get(int(c)) and c != main]
    # main reach: everything reachable from main and everything that can reach main
    fwd = m.reach(main, cond, cache)
    can_reach_main = {int(c) for c in comps if main in m.reach(int(c), cond, cache)}
    res = {
        'params': m.p,
        'walkable_m2': round(float(alive.sum()) * cs2, 1),
        'open_before_erosion_m2': round(float(m.open.sum()) * cs2, 1),
        'components': int(len(comps)),
        'main_m2': round(float(sizes[main]) * cs2, 1) if main >= 0 else 0.0,
        'main_share_pct': round(float(100.0 * sizes[main] / max(1, alive.sum())), 1) if main >= 0 else 0.0,
        'sinks': len(sinks),
        'sink_m2': round(float(sum(sizes[s] for s in sinks if s != main)) * cs2, 1),
        'two_way_with_main_m2': round(float(sum(sizes[c] for c in can_reach_main if c in fwd)) * cs2, 1),
    }

    def classify(pts):
        out = {'on_mesh': 0, 'main': 0, 'reach_main_only': 0, 'from_main_only': 0, 'island': 0, 'off_mesh': []}
        for pid, x, y, z in pts:
            # items lie on crates/racks and spawns stand next to walls: allow 1 m sideways, 2 m down
            s = m.project(x, y, z, up=0.6, down=2.0, radius=max(1, int(round(1.0 / m.p['cs']))))
            if s < 0:
                out['off_mesh'].append(pid); continue
            out['on_mesh'] += 1
            c = int(m.scc[s])
            if c == main: out['main'] += 1
            elif c in can_reach_main and c in fwd: out['main'] += 1
            elif c in can_reach_main: out['reach_main_only'] += 1
            elif c in fwd: out['from_main_only'] += 1
            else: out['island'] += 1
        return out

    if spawns:
        res['spawns'] = classify(spawns); res['spawns']['total'] = len(spawns)
    if items:
        res['items'] = classify(items); res['items']['total'] = len(items)
    if route:
        on = 0; pairs = ok = 0; prev = None
        for t, x, y, z in route:
            s = m.project(x, y, z)
            if s >= 0:
                on += 1
            cur = (s, x, y, z)
            if prev is not None and prev[0] >= 0 and s >= 0:
                if math.dist(prev[1:3], cur[1:3]) <= route_max_step:
                    pairs += 1
                    a, b = int(m.scc[prev[0]]), int(m.scc[s])
                    ok += (a == b) or (b in m.reach(a, cond, cache))
            prev = cur
        res['route'] = {'samples': len(route), 'on_mesh_pct': round(100.0 * on / max(1, len(route)), 1),
                        'pairs': pairs, 'reproducible_pct': round(100.0 * ok / max(1, pairs), 1)}
    return res


def render_png(model, path, spawns=(), route=(), scale=1):
    """Top view: highest alive span per column. Green = two-way with the main part, orange = can only reach main
    (drop-offs), red = reachable from main but no way back (sinks/pits), grey = isolated, black = not walkable."""
    from PIL import Image, ImageDraw
    m = model
    cond = m.condensation(); cache = {}
    sizes = np.bincount(m.scc[m.alive]) if m.alive.any() else np.zeros(1, int)
    main = int(np.argmax(sizes))
    fwd = m.reach(main, cond, cache)
    comps = np.flatnonzero(sizes)
    back = {int(c) for c in comps if main in m.reach(int(c), cond, cache)}
    cls = np.zeros(len(sizes), np.uint8)
    for c in comps:
        c = int(c)
        cls[c] = 1 if (c in fwd and c in back) else 2 if c in back else 3 if c in fwd else 4
    pal = np.array([[0, 0, 0], [60, 170, 90], [230, 150, 40], [210, 60, 60], [120, 120, 120]], np.uint8)
    img = np.zeros((m.ny, m.nx, 3), np.uint8)
    idx = np.flatnonzero(m.alive)
    order = idx[np.argsort(m.zmax[idx])]                       # higher spans painted last
    cx = m.col[order] % m.nx; cy = m.col[order] // m.nx
    img[cy, cx] = pal[cls[m.scc[order]]]
    im = Image.fromarray(img[::-1])                             # north up
    if scale != 1:
        im = im.resize((m.nx * scale, m.ny * scale), Image.NEAREST)
    d = ImageDraw.Draw(im)
    def px(x, y):
        return ((x - m.origin[0]) / m.p['cs'] * scale, (m.ny - (y - m.origin[1]) / m.p['cs']) * scale)
    pts = [px(x, y) for _, x, y, _ in route]
    if len(pts) > 1:
        d.line(pts, fill=(80, 160, 255), width=max(1, scale))
    for _, x, y, _ in spawns:
        u, w = px(x, y); r = 3 * scale
        d.ellipse((u - r, w - r, u + r, w + r), outline=(255, 255, 255), width=max(1, scale))
    im.save(path)
    return path
