#!/usr/bin/env python3
"""WarsBots playtest report: one server *.bots.log (+ optional owner videos) -> one self-contained HTML page.

Finds, per bot: where it got stuck or stared at a wall, where it dithered (back-and-forth), why it jumped in Strength
(the BotBrain.cpp rule that asked, from INTENT WHY), what it held, how well it shot, whether it ever had a sniper
rifle or held a sniper spot. Every event carries the time in the session and, when videos are given, the video file
and the time inside it (NVIDIA ShadowPlay names carry the wall-clock start: "Desktop 2026.10.25 - 19.53.02.02.mp4").

Usage:
  playtest_report.py SERVER.bots.log [--source BotBrain.cpp] [--video FILE.mp4 ...] [--offset SEC] [--out report.html]
--source maps INTENT WHY line numbers to BotBrain functions (use the file of the build that played).
--offset shifts the video clock against the server clock if the PC clock and the log disagree (default 0).
Read-only: never touches the game, the logs or the videos.
"""
import argparse, bisect, collections, datetime as dt, html, json, math, os, re, statistics, sys

R_T = re.compile(r'^(\d+\.\d+) \[WarsBots')
R_KV = re.compile(r'(\w+)=\(([-\d.]+),\s*([-\d.]+),\s*([-\d.]+)\)|(\w+)=([^\s,]+)')

def kv(line):
    out = {}
    for m in R_KV.finditer(line):
        if m.group(1): out[m.group(1)] = (float(m.group(2)), float(m.group(3)), float(m.group(4)))
        else: out[m.group(5)] = m.group(6).rstrip('.')
    return out

def fnum(x, d=0.0):
    try: return float(x)
    except Exception: return d

MOVING_STATES = {'MOVING_TO_WEAPON', 'SEEKING_WEAPON', 'PATROL', 'PATROLLING', 'CHASING_ENEMY', 'ROAMING', 'MOVING', 'SEARCHING', 'UPGRADING', 'LOOTING'}

def source_functions(path):
    """line -> enclosing BotBrain function name."""
    if not path or not os.path.exists(path): return None
    starts = []
    for i, l in enumerate(open(path, encoding='utf-8', errors='replace'), 1):
        m = re.match(r'^[A-Za-z_][\w:<>,\s\*&]*\b(BotBrain::\w+)\s*\(', l)
        if m: starts.append((i, m.group(1).split('::')[1]))
    def f(line):
        k = bisect.bisect_right([s[0] for s in starts], line) - 1
        return starts[k][1] if k >= 0 else '?'
    return f

