"""Per-run verdict, schema `warsbots-run-summary/1` (contract: Docs/Research/SteelmillTriage/SUMMARY_CONTRACT.md).

One small JSON per QA run or playtest, comparable across builds. Flat numeric fields only where a
gate can test them; everything heavier stays in the steelmill-triage/1 report.
"""
from __future__ import annotations

import hashlib
import re
from collections import Counter, defaultdict
from pathlib import Path

from . import analysis, logparse, movement

SCHEMA = "warsbots-run-summary/1"
SPAWN_195 = (1247.50, 1319.75)
FROZEN_WINDOW_S = 12.0
FROZEN_MAX_MOVE_M = 0.3

_T = re.compile(r"^([\d.]+) ")
_REVIVE = re.compile(r"revive requested EntityId=(\d+) spawnId=(\d+)")
_TELE = re.compile(r"AI telemetry EntityId=(\d+) state=(\w+) pos=\(([-\d.]+), ([-\d.]+), ([-\d.]+)\)")
_TRAP = re.compile(r"TRAP RECOVERY level=(\d) actor=(\d+)(?: age=[\d.]+ pos=\(([-\d.]+),([-\d.]+),[-\d.]+\))?")
_REASON = re.compile(r"NAV fallback .*reason=(\w+)")


def _pct(a, b):
    return round(100.0 * a / b, 1) if b else None


def revives(lines):
    """Per spawn: revives and how many left the bot frozen (<0.3 m within ~12 s)."""
    pend = {}; per = defaultdict(lambda: [0, 0])
    for ln in lines:
        m = _T.match(ln)
        if not m: continue
        t = float(m.group(1))
        r = _REVIVE.search(ln)
        if r:
            pend[r.group(1)] = (t, int(r.group(2)), []); continue
        e = _TELE.search(ln)
        if e and e.group(1) in pend:
            t0, sp, ps = pend[e.group(1)]
            if t - t0 <= FROZEN_WINDOW_S + 2:
                ps.append((float(e.group(3)), float(e.group(4))))
            else:
                if len(ps) >= 2:
                    moved = ((ps[-1][0] - ps[0][0]) ** 2 + (ps[-1][1] - ps[0][1]) ** 2) ** 0.5
                    per[sp][0] += 1; per[sp][1] += moved < FROZEN_MAX_MOVE_M
                del pend[e.group(1)]
    return {sp: dict(revives=n, frozen=f) for sp, (n, f) in per.items()}


def watchdog(lines):
    """Episodes = level-1 events; level reached; self-kills; episodes at spawn 195."""
    eps = 0; at195 = 0; levels = Counter(); kills = 0
    for ln in lines:
        if "TRAP RECOVERY" not in ln: continue
        if "nativeSelfKill=1" in ln:
            kills += 1; continue
        m = _TRAP.search(ln)
        if not m: continue
        lvl = int(m.group(1)); levels[lvl] += 1
        if lvl == 1:
            eps += 1
            if m.group(3) and abs(float(m.group(3)) - SPAWN_195[0]) < 0.5 and abs(float(m.group(4)) - SPAWN_195[1]) < 0.5:
                at195 += 1
    return dict(episodes=eps, at_spawn195=at195, reached_level={str(k): v for k, v in sorted(levels.items())},
                self_kills=kills)


def build(log_path, graph=None, memory=None, label=None):
    p = Path(log_path)
    raw = p.read_bytes()
    lines = raw.decode("utf-8", errors="replace").splitlines()
    runs = logparse.parse_lines(lines)
    run = max(runs, key=lambda r: r.end - r.start) if runs else None
    s = analysis.summarize(run)[0] if run else {}
    rv = revives(lines)
    tot_rev = sum(v["revives"] for v in rv.values()); tot_frozen = sum(v["frozen"] for v in rv.values())
    s195 = rv.get(195, dict(revives=0, frozen=0))
    reasons = Counter(m.group(1) for ln in lines for m in [_REASON.search(ln)] if m)
    fb_total = s.get("nav_fallback", {}).get("counts", {}).get("total", sum(reasons.values()))
    states, _ = movement.bot_states(lines)
    states = {k: v for k, v in states.items() if not k.startswith('_')}
    moving = sum(v["moving_samples"] for v in states.values())
    over4 = sum(v["measured_speed"]["over_4ms_percent"] * v["moving_samples"] for v in states.values())
    sprint = sum((v["native_sprint_percent"] or 0) * v["moving_samples"] for v in states.values())
    med = sorted(v["measured_speed"]["median"] for v in states.values() if v["moving_samples"])
    out = dict(
        schema=SCHEMA, label=label or p.stem, log=p.name, log_sha256=hashlib.sha256(raw).hexdigest(),
        build=s.get("build"), bots=s.get("bots"), seconds=s.get("seconds"),
        live_bot_minutes=s.get("live_bot_minutes"),
        spawn195=dict(revives=s195["revives"], frozen=s195["frozen"]),
        revives=dict(total=tot_rev, frozen=tot_frozen,
                     frozen_spawns={str(k): v for k, v in sorted(rv.items()) if v["frozen"]}),
        watchdog=watchdog(lines),
        pinned_live_percent=s.get("pinned", {}).get("live_percent"),
        traps_ge60s=s.get("traps", {}).get("count"),
        stalls=s.get("stalls", {}).get("count"),
        nav_fallback=dict(total=fb_total, per_bot_minute=s.get("nav_fallback", {}).get("per_bot_minute"),
                          reasons=dict(reasons), no_route_percent=_pct(reasons.get("no_route", 0), sum(reasons.values()))),
        unreachable_weapon_skips=sum(s.get("unreachable_weapons", {}).values()),
        movement=dict(moving_samples=moving,
                      over_4ms_percent=round(over4 / moving, 1) if moving else None,
                      native_sprint_percent=round(sprint / moving, 1) if moving else None,
                      median_speed_by_state_max=med[-1] if med else None),
        graph=None,
    )
    if graph is not None:
        prof = graph.scc_profile()
        routes = graph.classify_native_routes(lines)
        nr = sum(routes["no_route_goal"].values())
        bad_goal = sum(v for k, v in routes["no_route_goal"].items() if not k.startswith("main"))
        sid, kind = graph._scc
        sp_kind = {r[0]: kind(sid[graph.nearest(r[1:4])]) for r in (graph.data.get("spawns") or [])
                   if graph.nearest(r[1:4]) >= 0}
        on_bad = sum(v["revives"] for k, v in rv.items() if k in sp_kind and not sp_kind[k].startswith("main"))
        out["graph"] = dict(fingerprint=graph.data.get("fingerprint"), nodes=len(graph.nodes), edges=len(graph.to),
                            sink_sccs=prof["sink_sccs"], nodes_in_sinks_percent=_pct(prof["nodes_in_sinks"], len(graph.nodes)),
                            main0_main1_edges=prof["main0_main1_edges"],
                            spawns_outside_main=len(prof.get("spawns_outside_main", [])),
                            revives_on_non_main_spawns_percent=_pct(on_bad, tot_rev),
                            no_route_goal_outside_main_percent=_pct(bad_goal, nr),
                            memory_link="OK" if memory is not None else None)
    return out
