"""Owner jumps from MOVE TRACE (kind=human) -> filtered off-mesh link candidates (OC-023, done by Claude).

Rejects 4 Hz gaps / revive teleports (external audit: OWNER_JUMPS had 59 m and 76 m "jumps"): a jump is kept only if
its flight time is plausible, its horizontal distance fits the ballistic reach for its height change, and no
position step faster than 20 m/s happens in the window.
"""
import math
import re

_KV = re.compile(r'(\w+)=(\([^)]*\)|\S+)')
G = 9.81
V0 = {'normal': 4.43, 'strength': 9.60}            # PlayerMovement.cpp:1134,1150-1170
VH_MAX = 10.0                                       # sprint 6.75; Speed-mode bursts up to ~10


def reach(v0, dz, vh=VH_MAX):
    disc = v0 * v0 - 2 * G * dz
    if disc < 0:
        return 0.0
    return vh * (v0 + math.sqrt(disc)) / G


def samples(path, side='client'):
    out = []
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            i = line.find('MOVE TRACE ')
            if i < 0 or 'kind=human' not in line:
                continue
            kv = dict(_KV.findall(line[i + 11:]))
            if side and kv.get('side') != side:
                continue
            try:
                x, y, z = (float(c) for c in kv['pos'].strip('()').split(','))
                out.append(dict(t=float(kv['t']), x=x, y=y, z=z, jump=int(kv.get('jumpEdges', 0)),
                                land=int(kv.get('landEdges', 0)), peak=float(kv.get('peakZ', z)),
                                energy=float(kv.get('energy', 0)), mode=int(kv.get('mode', 3)),
                                health=float(kv.get('health', 100))))
            except (KeyError, ValueError):
                pass
    return out


def extract(ss):
    jumps, open_ = [], None
    for i in range(1, len(ss)):
        a, b = ss[i - 1], ss[i]
        if b['jump'] > a['jump']:
            open_ = dict(i=i, take=a, e0=a['energy'])
        if open_ and b['land'] > a['land'] and i > open_['i'] - 1:
            tk = open_['take']; dt = b['t'] - tk['t']
            seg = ss[open_['i'] - 1:i + 1]
            fastest = max((math.dist((p['x'], p['y']), (q['x'], q['y'])) / max(1e-3, q['t'] - p['t'])
                           for p, q in zip(seg, seg[1:])), default=0.0)
            dz = b['z'] - tk['z']; dist = math.dist((tk['x'], tk['y']), (b['x'], b['y']))
            peak = max(p['peak'] for p in seg) - tk['z']
            edrop = open_['e0'] - min(p['energy'] for p in seg)
            kind = 'strength' if (edrop >= 50 or peak > 1.2 or dz > 0.9) else ('drop' if dz < -1.5 and peak < 0.6 else 'normal')
            vh = 13.5 if any(p['mode'] == 0 for p in seg) else VH_MAX          # Speed mode bursts
            reasons = []
            if not 0.15 <= dt <= 3.5:
                reasons.append(f'flight {dt:.2f}s')
            if fastest > 20.0:
                reasons.append(f'step {fastest:.0f} m/s (teleport/revive)')
            apex = V0[kind] ** 2 / (2 * G) if kind != 'drop' else 0
            if kind != 'drop' and dz > apex + 0.3:
                reasons.append(f'dz {dz:.1f} > apex {apex:.1f} (multi-stage climb, intermediate landing missed)')
            elif kind != 'drop' and dist > reach(V0[kind], dz, vh) + 1.0:
                reasons.append(f'dist {dist:.1f} > reach {reach(V0[kind], dz, vh):.1f}')
            if any(p['health'] <= 0 for p in seg):
                reasons.append('died')
            jumps.append(dict(kind=kind, t=round(tk['t'], 2), take=(tk['x'], tk['y'], tk['z']), land=(b['x'], b['y'], b['z']),
                              dist=round(dist, 2), dz=round(dz, 2), peak=round(peak, 2), flight=round(dt, 2),
                              energy_drop=round(edrop, 1), keep=not reasons, reasons=reasons))
            open_ = None
    # clusters: >= 2 kept take-offs within 2 m with landings within 2 m -> link candidate
    kept = [j for j in jumps if j['keep']]
    for j in kept:
        j['cluster'] = sum(1 for k in kept if math.dist(j['take'], k['take']) <= 2 and math.dist(j['land'], k['land']) <= 2)
        j['link_candidate'] = j['cluster'] >= 2 and (j['dz'] > 0.6 or j['dist'] > 2.5)
    return jumps
