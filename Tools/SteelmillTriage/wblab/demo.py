"""Reader, test writer and metrics for WarsBots demos (`wbdemo/1`, Docs/Research/SteelmillTriage/DEMO_FORMAT.md)."""
import json
import math
import struct
from statistics import median

MAGIC = b'WBDM'
STATE = struct.Struct('<IfffhhBBBBBHI')          # 31 bytes
BRAIN = struct.Struct('<IIHIB3f3f3fH')
EVENT = struct.Struct('<IBIIf3f')
EV = {1: 'shot', 2: 'hit', 3: 'kill', 4: 'revive', 5: 'pickup', 6: 'drop', 7: 'suit', 8: 'jump', 9: 'land', 10: 'mark'}
F_ALIVE, F_SPRINT, F_GROUND, F_FIRING = 1, 2, 4, 8
PISTOLS = {'SOCOM', 'Socom', 'socom'}


# ------------------------------------------------------------------------------------------------ reading
def read(path):
    with open(path, 'rb') as f:
        b = f.read()
    if b[:4] != MAGIC:
        raise ValueError('not a wbdemo file')
    ver, hlen = struct.unpack_from('<HI', b, 4)
    header = json.loads(b[10:10 + hlen].decode('utf-8'))
    d = dict(version=ver, header=header, strings={0: ''}, actors={}, frames=[], brain=[], events=[])
    o = 10 + hlen
    while o + 3 <= len(b):
        typ, ln = struct.unpack_from('<BH', b, o); o += 3
        p = b[o:o + ln]; o += ln
        if len(p) < ln:
            break                                  # truncated tail (file still being written)
        if typ == 1:
            idx, sl = struct.unpack_from('<HB', p); d['strings'][idx] = p[3:3 + sl].decode('utf-8', 'replace')
        elif typ == 2:
            t, aid, kind, ni, pi, join = struct.unpack_from('<IIBHHB', p)
            d['actors'].setdefault(aid, dict(id=aid, kind='human' if kind == 0 else 'bot',
                                             name=d['strings'].get(ni, str(aid)),
                                             profile=d['strings'].get(pi, '')))
        elif typ == 3:
            t, n = struct.unpack_from('<IB', p)
            rows = [STATE.unpack_from(p, 5 + k * STATE.size) for k in range(n)]
            d['frames'].append((t / 1000.0, rows))
        elif typ == 4:
            r = BRAIN.unpack_from(p)
            d['brain'].append(dict(t=r[0] / 1000.0, id=r[1], state=d['strings'].get(r[2], ''), target=r[3],
                                   visible=bool(r[4]), aim=r[5:8], dest=r[8:11], corner=r[11:14],
                                   reason=d['strings'].get(r[14], '')))
        elif typ == 5:
            t, kind, a, other, val, x, y, z = EVENT.unpack_from(p)
            d['events'].append(dict(t=t / 1000.0, kind=EV.get(kind, str(kind)), actor=a, other=other, value=val,
                                    pos=(x, y, z)))
    return d


# ------------------------------------------------------------------------------------------------ writing (tests, fixtures)
class Writer:
    def __init__(self, path, header):
        self.f = open(path, 'wb'); self.s = {}
        h = json.dumps(header).encode()
        self.f.write(MAGIC + struct.pack('<HI', 1, len(h)) + h)

    def _rec(self, typ, payload):
        self.f.write(struct.pack('<BH', typ, len(payload)) + payload)

    def sid(self, text):
        if not text:
            return 0
        if text not in self.s:
            self.s[text] = len(self.s) + 1
            raw = text.encode()[:255]
            self._rec(1, struct.pack('<HB', self.s[text], len(raw)) + raw)
        return self.s[text]

    def actor(self, t, aid, kind, name, profile=''):
        self._rec(2, struct.pack('<IIBHHB', int(t * 1000), aid, 0 if kind == 'human' else 1, self.sid(name),
                                 self.sid(profile), 1))

    def frame(self, t, states):
        """states: list of dicts id,x,y,z,yaw,pitch(deg),stance,flags,mode,energy,health,weapon(class),weapon_id."""
        body = b''.join(STATE.pack(s['id'], s['x'], s['y'], s['z'], int(round(s.get('yaw', 0) * 100)),
                                   int(round(s.get('pitch', 0) * 100)), s.get('stance', 0), s.get('flags', 5),
                                   s.get('mode', 3), s.get('energy', 200), s.get('health', 100),
                                   self.sid(s.get('weapon', '')), s.get('weapon_id', 0)) for s in states)
        self._rec(3, struct.pack('<IB', int(t * 1000), len(states)) + body)

    def brain(self, t, aid, state, target=0, visible=False, aim=(0, 0, 0), dest=(0, 0, 0), corner=(0, 0, 0), reason=''):
        self._rec(4, BRAIN.pack(int(t * 1000), aid, self.sid(state), target, int(visible), *aim, *dest, *corner,
                                self.sid(reason)))

    def event(self, t, kind, actor, other=0, value=0.0, pos=(0, 0, 0)):
        k = {v: k for k, v in EV.items()}[kind]
        self._rec(5, EVENT.pack(int(t * 1000), k, actor, other, value, *pos))

    def close(self):
        self.f.close()


