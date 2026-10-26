"""Streaming parser for WarsBots trace files (<log>.bots.log, see BotLog.cpp).

Each line is "<seconds> <message>". Only messages that carry an actor identity are
attributed to a bot. Several high-volume lines (MOTION INTENT, ABILITY PHYSICS,
BOT SUIT, JUMP TAKEOFF/LANDING, SPRINT, Motion heading, AI decision) carry NO actor
id; they are counted as `unattributed` and never guessed onto a bot.
"""
from __future__ import annotations

import re
from collections import Counter
from dataclasses import dataclass, field

F = r"(-?[\d.]+)"
V3 = rf"\(\s*{F},\s*{F},\s*{F}\)"

PATTERNS = {
    "created": re.compile(rf"#(\d+) created EntityId=(\d+) name=(.+?) profile=(\w+)/(\w+) position={V3}"),
    "telemetry": re.compile(rf"AI telemetry EntityId=(\d+) state=(\w+) pos={V3} health=(-?\d+) weapon=(\d+) ammo=(-?\d+) target=(\d+)"),
    "nav_fallback": re.compile(rf"#(\d+) NAV fallback start={V3} goal={V3}"),
    "nav_build": re.compile(rf"NAV build started: (\d+)x(\d+) columns, cell {F} m, z {F}\.\.{F}"),
    "nav_ready": re.compile(r"NAV ready: (\d+) nodes, (\d+) edges"),
    "mem_loaded": re.compile(rf"MAP MEMORY: loaded (\S+) \(stuck {F}, kills {F}, deaths {F}\)"),
    "escape": re.compile(rf"PROGRESS ESCAPE actor=(\d+) kind=(\w+) goal=(\d+) pos={V3} age={F}"),
    "death": re.compile(r"#(\d+) death observed EntityId=(\d+) health=(-?\d+) lastHitShooter=(\d+)"),
    "kill": re.compile(r"#(\d+) combat kill shooter=(\d+) target=(\d+)"),
    "unreachable": re.compile(r"#(\d+) weapon (\d+) unreachable; skipping"),
    "weapon_cancel": re.compile(r"#(\d+) weapon goal cancelled item=(\d+) reason=(\w+)"),
    "chase_abandon": re.compile(r"#(\d+) chase abandoned target=(\d+)"),
    "pickup": re.compile(r"REAL PICKUP actor=(\d+) item=(\d+) acquired=(\d)"),
    "revive_req": re.compile(rf"#(\d+) revive requested EntityId=(\d+) spawnId=(\d+) previousSpawn=(\d+) position={V3}"),
    "tactic_sample": re.compile(rf"TACTIC SAMPLE actor=(\d+) skill=(\w+) style=(\w+) weapon=(\S+) suit=(\d+) distance={F} action=(\w+) target=(\d+) visible=(\d)"),
    "tactic_withdraw": re.compile(rf"TACTIC actor=(\d+) skill=(\w+) style=(\w+) action=withdraw health=(-?\d+) energy={F} distance={F} visible=(\d)"),
    "los_blocked": re.compile(rf"LOS blocked target=(\d+) obstruction=(\d+) eyes={V3} chest={V3}"),
    "reset": re.compile(r"Game reset: restored (\d+)/(\d+)"),
    "profile": re.compile(rf"PROFILE bots=(\d+) avgSum={F}ms/frame worstFrame={F}ms"),
    "build": re.compile(r"BotManager initialized \(build (\S+)\)"),
}
# Emitted by BotBrain::Update right after "AI telemetry EntityId=X" in the same call
# (BotBrain.cpp, telemetry block): attributable to X by adjacency at the same timestamp.
BLOCK = {
    "motion_intent": re.compile(rf"MOTION INTENT: gait=(\d+) strafe=(\d) requestedBase={F} actual={F} lateral={F} facingDot={F} blocked=(\d)"),
    "ability": re.compile(rf"ABILITY PHYSICS: speed={F} vz={F} grounded=(\d) sprint=(\d) mode=(\d+) energy={F}"),
}
BLOCK_PASSIVE = ("TACTIC SAMPLE", "AI decision", "Motion heading")
# Emitted elsewhere with no actor id at all; counted, never guessed onto a bot.
UNATTRIBUTED = ("BOT SUIT", "JUMP TAKEOFF", "JUMP LANDING", "BOT ACTION", "Stock weapon burst", "ACTION REFUSED")
LINE = re.compile(r"^(-?[\d.]+) (.*)$")


@dataclass
class Bot:
    slot: int
    entity: int
    style: str
    skill: str


@dataclass
class Run:
    """One server session; a backwards timestamp starts a new Run."""
    index: int
    build: str | None = None
    grid: dict | None = None
    nav: dict | None = None
    memory_loaded: dict | None = None
    bots: dict = field(default_factory=dict)          # entity -> Bot
    slot_entity: dict = field(default_factory=dict)   # slot -> current entity
    events: list = field(default_factory=list)        # (kind, t, data)
    unattributed: Counter = field(default_factory=Counter)
    start: float = 0.0
    end: float = 0.0
    resets: int = 0

    def bot_of_slot(self, slot):
        e = self.slot_entity.get(slot)
        return self.bots.get(e) if e is not None else None


def _v(m, i):
    return (float(m[i]), float(m[i + 1]), float(m[i + 2]))


