"""Movement pace profile: bots by state, and a like-for-like bot vs human speed estimate.

Two sources:
  * Telemetry block (AI telemetry + ABILITY PHYSICS + MOTION INTENT, joined by adjacency): per-bot
    measured speed, sprint flag, suit mode, energy, grounded, requested gait. Bots only.
  * `LOS blocked target=X ... chest=(..) sample=T`: positions of whatever a bot looked at,
    humans included. Both humans and bots are measured through this same channel, so the
    comparison is like-for-like even though it is sparse.
Suit modes follow CryMP ENanoMode: 0 Speed, 1 Strength, 2 Cloak, 3 Armor.
"""
from __future__ import annotations

import re
import statistics
from collections import Counter, defaultdict

MODES = {0: "Speed", 1: "Strength", 2: "Cloak", 3: "Armor"}
GAITS = {0: "walk", 1: "run", 2: "sprint"}
LOS = re.compile(r"^([\d.]+) \[WarsBots\] LOS blocked target=(\d+) obstruction=\d+ eyes=\([^)]*\) "
                 r"chest=\(([-\d.]+),([-\d.]+),([-\d.]+)\) sample=(\d+\.\d+)")
HUMAN = re.compile(r"acquired enemy id=(\d+) name=(\S+) kind=human")
MOTION_FULL = re.compile(r"MOTION INTENT: gait=(\d) strafe=(\d) requestedBase=([\d.]+) actual=([\d.]+)")
ABILITY_FULL = re.compile(r"ABILITY PHYSICS: speed=([\d.]+) vz=\S+ grounded=(\d) sprint=(\d) mode=(\d+) energy=([\d.]+)")
TELE = re.compile(r"AI telemetry EntityId=(\d+) state=(\w+) .* health=(-?\d+)")
REFUSED = re.compile(r"ACTION REFUSED: (\w+)")


def _pct(v, q):
    v = sorted(v)
    return round(v[min(len(v) - 1, int(len(v) * q))], 2) if v else None


def _speed_profile(v):
    if not v:
        return None
    return dict(n=len(v), median=round(statistics.median(v), 2), p75=_pct(v, 0.75), p90=_pct(v, 0.90),
                over_4ms_percent=round(100 * sum(x > 4.0 for x in v) / len(v), 1),
                still_percent=round(100 * sum(x < 0.5 for x in v) / len(v), 1))


def bot_states(lines):
    """Per-state pace from the telemetry block. Lines must be one run in order."""
    state = None; alive = False; t_line = None; t_tele = None
    rows = defaultdict(lambda: dict(speed=[], gait=Counter(), strafe=0, moving=0, sprint=0, ability=0,
                                    mode=Counter(), energy=[], grounded=0))
    refused = Counter()
    by_sprint = defaultdict(list)  # (sprint, mode) -> measured ground speed while moving
    for line in lines:
        sp = line.find(" ")
        t_line = line[:sp]
        m = TELE.search(line)
        if m:
            state, alive, t_tele = m[2], int(m[3]) > 0, t_line
            continue
        r = REFUSED.search(line)
        if r:
            refused[r[1]] += 1
        if not alive or state in (None, "DEAD", "RESPAWNING") or t_line != t_tele:
            continue
        m = ABILITY_FULL.search(line)
        if m:
            row = rows[state]; row["ability"] += 1; row["sprint"] += int(m[3]); row["grounded"] += int(m[2])
            row["mode"][MODES.get(int(m[4]), m[4])] += 1; row["energy"].append(float(m[5]))
            if m[2] == "1" and float(m[1]) > 0.3:
                by_sprint[("sprint" if m[3] == "1" else "no_sprint") + "/" + MODES.get(int(m[4]), m[4])].append(float(m[1]))
            continue
        m = MOTION_FULL.search(line)
        if m and float(m[3]) > 0:
            row = rows[state]; row["moving"] += 1; row["gait"][GAITS.get(int(m[1]), m[1])] += 1
            row["strafe"] += int(m[2]); row["speed"].append(float(m[4]))
    out = {}
    for s, r in rows.items():
        if not r["moving"]:
            continue
        out[s] = dict(moving_samples=r["moving"],
                      requested_gait_percent={k: round(100 * v / r["moving"]) for k, v in r["gait"].items()},
                      strafe_percent=round(100 * r["strafe"] / r["moving"]),
                      measured_speed=_speed_profile(r["speed"]),
                      native_sprint_percent=round(100 * r["sprint"] / r["ability"], 1) if r["ability"] else None,
                      grounded_percent=round(100 * r["grounded"] / r["ability"], 1) if r["ability"] else None,
                      suit_mode_percent={k: round(100 * v / r["ability"]) for k, v in r["mode"].most_common()} if r["ability"] else {},
                      energy_median=round(statistics.median(r["energy"])) if r["energy"] else None)
    out["_speed_by_sprint_and_mode"] = {k: _speed_profile(v) for k, v in sorted(by_sprint.items())}
    return out, dict(refused)


def los_speeds(lines, min_dt=0.3, max_dt=1.2, max_speed=15.0):
    """Speed estimates for every observed target from consecutive LOS samples."""
    humans = {}; pts = defaultdict(dict)
    for line in lines:
        h = HUMAN.search(line)
        if h:
            humans[h[1]] = h[2]
        m = LOS.match(line)
        if m:
            pts[m[2]][float(m[6])] = (float(m[3]), float(m[4]))
    def speeds(ids):
        v = []
        for e in ids:
            s = sorted(pts[e].items())
            for (t0, a), (t1, b) in zip(s, s[1:]):
                dt = t1 - t0
                if min_dt <= dt <= max_dt:
                    d = ((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2) ** 0.5 / dt
                    if d < max_speed:
                        v.append(d)
        return v
    return dict(method=f"consecutive LOS samples {min_dt}-{max_dt} s apart, horizontal, < {max_speed} m/s",
                humans={humans[h]: _speed_profile(speeds([h])) for h in humans},
                bots=_speed_profile(speeds([e for e in pts if e not in humans])))


def profile(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        lines = f.read().splitlines()
    states, refused = bot_states(lines)
    return dict(log=str(path), bot_states=states, refusals=refused, like_for_like=los_speeds(lines))
