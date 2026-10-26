#!/usr/bin/env python3
"""Dumbness detectors over a WarsBots *.bots.log (DS-032 / DS-037 #5).

Read-only. Produces dumbness.json; the PlaytestReport can render it. The detectors are deliberately
conservative: every rule has a false-positive twin in Docs/Research/DeepSeek/DUMBNESS_DETECTORS.md.

Usage:
    python detectors.py <Server.bots.log> [--out dumbness.json] [--marks marks.json]

Only the log lines that already exist are used. Lines without an actor id (`MOTION INTENT`, `AI decision`)
cannot be attributed to a bot; the affected detectors use MOVE TRACE / TARGET CHOICE / FIRE HIT instead and
say so in the event confidence.
"""
from __future__ import annotations

import argparse
import bisect
import json
import math
import re
from collections import defaultdict
from dataclasses import dataclass, field
from typing import Dict, Iterable, List, Optional, Tuple

TIME_RE = re.compile(r"^([0-9.]+)")
CREATED_RE = re.compile(r"#(\d+) created EntityId=(\d+) name=.*? profile=\S+/(\w+)")
STATE_RE = re.compile(r"#(\d+) AI state=([A-Z_]+)")
MOVE_RE = re.compile(r"MOVE TRACE actor=(\d+).*pos=\(([^)]+)\).*vel=\(([^)]+)\).*speed=([0-9.]+).*vz=([-0-9.]+)")
TRAP_RE = re.compile(r"TRAP RECOVERY level=(\d+) actor=(\d+)")
TARGET_RE = re.compile(r"TARGET CHOICE actor=(\d+) old=(\d+) target=(\d+).*visible=(\d)")
SHOT_RE = re.compile(r"FIRE SHOT shooter=(\d+) target=(\d+).*class=(\w+)")
HIT_RE = re.compile(r"FIRE HIT phase=before-Lua target=(\d+) shooter=(\d+).*distance=([0-9.]+).*head=(\d)")
LOOK_RE = re.compile(r"LOOK TRACE actor=(\d+).*clear=(\d)")
KILL_RE = re.compile(r"GameRules::KillPlayer target=(\d+) killer=(\d+)")
PICKUP_RE = re.compile(r"REAL PICKUP actor=(\d+)")
MARK_RE = re.compile(r"OWNER MARK t=([0-9.]+).*?(?:text=\"([^\"]*)\")?")
VANTAGE_RE = re.compile(r"VANTAGE .*actor=(\d+)")

BANDS = ((0.0, 10.0, "0-10"), (10.0, 20.0, "10-20"), (20.0, 30.0, "20-30"), (30.0, 50.0, "30-50"), (50.0, 1e9, "50+"))
FIRE_STATES = {"ATTACKING", "CHASING_ENEMY", "MOVING_TO_WEAPON"}
CALM_STATES = {"HOLD", "RETREAT"}


def band(d: float) -> str:
    for lo, hi, name in BANDS:
        if lo <= d < hi:
            return name
    return "50+"


@dataclass
class Move:
    t: float
    actor: int
    x: float
    y: float
    z: float
    vx: float
    vy: float
    vz: float
    speed: float


@dataclass
class Event:
    type: str
    t: float
    actor: int
    bot: str = ""
    severity: str = "medium"
    confidence: str = "medium"
    window: Tuple[float, float] = (0.0, 0.0)
    evidence: List[dict] = field(default_factory=list)
    false_positive_twin: str = ""
    mark_id: Optional[int] = None

    def as_dict(self) -> dict:
        return {
            "type": self.type, "t": round(self.t, 2), "actor": self.actor, "bot": self.bot,
            "severity": self.severity, "confidence": self.confidence,
            "window": [round(self.window[0], 2), round(self.window[1], 2)],
            "evidence": self.evidence, "falsePositiveTwin": self.false_positive_twin,
            "markId": self.mark_id,
        }


