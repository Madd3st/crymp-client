#!/usr/bin/env python3
"""Summarise a WarsBots CryMP server log: lifecycle, targeting, hits, kills, revives, profile, resets.
Usage: wb_log_report.py <server.log> [...]"""
import re, sys, collections

def report(path):
    text = open(path, encoding='utf-8', errors='replace').read()
    lines = text.splitlines()
    c = collections.Counter()
    created = re.findall(r'#(\d+) created EntityId=(\d+) name=(\S+) profile=(\S+)', text)
    acquired = re.findall(r'Bot (\d+) acquired enemy id=(\d+) name=(\S+) kind=(\w+)', text)
    hits = re.findall(r'Hit before-Lua target=(\d+) shooter=(\d+).*?damage=([\d.]+) health=(-?\d+) botTarget=(\d+) botShooter=(\d+)', text)
    kills = re.findall(r'#(\d+) combat kill shooter=(\d+) target=(\d+)', text)
    deaths = re.findall(r'#(\d+) death observed EntityId=(\d+)', text)
    revives = re.findall(r'#(\d+) revive result EntityId=(\d+) sameEntity=(\d) alive=(\d)', text)
    resets = len(re.findall(r'removed externally|Game reset: removing', text))
    restored = re.findall(r'Game reset: restored (\d+)/(\d+)', text)
    profile = re.findall(r'PROFILE bots=(\d+) avgSum=([\d.]+)ms/frame worstFrame=([\d.]+)ms rays=(\d+) targetScans=(\d+)', text)
    scores = re.findall(r'IA SCORE EntityId=(\d+) kills=(-?\d+) deaths=(-?\d+)', text)
    crash = 'CRASH DETECTED' in text or 'Exception' in text
    print(f'== {path}')
    print(f'bots created: {len(created)} ' + ', '.join(f'#{n}={e}({p})' for n, e, _, p in created))
    print(f'resets: {resets} restored: {restored}')
    print(f'enemy acquisitions: {len(acquired)} (bot->bot {sum(1 for a in acquired if a[3]=="bot")}, bot->human {sum(1 for a in acquired if a[3]=="human")})')
    bb = [h for h in hits if h[4] != '0' and h[5] != '0']
    print(f'hits: {len(hits)} (bot->bot {len(bb)}) damage sum {sum(float(h[2]) for h in hits):.0f}')
    print(f'kills: {len(kills)} deaths: {len(deaths)} revives: {len(revives)} (ok {sum(1 for r in revives if r[2]=="1" and r[3]=="1")})')
    if scores: print('last scores: ' + ', '.join(f'{e}:{k}/{d}' for e, k, d in scores[-4:]))
    if profile:
        avg = max(float(p[1]) for p in profile[1:] or profile)
        worst = max(float(p[2]) for p in profile)
        print(f'profile: max avgSum {avg:.3f} ms/frame, worst frame {worst:.3f} ms, rays/report max {max(int(p[3]) for p in profile)}')
    print(f'crash: {crash}')
    # Per-bot view (by display number): independence of decisions, kills, deaths.
    num_of = {e: n for n, e, _, _ in created}
    per = collections.defaultdict(lambda: collections.Counter())
    for shooter, target, _, _ in acquired: per[num_of.get(shooter, '?')]['acq'] += 1
    for n, shooter, target in kills:
        if shooter != target: per[num_of.get(shooter, '?')]['kills'] += 1
        else: per[num_of.get(shooter, '?')]['suicides'] += 1
    for n, _ in deaths: per[n]['deaths'] += 1
    selfhits = [h for h in hits if h[0] == h[1]]
    pos = re.findall(r'#(\d+) created EntityId=\d+ name=\S+ profile=\S+ position=\(([-\d.]+), ([-\d.]+), ([-\d.]+)\)', text)
    if pos:
        first = {}
        for n, x, y, z in pos: first.setdefault(n, (float(x), float(y)))
        pts = list(first.values())
        mind = min(((a[0]-b[0])**2 + (a[1]-b[1])**2) ** 0.5 for i, a in enumerate(pts) for b in pts[i+1:]) if len(pts) > 1 else 0
        print(f'initial spawns: {len(pts)} distinct bots, min separation {mind:.1f} m')
    print(f'self-damage hits (fall etc.): {len(selfhits)} sum {sum(float(h[2]) for h in selfhits):.0f}')
    for n in sorted(per, key=lambda k: int(k) if k.isdigit() else 99):
        print(f'  bot #{n}: ' + ' '.join(f'{k}={v}' for k, v in sorted(per[n].items())))
    # Behaviour quality: share of telemetry samples per state and share of blocked movement.
    states = collections.Counter(re.findall(r'AI telemetry EntityId=\d+ state=(\w+)', text))
    blocked = re.findall(r'MOTION INTENT: .*?requestedBase=([\d.]+).*?blocked=(\d)', text)
    moving = [b for r, b in blocked if float(r) > 0]
    if states:
        tot = sum(states.values())
        print('state share: ' + ', '.join(f'{k}={100*v/tot:.0f}%' for k, v in states.most_common()))
    if moving:
        print(f'blocked while trying to move: {100*sum(int(b) for b in moving)/len(moving):.0f}% of {len(moving)} samples')
    nav = re.findall(r'NAV ready: (\d+) nodes, (\d+) edges, build ([\d.]+) ms', text)
    if nav: print(f'nav: {nav[-1][0]} nodes, {nav[-1][1]} edges, {nav[-1][2]} ms build')
    verdict = 'PASS' if (len(created) >= 2 and bb and kills and not crash) else 'INCOMPLETE'
    print(f'M3 bot-vs-bot verdict: {verdict}')

for p in sys.argv[1:]:
    report(p)