def parse_lines(lines):
    runs = []
    run = None
    last_t = None
    ctx = None  # (entity, t) of the telemetry block being read
    for raw in lines:
        lm = LINE.match(raw.rstrip("\n"))
        if not lm:
            continue
        t = float(lm[1]); msg = lm[2]
        if run is None or (last_t is not None and t + 1.0 < last_t):
            run = Run(index=len(runs), start=t); runs.append(run)
        last_t = t; run.end = t
        if not msg.startswith("[WarsBots"):
            continue
        if ctx and ctx[1] == t:
            done = False
            for kind, rx in BLOCK.items():
                m = rx.search(msg)
                if m:
                    _block(run, kind, ctx[0], t, m); done = True; break
            if done:
                continue
            if any(b in msg for b in BLOCK_PASSIVE) and "TACTIC SAMPLE" not in msg:
                continue
        hit = False
        for kind, rx in PATTERNS.items():
            m = rx.search(msg)
            if not m:
                continue
            hit = True
            _apply(run, kind, t, m)
            if kind == "telemetry":
                ctx = (int(m[1]), t)
            elif kind != "tactic_sample":
                ctx = None
            break
        if not hit:
            if any(b in msg for b in BLOCK) or "MOTION INTENT" in msg or "ABILITY PHYSICS" in msg:
                run.unattributed["block line outside telemetry block"] += 1
                continue
            for u in UNATTRIBUTED:
                if u in msg:
                    run.unattributed[u] += 1
                    break
    return runs


def _block(run, kind, entity, t, m):
    if kind == "motion_intent":
        run.events.append(("motion", t, dict(entity=entity, requested=float(m[3]), actual=float(m[4]),
                                             blocked=m[7] == "1", strafe=m[2] == "1")))
    else:
        run.events.append(("ability", t, dict(entity=entity, speed=float(m[1]), grounded=m[3] == "1",
                                              mode=int(m[5]), energy=float(m[6]))))


def _apply(run, kind, t, m):
    ev = run.events.append
    if kind == "created":
        slot, ent = int(m[1]), int(m[2])
        run.bots[ent] = Bot(slot, ent, m[4], m[5])
        run.slot_entity[slot] = ent
        ev(("spawn", t, dict(entity=ent, slot=slot, pos=_v(m, 6))))
    elif kind == "telemetry":
        ev(("telemetry", t, dict(entity=int(m[1]), state=m[2], pos=_v(m, 3), health=int(m[6]),
                                 weapon=int(m[7]), ammo=int(m[8]), target=int(m[9]))))
    elif kind == "nav_fallback":
        slot = int(m[1]); bot = run.bot_of_slot(slot)
        ev(("nav_fallback", t, dict(slot=slot, entity=bot.entity if bot else None,
                                    start=_v(m, 2), goal=_v(m, 5))))
    elif kind == "nav_build":
        run.grid = dict(nx=int(m[1]), ny=int(m[2]), cell=float(m[3]), zmin=float(m[4]), zmax=float(m[5]))
    elif kind == "nav_ready":
        run.nav = dict(nodes=int(m[1]), edges=int(m[2]))
    elif kind == "mem_loaded":
        run.memory_loaded = dict(level=m[1], stuck=float(m[2]), kills=float(m[3]), deaths=float(m[4]))
    elif kind == "escape":
        ev(("escape", t, dict(entity=int(m[1]), kind=m[2], goal=int(m[3]), pos=_v(m, 4), age=float(m[7]))))
    elif kind == "death":
        ev(("death", t, dict(slot=int(m[1]), entity=int(m[2]), shooter=int(m[4]))))
    elif kind == "kill":
        ev(("kill", t, dict(slot=int(m[1]), shooter=int(m[2]), target=int(m[3]))))
    elif kind == "unreachable":
        bot = run.bot_of_slot(int(m[1]))
        ev(("unreachable", t, dict(slot=int(m[1]), entity=bot.entity if bot else None, item=int(m[2]))))
    elif kind == "weapon_cancel":
        ev(("weapon_cancel", t, dict(slot=int(m[1]), item=int(m[2]), reason=m[3])))
    elif kind == "chase_abandon":
        bot = run.bot_of_slot(int(m[1]))
        ev(("chase_abandon", t, dict(slot=int(m[1]), entity=bot.entity if bot else None, target=int(m[2]))))
    elif kind == "pickup":
        ev(("pickup", t, dict(entity=int(m[1]), item=int(m[2]), acquired=m[3] == "1")))
    elif kind == "revive_req":
        ev(("revive", t, dict(slot=int(m[1]), entity=int(m[2]), spawn=int(m[3]), pos=_v(m, 5))))
    elif kind == "tactic_sample":
        ev(("tactic", t, dict(entity=int(m[1]), skill=m[2], style=m[3], weapon=m[4], suit=int(m[5]),
                              distance=float(m[6]), action=m[7], target=int(m[8]), visible=m[9] == "1")))
    elif kind == "tactic_withdraw":
        ev(("withdraw", t, dict(entity=int(m[1]), health=int(m[4]), energy=float(m[5]),
                                distance=float(m[6]), visible=m[7] == "1")))
    elif kind == "los_blocked":
        ev(("los_blocked", t, dict(target=int(m[1]), eyes=_v(m, 3), chest=_v(m, 6))))
    elif kind == "reset":
        run.resets += 1
        ev(("reset", t, dict(restored=int(m[1]), total=int(m[2]))))
    elif kind == "profile":
        ev(("profile", t, dict(bots=int(m[1]), avg_ms=float(m[2]), worst_ms=float(m[3]))))
    elif kind == "build":
        run.build = m[1]


def parse_file(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return parse_lines(f)
