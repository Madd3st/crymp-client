"""C031 frozen probe validation and demo measurements. No engine dependencies."""
import argparse
import bisect
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import sys
sys.path.insert(0, str(Path(__file__).parent / 'SteelmillTriage'))
from wblab import demo

MAPS = {'Steelmill', 'Quarry', 'Armada', 'Outpost'}

def validate_manifest(data):
    if data.get('schema') != 'warsbots-probes/1':
        raise ValueError('Unknown probe manifest')
    ids = set()
    for c in data['cases']:
        if not re.fullmatch(r'[a-z0-9-]{1,40}', c['id']) or c['id'] in ids:
            raise ValueError('Invalid/duplicate scenario id')
        ids.add(c['id'])
        if c['map'] not in MAPS or c['kind'] not in {'pickup', 'jump', 'stand', 'decision'}:
            raise ValueError('Unsupported scenario')
        if not 30 <= c['seconds'] <= 90 or not 1 <= len(c['bots']) <= 2:
            raise ValueError('Probe duration/actor count out of range')
        for b in c['bots']:
            if len(b['position']) != 3 or not all(math.isfinite(x) and abs(x) < 100000 for x in b['position']):
                raise ValueError('Invalid frozen coordinates')
        if c['kind'] == 'jump' and c['jump_mode'] not in {'normal', 'strength'}:
            raise ValueError('Invalid jump mode')
    return data

def flips(brain):
    """A-B-A state changes within3s; never cross death/respawn boundaries."""
    seq = {}; count = 0
    for r in brain:
        s = seq.setdefault(r['id'], [])
        if r['state'] in {'DEAD', 'RESPAWNING'}:
            s.clear(); continue
        if s and s[-1][1] == r['state']:
            continue
        s.append((r['t'], r['state']))
        if len(s) >= 3 and s[-1][1] == s[-3][1] and s[-1][0] - s[-3][0] <= 3:
            count += 1
    return count

def stationary(d):
    """No-target intervals moving <0.2m/s; no inference about wall occlusion."""
    timelines = {}; times = {}; prev = {}; total = 0.; longest = 0.; episodes = 0
    for r in d['brain']:
        timelines.setdefault(r['id'], []).append(r)
    for a, rows in timelines.items(): times[a] = [r['t'] for r in rows]
    for t, rows in d['frames']:
        for row in rows:
            a = row[0]
            if d['actors'].get(a, {}).get('kind') != 'bot': continue
            p = prev.get(a); i = bisect.bisect_right(times.get(a, []), t) - 1
            b = timelines[a][i] if i >= 0 else None
            eligible = bool(row[7] & demo.F_ALIVE) and b and not b['target']
            duration = 0.
            if p and eligible and p[3] and 0 < t - p[0] <= .2:
                dt = t - p[0]
                if math.dist(row[1:4], p[1]) / dt < .2:
                    total += dt; duration = p[2] + dt
                    if p[2] < 1 <= duration: episodes += 1
                    longest = max(longest, duration)
            prev[a] = (t, row[1:4], duration, eligible)
    return {'seconds': round(total, 3), 'longest_s': round(longest, 3), 'episodes_ge1s': episodes,
            'definition': 'alive, no target, displacement speed<0.2m/s; not a geometry wall-stare detector'}

def analyze(path, case):
    raw = Path(path).read_bytes()
    if raw[:4] != b'WBDM': raise ValueError('Bad demo magic')
    version, n = struct.unpack_from('<HI', raw, 4); offset = 10 + n
    while offset + 3 <= len(raw):
        _, size = struct.unpack_from('<BH', raw, offset); offset += 3 + size
    if offset != len(raw): raise ValueError('Truncated demo record tail')
    d = demo.read(path)
    if version != 2 or not d['frames']: raise ValueError('Expected populated wbdemo/2')
    if d['header']['map'].lower() != ('multiplayer/ia/' + case['map']).lower(): raise ValueError('Wrong demo map')
    span = d['frames'][-1][0] - d['frames'][0][0]
    events = Counter(e['kind'] for e in d['events'])
    result = {'schema': 'warsbots-probe-result/1', 'id': case['id'], 'map': case['map'],
        'source_sha': d['header']['build'], 'demo_sha256': hashlib.sha256(raw).hexdigest(),
        'record_boundary_complete': True, 'frames': len(d['frames']), 'seconds': span,
        'hz': (len(d['frames'])-1)/span if span else None, 'events': dict(events),
        'aba_count': flips(d['brain']), 'stationary_no_target': stationary(d), 'metrics': demo.metrics(d)}
    result['runtime_ok'] = span >= case['seconds'] - 2 and len(d['actors']) >= len(case['bots'])
    result['outcome'] = 'measured'
    if case['kind'] == 'pickup':
        wanted = case['expected_item']
        pickups = [e for e in d['events'] if e['kind'] == 'pickup' and e['other'] == wanted['id']
                   and d['strings'].get(int(e['value'])) == wanted['class']]
        result['expected_item'] = wanted
        result['outcome'] = 'reached' if pickups else 'not_reached'
        result['first_expected_pickup_s'] = pickups[0]['t'] if pickups else None
    if case['kind'] == 'jump':
        # Match the explicitly queued2s jump, not an unrelated later travel jump.
        actor = next(a for a,v in d['actors'].items() if v['kind']=='bot')
        start = min((b['t'] for b in d['brain'] if b['id']==actor), default=d['frames'][0][0])
        mode = 1 if case['jump_mode']=='strength' else 0
        jump = next((e for e in d['events'] if e['kind']=='jump' and e['actor']==actor
                     and 1.5 <= e['t']-start <= 3.5 and e['value']==mode), None)
        land = next((e for e in d['events'] if jump and e['kind'] == 'land' and e['actor'] == jump['actor'] and e['t'] > jump['t']), None)
        result['outcome'] = 'landed' if land else 'not_completed'
        result['jump_mode'] = case['jump_mode']; result['scope'] = 'native vertical jump; not an off-mesh link test'
        result['requested_jump_observed'] = jump is not None
        result['flight_s'] = land['t']-jump['t'] if land else None
        result['landing_offset_m'] = math.dist(jump['pos'], land['pos']) if land else None
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--manifest', required=True)
    p.add_argument('--case');p.add_argument('--demo');p.add_argument('--out');a=p.parse_args()
    data=validate_manifest(json.loads(Path(a.manifest).read_text(encoding='utf-8-sig')))
    if not a.case: print('manifest PASS: %d cases' % len(data['cases']));return
    case=next(x for x in data['cases'] if x['id']==a.case)
    result=analyze(a.demo,case)
    Path(a.out).write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(case['id'],result['outcome'],'runtime_ok='+str(result['runtime_ok']))
if __name__=='__main__': main()
