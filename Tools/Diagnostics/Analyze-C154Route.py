"""C154 live/archive gate: one registered lower spawn through native ladder to dry shore."""
from pathlib import Path
import argparse
import json
import math
import re

F = r'-?\d+(?:\.\d+)?'
TRACE = re.compile(r'MOVE TRACE actor=(\d+) kind=bot .*?t=('+F+r') .*?pos=\(('+F+r'),('+F+r'),('+F+r')\).*?grounded=(\d+).*?health=(\d+).*?swim=(\d+)')


def analyze(text):
    # A trailing partially written row cannot be evidence; repeated complete chunks are harmless.
    rows = list(dict.fromkeys(text[:text.rfind('\n')+1].splitlines()))
    result = dict(status='WAIT', failure=None, stages={}, actor=None, trace_rows=0,
                  setup='one explicit native registered spawn545; setup death excluded', client_visual='NOT TESTED')

    def fail(reason):
        result.update(status='FAIL', failure=reason)
        return result

    setups = [(i, re.search(r'DEBUG REVIVE actor=(\d+) bot=2 spawnId=545 manual=1 nativeKill=1 zOffset=0\.200 hooks=0', s))
              for i, s in enumerate(rows) if 'DEBUG REVIVE actor=' in s]
    if not setups:
        return result
    if len(setups) != 1 or setups[0][1] is None:
        return fail('Setup must use registered lower545 exactly once, no other manual actor setup')
    setup, match = setups[0]
    actor = match[1]
    result['actor'] = int(actor)
    births = [s for s in rows if '#2 created EntityId=' in s]
    if len(births) != 1 or f'#2 created EntityId={actor} ' not in births[0]:
        return fail('Retained slot was recreated or setup actor does not match')
    revive = next((i for i in range(setup+1, len(rows))
                   if f'#2 revive result EntityId={actor} sameEntity=1 alive=1 health=100.' in rows[i]), None)
    if revive is None:
        return result
    result['stages']['setup_revive'] = revive
    started = False
    goal = None
    first_water = None
    dry_since = None
    last_t = -1
    lower = False
    for i in range(revive+1, len(rows)):
        s = rows[i]
        if re.search(r'#2 (?:AI state=DEAD|AI state=RESPAWNING|revive requested|revive result|removed|ENTITY_EVENT_DONE)', s):
            return fail('Death, respawn or actor replacement after setup')
        if re.search(r'DEBUG (?:REVIVE|TELEPORT)|STATE CONFLICT|REVIVE GUARD.*(?:retry=[1-9]|nativeKill=1)|TRAP.*(?:rescue|teleport)|TEST GRANT', s):
            return fail('Post-setup intervention/state conflict')
        if '#2 NAV fallback' in s or '#2 LINK failed' in s or '#2 LADDER failed' in s:
            return fail('Route/link failure: '+s)
        if '#2 PS CAPTURE target=' in s:
            m = re.search(r'#2 PS CAPTURE target=(\d+) pos=\(('+F+r'),('+F+r'),('+F+r')\).*actor='+actor+r'\.', s)
            if not m or m[1] != '654':
                return fail('Native destination changed from declared shore target654')
            goal = (float(m[2]), float(m[3]), float(m[4]))
        if '#2 LADDER start ' in s:
            if started:
                return fail('Repeated ladder attempt')
            started = True
            result['stages']['ladder_start'] = i
        if started and '#2 LADDER cancel ' in s:
            return fail('Native ladder cancelled')
        if '#2 LADDER phase=' in s and f'actor={actor}.' in s:
            if re.search(r'nativeOn=1 exiting=0', s):
                result['stages'].setdefault('native_attached', i)
            if 'exiting=1' in s:
                result['stages'].setdefault('native_exiting', i)
        if '#2 LADDER done ' in s:
            m = re.search(r'nativeOn=0 exiting=0 grounded=1 pos=\(('+F+r'),('+F+r'),('+F+r')\) actor='+actor+r'\.', s)
            if not m or not started or not 98.8 <= float(m[3]) <= 100.7:
                return fail('Ladder completion lacks native clear/grounded correct-floor evidence')
            result['stages']['ladder_done'] = i
        if '#2 LINK done kind=strength' in s:
            if 'ladder_done' not in result['stages']:
                return fail('Upper jump precedes native ladder completion')
            result['stages']['strength_done'] = i
        m = TRACE.search(s)
        if not m or m[1] != actor:
            continue
        t, x, y, z = map(float, m.group(2, 3, 4, 5))
        ground, hp, swim = map(int, m.group(6, 7, 8))
        if t <= last_t:
            return fail('Trace clock does not increase')
        last_t = t
        result['trace_rows'] += 1
        if hp <= 0:
            return fail('Bot died after setup')
        if ground and not swim and 89.5 <= z <= 91.0 and math.hypot(x-2114.5, y-1887.875) <= 3:
            lower = True
            result['stages'].setdefault('lower_grounded', dict(t=t, pos=[x, y, z]))
        if 'strength_done' not in result['stages']:
            continue
        if swim:
            first_water = t if first_water is None else first_water
            result['stages'].setdefault('water', t)
        qualified = (first_water is not None and not swim and ground and z > 100.5 and goal is not None
                     and math.hypot(x-goal[0], y-goal[1]) <= 40 and math.hypot(x-2110, y-1880) > 100)
        if not qualified:
            dry_since = None
            result['status'] = 'WAIT'
        else:
            dry_since = t if dry_since is None else dry_since
            result['final'] = dict(t=t, pos=[x, y, z], health=hp, native_goal=654,
                                   distance_to_goal=math.hypot(x-goal[0], y-goal[1]), dry_seconds=t-dry_since)
            if t-dry_since >= 7 and lower and all(k in result['stages'] for k in ('native_attached', 'native_exiting', 'ladder_done')):
                result['status'] = 'PASS'
    # Later failures above still override an earlier dry interval.
    return result


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('trace', type=Path)
    ap.add_argument('--out', type=Path)
    args = ap.parse_args()
    result = analyze(args.trace.read_text(encoding='utf-8-sig', errors='replace') if args.trace.exists() else '')
    payload = json.dumps(result, ensure_ascii=False)
    if args.out:
        args.out.write_text(payload+'\n', encoding='utf-8')
    print(payload)


if __name__ == '__main__':
    main()