class Session:
    def __init__(self) -> None:
        self.bot_name: Dict[int, str] = {}
        self.bot_skill: Dict[int, str] = {}
        self.number: Dict[int, int] = {}
        self.state: Dict[int, List[Tuple[float, str]]] = defaultdict(list)
        self.moves: Dict[int, List[Move]] = defaultdict(list)
        self.targets: Dict[int, List[Tuple[float, int, int, int]]] = defaultdict(list)
        self.shots: Dict[int, List[Tuple[float, int, str]]] = defaultdict(list)
        self.hits: Dict[int, List[Tuple[float, int, float, int]]] = defaultdict(list)       # by shooter
        self.victim_hits: Dict[int, List[Tuple[float, int, float, int]]] = defaultdict(list)  # by target
        self.look: Dict[int, List[Tuple[float, int]]] = defaultdict(list)
        self.traps: Dict[int, List[float]] = defaultdict(list)
        self.kills: List[Tuple[float, int, int]] = []
        self.pickups: Dict[int, List[float]] = defaultdict(list)
        self.vantage: Dict[int, List[float]] = defaultdict(list)
        self.marks: List[Tuple[float, str, int]] = []
        self.lines: List[Tuple[int, float, str]] = []
        self.move_times: Dict[int, List[float]] = {}
        self.move_speeds: Dict[int, List[float]] = {}

    def state_at(self, actor: int, t: float) -> str:
        cur = ""
        for ts, s in self.state.get(actor, []):
            if ts <= t + 1e-6:
                cur = s
            else:
                break
        return cur

    def speed_at(self, actor: int, t: float) -> float:
        times = self.move_times.get(actor)
        if not times:
            return 0.0
        i = bisect.bisect_right(times, t + 1e-6) - 1
        return self.move_speeds[actor][i] if i >= 0 else 0.0


def parse(lines: Iterable[str]) -> Session:
    s = Session()
    number_of: Dict[int, int] = {}
    current: Dict[int, int] = {}
    for ln, raw in enumerate(lines, 1):
        line = raw.rstrip("\n")
        m = TIME_RE.match(line)
        if not m:
            continue
        t = float(m.group(1))
        s.lines.append((ln, t, line))
        m = CREATED_RE.search(line)
        if m:
            n, eid, skill = int(m.group(1)), int(m.group(2)), m.group(3)
            s.number[eid] = n
            s.bot_name[eid] = "WarsBot_%02d" % n
            s.bot_skill[eid] = skill
            current[n] = eid
            continue
        m = STATE_RE.search(line)
        if m:
            n = int(m.group(1))
            if n in current:
                s.state[current[n]].append((t, m.group(2)))
            continue
        m = MOVE_RE.search(line)
        if m:
            eid = int(m.group(1))
            p = [float(v) for v in m.group(2).split(",")]
            v = [float(x) for x in m.group(3).split(",")]
            s.moves[eid].append(Move(t, eid, p[0], p[1], p[2], v[0], v[1], float(m.group(5)), float(m.group(4))))
            continue
        m = TRAP_RE.search(line)
        if m:
            s.traps[int(m.group(2))].append(t)
            continue
        m = TARGET_RE.search(line)
        if m:
            s.targets[int(m.group(1))].append((t, int(m.group(2)), int(m.group(3)), int(m.group(4))))
            continue
        m = SHOT_RE.search(line)
        if m:
            s.shots[int(m.group(1))].append((t, int(m.group(2)), m.group(3)))
            continue
        m = HIT_RE.search(line)
        if m:
            victim, shooter, dist, head = int(m.group(1)), int(m.group(2)), float(m.group(3)), int(m.group(4))
            s.hits[shooter].append((t, victim, dist, head))
            s.victim_hits[victim].append((t, shooter, dist, head))
            continue
        m = LOOK_RE.search(line)
        if m:
            s.look[int(m.group(1))].append((t, int(m.group(2))))
            continue
        m = KILL_RE.search(line)
        if m:
            s.kills.append((t, int(m.group(2)), int(m.group(1))))
            continue
        m = PICKUP_RE.search(line)
        if m:
            s.pickups[int(m.group(1))].append(t)
            continue
        m = VANTAGE_RE.search(line)
        if m:
            s.vantage[int(m.group(1))].append(t)
            continue
        m = MARK_RE.search(line)
        if m:
            s.marks.append((float(m.group(1)), m.group(2) or "", ln))
    for actor, moves in s.moves.items():
        s.move_times[actor] = [m.t for m in moves]
        s.move_speeds[actor] = [m.speed for m in moves]
    return s


def _ev(node: Session, typ: str, t: float, actor: int, evidence_lines: List[Tuple[int, str]],
        severity: str = "medium", confidence: str = "medium", twin: str = "", window: float = 5.0) -> Event:
    return Event(typ, t, actor, node.bot_name.get(actor, str(actor)), severity, confidence,
                 (t - window, t + window),
                 [{"t": round(ts, 2), "line": ln, "text": txt[:240]} for ts, ln, txt in evidence_lines],
                 twin)