def video_index(paths, offset):
    vids = []
    for p in paths or []:
        m = re.search(r'(\d{4})\.(\d{2})\.(\d{2}) - (\d{2})\.(\d{2})\.(\d{2})', os.path.basename(p))
        if not m: continue
        start = dt.datetime(*map(int, m.groups()))
        dur = None
        try:
            import subprocess
            dur = float(subprocess.check_output(['ffprobe', '-v', 'error', '-show_entries', 'format=duration', '-of', 'csv=p=0', p], text=True).strip())
        except Exception: pass
        vids.append({'name': os.path.basename(p), 'start': start, 'dur': dur, 'offset': offset})
    return vids

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('log'); ap.add_argument('--source'); ap.add_argument('--video', nargs='*'); ap.add_argument('--offset', type=float, default=0.0)
    ap.add_argument('--out', default='playtest_report.html'); ap.add_argument('--title')
    a = ap.parse_args()
    fn = source_functions(a.source)

    names = {}; numToActor = {}; actorToNum = {}; skill = {}; style = {}
    state = {}                 # actor -> current AI state
    stateTl = collections.defaultdict(list)  # actor -> [(t, state)]
    numState = {}
    move = collections.defaultdict(list)   # actor -> [(t,x,y,z,speed,vx,vy,yaw,grounded)]
    weapons = collections.defaultdict(collections.Counter)  # actor -> class -> samples
    shots = collections.defaultdict(collections.Counter); hits = collections.defaultdict(collections.Counter)
    hitDist = collections.defaultdict(list)
    lastWhy = {}               # actor -> (t, dict)
    strength = []              # (t, actor, rule, line, state, pos)
    links = collections.Counter(); linkFail = collections.Counter()
    traps = []; score = {}; pickups = collections.Counter(); sniperHold = collections.Counter(); vantage = []
    arbiter = collections.defaultdict(list); targets = collections.defaultdict(list)
    demoStart = None; demoAim = None; tmax = 0.0; aimSeries = []; hitsAny = collections.Counter()
    for line in open(a.log, encoding='utf-8', errors='replace'):
        m = R_T.match(line)
        if not m: continue
        t = float(m.group(1)); tmax = max(tmax, t)
        num = re.search(r'\] #(\d+) ', line)
        d = kv(line)
        if 'CreateActor name=' in line: names[d['EntityId']] = d['name']
        raw = d.get('actor') or d.get('EntityId')
        actor = names.get(raw, raw)   # a bot keeps its name across re-created entities
        if 'shooter' in d: d['shooter'] = names.get(d['shooter'], d['shooter'])
        if num and actor and raw and raw.isdigit(): numToActor[num.group(1)] = actor; actorToNum[actor] = num.group(1)
        if 'DEMO START' in line:
            mm = re.search(r'-(\d{8})-(\d{6})-\d+\.wbdemo', line)
            if mm: demoStart = dt.datetime.strptime(mm.group(1) + mm.group(2), '%Y%m%d%H%M%S') - dt.timedelta(seconds=t)
        elif 'MOVE TRACE' in line and d.get('kind') == 'bot' and d.get('side') == 'server':
            p = d.get('pos'); v = d.get('vel')
            if p and v: move[actor].append((fnum(d.get('t'), t), p[0], p[1], p[2], fnum(d.get('speed')), v[0], v[1], fnum(d.get('yaw')), d.get('grounded') == '1'))
        elif ' AI state=' in line and num:
            s = re.search(r'AI state=([A-Z_]+)', line).group(1); numState[num.group(1)] = s
            if num.group(1) in numToActor: state[numToActor[num.group(1)]] = s; stateTl[numToActor[num.group(1)]].append((t, s))
        elif 'TACTIC SAMPLE' in line:
            weapons[actor][d.get('weapon', '?')] += 1; skill[actor] = d.get('skill'); style[actor] = d.get('style')
            if d.get('weapon') in ('DSG1', 'GaussRifle'): sniperHold[actor] += 1
        elif 'FIRE SHOT' in line: shots[d.get('shooter')][d.get('class')] += 1
        elif 'FIRE HIT' in line and 'phase=before' in line:
            hits[d.get('shooter')][d.get('class')] += 1; hitDist[d.get('shooter')].append(fnum(d.get('distance')))
        elif 'INTENT WHY' in line:
            jm = re.search(r'jump=(\d)@(\d+)', line); lastWhy[actor] = (t, int(jm.group(1)) if jm else 0, int(jm.group(2)) if jm else 0, d.get('state'))
            if d.get('state'): stateTl[actor].append((t, d.get('state')))
        elif 'JUMP TAKEOFF' in line and d.get('strength') == '1':
            w = lastWhy.get(actor); rule = '?'; ln = 0; st = '?'
            if w and t - w[0] < 1.5 and w[1] == 2: ln = w[2]; rule = fn(ln) if fn else 'line %d' % ln; st = w[3]
            pos = move[actor][-1][1:4] if move.get(actor) else None
            strength.append((t, actor, rule, ln, st, pos))
        elif 'LINK start' in line: links[d.get('kind')] += 1
        elif 'LINK failed' in line: linkFail[d.get('kind')] += 1
        elif 'TRAP RECOVERY' in line: traps.append((t, actor, int(d.get('level', 0)), d.get('pos')))
        elif 'IA SCORE' in line: score[actor] = (int(d.get('kills', 0)), int(d.get('deaths', 0)))
        elif 'REAL PICKUP' in line and d.get('acquired') == '1': pickups[actor] += 1
        elif 'VANTAGE' in line and '#' in line: vantage.append((t, actor, line.split('] ', 1)[1].strip()))
        elif 'ARBITER goal=' in line: arbiter[actor].append((t, d.get('goal'), d.get('reason')))
        elif 'TARGET CHOICE' in line: targets[actor].append((t, d.get('target')))
        elif 'DEMO AIM ERROR' in line:
            demoAim = line.split('] ', 1)[1].strip()
            if fnum(d.get('samples')) >= 50: aimSeries.append((round(t), fnum(str(d.get('median', '0')).replace('deg', '')), fnum(str(d.get('p95', '0')).replace('deg', ''))))
        elif 'Hit before-Lua' in line: hitsAny[d.get('shooter')] += 1

    bots = sorted(set(names.values()))
    names = {b: b for b in bots}
    vids = video_index(a.video, a.offset)

    def where(t):
        if not demoStart or not vids: return ''
        wall = demoStart + dt.timedelta(seconds=t)
        for v in vids:
            s = (wall - v['start']).total_seconds() + v['offset']
            if s >= 0 and (v['dur'] is None or s <= v['dur']): return 'видео %s → %d:%02d' % (v['start'].strftime('%H:%M:%S'), s // 60, s % 60)
        return ''

    events = []
    def stateAt(b, t):
        tl = stateTl.get(b)
        if not tl: return '?'
        k = bisect.bisect_right([x[0] for x in tl], t) - 1
        return tl[k][1] if k >= 0 else '?'
    # stuck: moving state, speed < 0.3 for >= 3 s (ignoring the first 2 s after a spawn gap)
    for b in bots:
        tr = move.get(b, []); i = 0
        while i < len(tr):
            if tr[i][4] < 0.3 and tr[i][8]:
                j = i
                while j + 1 < len(tr) and tr[j + 1][4] < 0.3 and abs(tr[j + 1][1] - tr[i][1]) < 1.0 and abs(tr[j + 1][2] - tr[i][2]) < 1.0 and tr[j + 1][0] - tr[j][0] < 1.0: j += 1
                dur = tr[j][0] - tr[i][0]
                if dur >= 3.0:
                    st = stateAt(b, tr[i][0])
                    if st not in ('DEAD', 'SPAWNING', 'ATTACKING', 'HOLDING'):
                        events.append({'t': tr[i][0], 'bot': b, 'type': 'stuck', 'dur': round(dur, 1), 'pos': tr[i][1:4], 'detail': 'стоит %.0f с при состоянии %s' % (dur, st)})
                i = j + 1
            else: i += 1
    # dithering: >= 3 reversals of the movement direction (> 120 deg) within 6 s while moving
    for b in bots:
        tr = [p for p in move.get(b, []) if p[4] > 1.0 and stateAt(b, p[0]) not in ('ATTACKING', 'DEAD', 'SPAWNING')]; rev = []  # combat strafing is intended
        for k in range(1, len(tr)):
            a1 = math.atan2(tr[k - 1][6], tr[k - 1][5]); a2 = math.atan2(tr[k][6], tr[k][5])
            dd = abs((a2 - a1 + math.pi) % (2 * math.pi) - math.pi)
            if dd > math.radians(120) and tr[k][0] - tr[k - 1][0] < 0.6: rev.append(tr[k])
        k = 0
        while k < len(rev):
            j = k
            while j + 1 < len(rev) and rev[j + 1][0] - rev[k][0] < 6.0: j += 1
            if j - k + 1 >= 3:
                events.append({'t': rev[k][0], 'bot': b, 'type': 'dither', 'dur': round(rev[j][0] - rev[k][0], 1), 'pos': rev[k][1:4], 'detail': '%d разворотов за %.0f с' % (j - k + 1, rev[j][0] - rev[k][0] + 0.1)})
                k = j + 1
            else: k += 1
    # goal flapping: >= 4 arbiter goal changes within 10 s
    for b, lst in arbiter.items():
        ch = [lst[i] for i in range(1, len(lst)) if lst[i][1] != lst[i - 1][1]]; k = 0
        while k < len(ch):
            j = k
            while j + 1 < len(ch) and ch[j + 1][0] - ch[k][0] < 10: j += 1
            if j - k + 1 >= 4:
                pos = None
                for p in move.get(b, []):
                    if p[0] >= ch[k][0]: pos = p[1:4]; break
                events.append({'t': ch[k][0], 'bot': b, 'type': 'flip', 'dur': round(ch[j][0] - ch[k][0], 1), 'pos': pos, 'detail': 'цель меняется %d раз: %s' % (j - k + 1, ' → '.join(c[1] or '?' for c in ch[k:j + 1])[:120])})
                k = j + 1
            else: k += 1
    for (t, b, lvl, pos) in traps:
        if lvl >= 2: events.append({'t': t, 'bot': b, 'type': 'trap', 'dur': 0, 'pos': pos, 'detail': 'аварийный выход из застревания, уровень %d' % lvl})
    strengthRules = collections.Counter()
    for (t, b, rule, ln, st, pos) in strength:
        strengthRules[(rule, ln)] += 1
        events.append({'t': t, 'bot': b, 'type': 'strength', 'dur': 0, 'pos': pos, 'detail': 'прыжок в силе: %s%s, состояние %s' % (rule, (' (строка %d)' % ln) if ln else '', st)})
    for e in events: e['video'] = where(e['t'])
    events.sort(key=lambda e: e['t'])

    # per-bot table
    rows = []
    for b in bots:
        tot = sum(weapons[b].values()) or 1
        wtxt = ', '.join('%s %d%%' % (k, 100 * v // tot) for k, v in weapons[b].most_common(4))
        sh = sum(shots[b].values()); hi = sum(hits[b].values())
        ev = collections.Counter(e['type'] for e in events if e['bot'] == b)
        rows.append({'bot': b, 'name': names.get(b, b), 'skill': skill.get(b, '?'), 'style': style.get(b, '?'), 'kd': score.get(b, (0, 0)),
                     'weapons': wtxt, 'acc': (100.0 * hi / sh) if sh else 0, 'shots': sh, 'pickups': pickups[b], 'sniper': sniperHold[b] * 2,
                     'stuck': ev['stuck'], 'dither': ev['dither'], 'flip': ev['flip'], 'strength': ev['strength'], 'trap': ev['trap'],
                     'medHit': statistics.median(hitDist[b]) if hitDist[b] else 0, 'hits': hi or hitsAny[b]})
    paths = {b: [(round(p[1], 1), round(p[2], 1)) for p in move.get(b, [])[::4]] for b in bots}
    data = {'title': a.title or os.path.basename(a.log), 'duration': round(tmax), 'rows': rows, 'events': events, 'paths': paths,
            'names': {b: names.get(b, b) for b in bots},
            'strengthRules': [[r, ln, c] for (r, ln), c in strengthRules.most_common()],
            'aim': aimSeries, 'links': dict(links), 'linkFail': dict(linkFail), 'vantage': vantage[:50], 'demoAim': demoAim,
            'videos': [{'name': v['name'], 'start': v['start'].strftime('%H:%M:%S'), 'dur': v['dur']} for v in vids],
            'demoStart': demoStart.strftime('%Y-%m-%d %H:%M:%S') if demoStart else None}
    tpl = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'template.html'), encoding='utf-8').read()
    open(a.out, 'w', encoding='utf-8').write(tpl.replace('/*DATA*/null', json.dumps(data, ensure_ascii=False)))
    print('wrote %s: %d bots, %d events (%s)' % (a.out, len(bots), len(events), dict(collections.Counter(e['type'] for e in events))))

if __name__ == '__main__':
    main()
