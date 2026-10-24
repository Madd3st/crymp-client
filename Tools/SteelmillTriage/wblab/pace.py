"""Pace profile from MOVE TRACE lines (wb_move_trace, 4 Hz) and the C-013 acceptance check.

Line: `[WarsBots] MOVE TRACE actor=.. kind=human|bot side=server|client .. speed=.. mode=.. energy=.. sprint=0|1
grounded=.. takeoffs=.. peakZ=..`. mode: 0 Speed, 1 Strength, 2 Cloak, 3 Armor (CryNetwork ENanoMode).
"""
import re
from statistics import median

_KV = re.compile(r'(\w+)=(\([^)]*\)|\S+)')
MODES = {0: 'speed', 1: 'strength', 2: 'cloak', 3: 'armor'}
MOVING = 0.5  # m/s; below this the sample counts as standing

# C-013 acceptance (owner reference: median 5.0-5.7, >4 m/s 70-77 %, sprint 46-54 %, Armor 84-87 %)
TARGETS = {
    'moving_median': (4.5, None),
    'over4_pct': (60.0, None),
    'sprint_pct': (40.0, 55.0),
    'armor_pct': (75.0, None),
}


def parse(lines, side='server'):
    for line in lines:
        i = line.find('MOVE TRACE ')
        if i < 0:
            continue
        kv = dict(_KV.findall(line[i + 11:]))
        if side and kv.get('side') != side:
            continue
        try:
            yield {
                'actor': kv['actor'], 'kind': kv.get('kind', '?'),
                'speed': float(kv['speed']), 'mode': int(kv.get('mode', -1)),
                'energy': float(kv.get('energy', 'nan')), 'sprint': kv.get('sprint') == '1',
                'health': float(kv.get('health', '100')), 'takeoffs': int(kv.get('takeoffs', 0)),
            }
        except (KeyError, ValueError):
            continue


def _pct(a, b):
    return round(100.0 * a / b, 1) if b else None


def profile(samples):
    """Aggregate per kind over living samples."""
    out = {}
    by_kind = {}
    for s in samples:
        if s['health'] <= 0:
            continue
        by_kind.setdefault(s['kind'], []).append(s)
    for kind, ss in by_kind.items():
        moving = [s for s in ss if s['speed'] > MOVING]
        sp = [s['speed'] for s in moving]
        modes = {}
        for s in moving:
            m = MODES.get(s['mode'], str(s['mode']))
            modes[m] = modes.get(m, 0) + 1
        takeoffs = {}
        for s in ss:
            takeoffs[s['actor']] = max(takeoffs.get(s['actor'], 0), s['takeoffs'])
        out[kind] = {
            'actors': len({s['actor'] for s in ss}),
            'samples': len(ss),
            'moving_pct': _pct(len(moving), len(ss)),
            'moving_median': round(median(sp), 2) if sp else None,
            'p90': round(sorted(sp)[int(0.9 * (len(sp) - 1))], 2) if sp else None,
            'max': round(max(sp), 2) if sp else None,
            'over4_pct': _pct(sum(v > 4.0 for v in sp), len(sp)),
            'sprint_pct': _pct(sum(s['sprint'] for s in moving), len(moving)),
            'armor_pct': _pct(modes.get('armor', 0), len(moving)),
            'modes_pct': {k: _pct(v, len(moving)) for k, v in sorted(modes.items())},
            'energy_median': round(median(s['energy'] for s in ss), 1),
            'takeoffs_total': sum(takeoffs.values()),
        }
    return out


def accept(p):
    """C-013 check on one kind's profile -> (ok, [lines])."""
    ok, notes = True, []
    for key, (lo, hi) in TARGETS.items():
        v = p.get(key)
        good = v is not None and (lo is None or v >= lo) and (hi is None or v <= hi)
        ok &= good
        rng = f">={lo}" if hi is None else f"{lo}..{hi}"
        notes.append(f"{'OK ' if good else 'BAD'} {key}={v} (target {rng})")
    return ok, notes