def detect_oscillation(s: Session) -> List[Event]:
    out: List[Event] = []
    for actor, moves in s.moves.items():
        window: List[Move] = []
        for m in moves:
            window.append(m)
            while window and m.t - window[0].t > 2.0:
                window.pop(0)
            rev = 0
            for a, b in zip(window, window[1:]):
                if b.t - a.t <= 0.6 and a.speed > 1.0 and b.speed > 1.0 and (a.vx * b.vx + a.vy * b.vy) < 0:
                    rev += 1
            if rev >= 4 and s.state_at(actor, m.t) not in ("ATTACKING", "HOLD"):
                out.append(_ev(s, "oscillation", m.t, actor,
                               [(int(m.t), 0, f"reversals={rev} in 2 s, state={s.state_at(actor, m.t)}")],
                               "high" if rev >= 6 else "medium", "medium", "duel strafe / single corner turn"))
                break
    return out


def detect_flapping(s: Session) -> List[Event]:
    out: List[Event] = []
    kills = s.kills
    for actor, choices in s.targets.items():
        for i in range(2, len(choices)):
            a, b, c = choices[i - 2], choices[i - 1], choices[i]
            if c[0] - a[0] <= 2.0 and b[3] == 1 and c[3] == 1 and b[1] != 0 and b[2] != b[1] and a[2] == c[2]:
                if any(k[1] == actor and k[2] == b[1] and abs(k[0] - b[0]) <= 2.0 for k in kills):
                    continue
                out.append(_ev(s, "flapping", c[0], actor,
                               [(int(a[0]), 0, f"target {a[2]} -> {b[2]} -> {c[2]}"), (int(c[0]), 0, "3 changes in 2 s")],
                               "medium", "medium", "finish a low-HP target / target death"))
                break
    return out


def detect_freeze(s: Session) -> List[Event]:
    out: List[Event] = []
    for actor, moves in s.moves.items():
        run: List[Move] = []
        for m in moves:
            if m.speed < 0.3 and m.vz <= 1.0:
                run.append(m)
            else:
                run = []
            if run and run[-1].t - run[0].t >= 1.5:
                state = s.state_at(actor, run[-1].t)
                if state in FIRE_STATES:
                    out.append(_ev(s, "freeze", run[-1].t, actor,
                                   [(int(run[0].t), 0, f"speed<0.3 for {run[-1].t - run[0].t:.1f}s state={state}")],
                                   "high", "high", "reload behind cover / jump wind-up / HOLD"))
                    run = []
                    break
    return out


def detect_wall(s: Session) -> List[Event]:
    out: List[Event] = []
    for actor, traps in s.traps.items():
        for i in range(1, len(traps)):
            if traps[i] - traps[i - 1] <= 3.0:
                out.append(_ev(s, "wall", traps[i], actor,
                               [(int(traps[i - 1]), 0, "TRAP RECOVERY"), (int(traps[i]), 0, "second recovery <= 3 s")],
                               "high", "high", "intentional cover hold / door wait"))
                break
    return out


def detect_shoot_cover(s: Session) -> List[Event]:
    out: List[Event] = []
    for actor, shots in s.shots.items():
        for i in range(len(shots)):
            t, target, cls = shots[i]
            recent = [c for ts, c in s.look.get(actor, []) if abs(ts - t) <= 0.5]
            if not recent or recent[-1] != 0:
                continue
            hits = [h for h in s.hits.get(actor, []) if h[1] == target and 0 <= h[0] - t <= 1.5]
            burst = [x for x in shots if 0 <= x[0] - t <= 2.0 and x[1] == target]
            if len(burst) >= 3 and len(hits) <= 1 and cls != "Shotgun":
                out.append(_ev(s, "shoot_cover", t, actor,
                               [(int(t), 0, f"3+ shots at {target}, clear=0, hits={len(hits)}")],
                               "medium", "medium", "thin-wall penetration / shotgun spread"))
                break
    return out


def detect_ignore_visible(s: Session) -> List[Event]:
    out: List[Event] = []
    for actor, choices in s.targets.items():
        for t, old, target, visible in choices:
            if visible != 1 or target == 0:
                continue
            if s.state_at(actor, t) in CALM_STATES:
                continue
            fired = any(0 <= st - t <= 1.0 and tg == target for st, tg, _ in s.shots.get(actor, []))
            reselected = any(0 < st - t <= 1.0 and tg == target for st, _, tg, _ in s.targets.get(actor, []))
            if not fired and not reselected:
                out.append(_ev(s, "ignore_visible", t, actor,
                               [(int(t), 0, f"visible target {target}, no shot/reselect for 1 s")],
                               "medium", "low", "reload / out of range / finishing another target"))
    return out


