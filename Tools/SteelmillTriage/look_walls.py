# "Staring at walls": view-ray distance to collision for every bot sample in a wbdemo, Steelmill geometry.
import sys, math, numpy as np, collections, json
import os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from wblab import demo, navmesh
# usage: look_walls.py demo.wbdemo geometry.obj[.gz] [step]
D = demo.read(sys.argv[1]); STEP = int(sys.argv[3]) if len(sys.argv) > 3 else 5
V, F = navmesh.load_obj(sys.argv[2])
T = V[F].astype(np.float64); A = T[:, 0]; E1 = T[:, 1] - A; E2 = T[:, 2] - A
lo = T.min(1); hi = T.max(1); CELL = 2.0; ox, oy = lo[:, 0].min(), lo[:, 1].min()
grid = collections.defaultdict(list)
for i in np.flatnonzero((hi[:, 0] - lo[:, 0] < 400)):
    for cx in range(int((lo[i, 0] - ox) // CELL), int((hi[i, 0] - ox) // CELL) + 1):
        for cy in range(int((lo[i, 1] - oy) // CELL), int((hi[i, 1] - oy) // CELL) + 1):
            grid[(cx, cy)].append(i)
grid = {k: np.array(v) for k, v in grid.items()}
def ray(o, d, maxd=12.0):
    cells = set()
    for s in np.arange(0, maxd + CELL, CELL * 0.5):
        p = o + d * s; cells.add((int((p[0] - ox) // CELL), int((p[1] - oy) // CELL)))
    idx = [grid[c] for c in cells if c in grid]
    if not idx: return maxd
    idx = np.unique(np.concatenate(idx))
    e1, e2, a = E1[idx], E2[idx], A[idx]
    pv = np.cross(d, e2); det = (e1 * pv).sum(1); ok = np.abs(det) > 1e-9
    inv = np.where(ok, 1 / np.where(ok, det, 1), 0); tv = o - a
    u = (tv * pv).sum(1) * inv; qv = np.cross(tv, e1); v = (qv * d).sum(1) * inv; t = (e2 * qv).sum(1) * inv
    hit = ok & (u >= 0) & (v >= 0) & (u + v <= 1) & (t > 0.05) & (t < maxd)
    return float(t[hit].min()) if hit.any() else maxd
brain = collections.defaultdict(list)
for r in D['brain']: brain[r['id']].append(r)
def brain_at(aid, t):
    l = brain.get(aid); r = None
    if not l: return None
    lo_, hi_ = 0, len(l) - 1
    while lo_ <= hi_:
        m = (lo_ + hi_) // 2
        if l[m]['t'] <= t: r = l[m]; lo_ = m + 1
        else: hi_ = m - 1
    return r
kinds = {a: v['kind'] for a, v in D['actors'].items()}; names = {a: v['name'] for a, v in D['actors'].items()}
prev = {}; rows = []
for fi, (t, states) in enumerate(D['frames']):
    if fi % STEP: continue
    for s in states:
        aid, x, y, z, yaw, pitch = s[0], s[1], s[2], s[3], s[4] / 100.0, s[5] / 100.0
        if kinds.get(aid) != 'bot' or s[10] <= 0: prev.pop(aid, None); continue
        p = prev.get(aid); prev[aid] = (t, x, y)
        sp = None; mvdir = None
        if p and t - p[0] > 0:
            dx, dy = x - p[1], y - p[2]; sp = math.hypot(dx, dy) / (t - p[0])
            if sp > 1.0: mvdir = math.degrees(math.atan2(dx, dy))
        yr, pr = math.radians(yaw), math.radians(pitch)
        d = np.array([math.sin(yr) * math.cos(pr), math.cos(yr) * math.cos(pr), math.sin(pr)])
        dist = ray(np.array([x, y, z + 1.6]), d)
        b = brain_at(aid, t)
        rows.append(dict(t=t, id=aid, dist=dist, speed=sp, look_vs_move=abs((yaw - mvdir + 180) % 360 - 180) if mvdir is not None else None,
                         visible=bool(b and b['visible']), state=b['state'] if b else '?'))
# episodes: no visible target, dist < 1.5 m, consecutive samples >= 1 s
eps = []; cur = {}
dt = STEP / 20.0
for r in rows:
    k = r['id']; bad = (not r['visible']) and r['dist'] < 1.5
    if bad: cur.setdefault(k, [r['t'], r['t'], r['state']]); cur[k][1] = r['t']
    elif k in cur:
        s0, s1, st = cur.pop(k)
        if s1 - s0 + dt >= 1.0: eps.append((names[k], round(s0, 1), round(s1 - s0 + dt, 1), st))
nv = [r for r in rows if not r['visible']]
moving = [r for r in nv if r['look_vs_move'] is not None]
out = dict(samples=len(rows), no_target_samples=len(nv),
    share_view_wall_lt_1_5m_no_target=round(sum(r['dist'] < 1.5 for r in nv) / max(1, len(nv)), 3),
    share_view_wall_lt_3m_no_target=round(sum(r['dist'] < 3 for r in nv) / max(1, len(nv)), 3),
    moving_no_target=len(moving),
    look_vs_move_median_deg=round(float(np.median([r['look_vs_move'] for r in moving])), 1) if moving else None,
    look_off_travel_gt45_share=round(sum(r['look_vs_move'] > 45 for r in moving) / max(1, len(moving)), 3),
    look_off_travel_gt90_share=round(sum(r['look_vs_move'] > 90 for r in moving) / max(1, len(moving)), 3),
    wall_stare_episodes_ge1s=len(eps), wall_stare_seconds=round(sum(e[2] for e in eps), 1),
    by_state=collections.Counter(e[3] for e in eps).most_common(), longest=sorted(eps, key=lambda e: -e[2])[:10])
print(json.dumps(out, indent=1, ensure_ascii=False))
# why: for wall samples (no target, <1.5 m), where does the bot's aim point lie?
import statistics
st = collections.Counter(); offs = []; ad = []; vis_other = 0
idx = {}
for fi, (t, states) in enumerate(D['frames']):
    if fi % STEP: continue
    for s in states: idx[(round(t, 3), s[0])] = s
for r in rows:
    if r['visible'] or r['dist'] >= 1.5: continue
    b = brain_at(r['id'], r['t']); s = idx.get((round(r['t'], 3), r['id']))
    if not b or not s: continue
    want = math.degrees(math.atan2(b['aim'][0] - s[1], b['aim'][1] - s[2]))
    offs.append(abs((s[4] / 100 - want + 180) % 360 - 180)); ad.append(math.hypot(b['aim'][0] - s[1], b['aim'][1] - s[2]))
    st[b['reason']] += 1; vis_other += b['visible_count'] > 0
print('wall samples', len(offs), 'yaw-vs-aimpoint median', round(statistics.median(offs), 1), 'aim dist median', round(statistics.median(ad), 1),
      'aim point >5 m away share', round(sum(a > 5 for a in ad) / len(ad), 2), 'reasons', st.most_common(5))