# ------------------------------------------------------------------------------------------------ metrics
def _wrap(d):
    return (d + 180.0) % 360.0 - 180.0


def _yaw_to(dx, dy):
    return math.degrees(math.atan2(dx, dy))                     # 0 = +Y, clockwise (format convention)


def metrics(d):
    kinds = {a: v['kind'] for a, v in d['actors'].items()}
    # brain timeline per bot for "has a visible target now?"
    brain = {}
    for r in d['brain']:
        brain.setdefault(r['id'], []).append(r)

    def brain_at(aid, t):
        lst = brain.get(aid)
        if not lst:
            return None
        lo, hi, r = 0, len(lst) - 1, None
        while lo <= hi:
            m = (lo + hi) // 2
            if lst[m]['t'] <= t: r = lst[m]; lo = m + 1
            else: hi = m - 1
        return r

    wander = {'human': [], 'bot': []}; aim_err = []
    prev = {}
    for t, rows in d['frames']:
        for r in rows:
            aid, x, y, z, yaw, pitch, stance, flags = r[0], r[1], r[2], r[3], r[4] / 100.0, r[5] / 100.0, r[6], r[7]
            alive = bool(flags & F_ALIVE)
            p = prev.get(aid)
            if alive and p and p[3] and 0 < t - p[0] <= 0.2:
                b = brain_at(aid, t)
                if not (b and b['visible']):
                    rate = (abs(_wrap(yaw - p[1])) + abs(pitch - p[2])) / (t - p[0])
                    wander[kinds.get(aid, 'bot')].append(rate)
                if b and any(b['aim']):
                    want = _yaw_to(b['aim'][0] - x, b['aim'][1] - y)
                    aim_err.append(abs(_wrap(want - yaw)))
            prev[aid] = (t, yaw, pitch, alive)
    # reaction: first visible-target brain sample → first shot by that bot at anyone after it
    shots = {}
    for e in d['events']:
        if e['kind'] == 'shot':
            shots.setdefault(e['actor'], []).append(e['t'])
    reactions = []
    for aid, lst in brain.items():
        seen_from = None; last_tgt = None
        for r in lst:
            if r['visible'] and r['target'] and (seen_from is None or r['target'] != last_tgt):
                seen_from, last_tgt = r['t'], r['target']
                nxt = [s for s in shots.get(aid, []) if seen_from <= s <= seen_from + 5]
                if nxt:
                    reactions.append(nxt[0] - seen_from)
            elif not r['visible']:
                seen_from = None; last_tgt = None
    # decision flips A→B→A within 3 s
    flips = 0; bot_seconds = 0.0
    for aid, lst in brain.items():
        if lst:
            bot_seconds += lst[-1]['t'] - lst[0]['t']
        seq = []
        for r in lst:
            if not seq or seq[-1][1] != r['state']:
                seq.append((r['t'], r['state']))
        for i in range(2, len(seq)):
            if seq[i][1] == seq[i - 2][1] and seq[i][0] - seq[i - 2][0] <= 3.0:
                flips += 1
    # time to primary weapon after revive
    ttp = []; pending = {}
    for e in sorted(d['events'], key=lambda e: e['t']):
        if e['kind'] == 'revive':
            pending[e['actor']] = e['t']
        elif e['kind'] == 'pickup' and e['actor'] in pending:
            cls = d['strings'].get(int(e['value']), '')
            if cls not in PISTOLS:
                ttp.append(e['t'] - pending.pop(e['actor']))
    held = {}
    for t, rows in d['frames']:
        for r in rows:
            held.setdefault(r[0], []).append((t, d['strings'].get(r[11], '')))
    kills_by_weapon = {}
    for e in d['events']:
        if e['kind'] != 'kill':
            continue
        lst = held.get(e['other'], []); w = '?'
        for t, cls in lst:                         # last frame at or before the kill
            if t > e['t']:
                break
            w = cls or '?'
        kills_by_weapon[w] = kills_by_weapon.get(w, 0) + 1
    def med(v):
        return round(median(v), 1) if v else None
    return {
        'aim_wander_deg_s': {k: med(v) for k, v in wander.items()},
        'aim_wander_samples': {k: len(v) for k, v in wander.items()},
        'bot_aim_error_deg': med(aim_err),
        'reaction_s': med(reactions), 'reactions': len(reactions),
        'decision_flips_per_bot_min': round(flips / (bot_seconds / 60.0), 2) if bot_seconds else None,
        'time_to_primary_s': med(ttp), 'primaries': len(ttp),
        'kills_by_weapon': kills_by_weapon,
    }