def detect_open_under_fire(s: Session) -> List[Event]:
    out: List[Event] = []
    for actor, hits in s.victim_hits.items():
        for i in range(1, len(hits)):
            if hits[i][0] - hits[i - 1][0] <= 2.0 and s.speed_at(actor, hits[i][0]) < 0.5:
                if s.state_at(actor, hits[i][0]) in CALM_STATES:
                    continue
                if any(0 <= hits[i][0] - v <= 5.0 for v in s.vantage.get(actor, [])):
                    continue
                out.append(_ev(s, "open_under_fire", hits[i][0], actor,
                               [(int(hits[i - 1][0]), 0, "hit"), (int(hits[i][0]), 0, "second hit, speed<0.5")],
                               "high", "medium", "sniper hold / armor tanking / reload behind cover"))
                break
    return out


def detect_stall(s: Session) -> List[Event]:
    out: List[Event] = []
    for actor, states in s.state.items():
        start = None
        for t, st in states:
            if st == "MOVING_TO_WEAPON":
                if start is None:
                    start = t
                elif t - start > 60.0:
                    if not any(start <= p <= t for p in s.pickups.get(actor, [])):
                        out.append(_ev(s, "stall", t, actor,
                                       [(int(start), 0, "MOVING_TO_WEAPON > 60 s, no pickup")],
                                       "medium", "medium", "long rotation to a distant pickup"))
                        start = None
                        break
            else:
                if start is not None and t - start > 60.0 and not any(start <= p <= t for p in s.pickups.get(actor, [])):
                    out.append(_ev(s, "stall", t, actor,
                                   [(int(start), 0, "MOVING_TO_WEAPON > 60 s, no pickup")],
                                   "medium", "medium", "long rotation to a distant pickup"))
                    start = None
                    break
                start = None
    return out


DETECTORS = (detect_oscillation, detect_flapping, detect_freeze, detect_wall,
             detect_shoot_cover, detect_ignore_visible, detect_open_under_fire, detect_stall)


def detect(s: Session) -> List[Event]:
    events: List[Event] = []
    for fn in DETECTORS:
        events.extend(fn(s))
    events.sort(key=lambda e: e.t)
    for i, mark in enumerate(s.marks, 1):
        mt = mark[0]
        for e in events:
            if abs(e.t - mt) <= 5.0 and e.mark_id is None:
                e.mark_id = i
    return events


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("log")
    ap.add_argument("--out", default="")
    ap.add_argument("--marks", default="", help="optional JSON list of {t,text}")
    args = ap.parse_args()
    with open(args.log, "r", encoding="utf-8", errors="replace") as f:
        s = parse(f)
    if args.marks:
        with open(args.marks, "r", encoding="utf-8") as f:
            for i, m in enumerate(json.load(f), 1):
                s.marks.append((float(m.get("t", 0)), m.get("text", ""), i))
    events = detect(s)
    marks = []
    for i, (mt, text, _ln) in enumerate(s.marks, 1):
        hits = [e for e in events if abs(e.t - mt) <= 5.0]
        marks.append({"id": i, "t": round(mt, 2), "text": text, "hit": bool(hits),
                      "events": [e.type for e in hits], "nearest": round(min((abs(e.t - mt) for e in hits), default=-1), 2)})
    per = defaultdict(lambda: {"events": 0, "marks": 0})
    for e in events:
        per[e.type]["events"] += 1
    for m in marks:
        for typ in set(m["events"]):
            per[typ]["marks"] += 1
    out = {
        "schema": "warsbots-dumbness/1",
        "source": args.log,
        "events": [e.as_dict() for e in events],
        "marks": marks,
        "summary": {"perDetector": dict(per), "marksTotal": len(marks),
                    "marksWithHit": sum(1 for m in marks if m["hit"]),
                    "hitRate": round(sum(1 for m in marks if m["hit"]) / max(1, len(marks)), 3),
                    "eventsPer10Min": round(len(events) / max(1e-9, (s.lines[-1][1] - s.lines[0][1]) / 600.0), 2) if s.lines else 0},
    }
    path = args.out or (args.log.rsplit(".", 1)[0] + ".dumbness.json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=1)
    print("detectors: %s (%d events, %d marks)" % (path, len(events), len(marks)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
