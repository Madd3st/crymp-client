"""Offline analyses over parsed WarsBots runs. Observational only: nothing here is a
causal claim, and nothing here is training. Thresholds are explicit parameters."""
from __future__ import annotations

import math
from collections import Counter, defaultdict

MOVING_STATES = {"MOVING_TO_WEAPON", "SEARCHING_ENEMY", "CHASING_ENEMY", "SEEKING_WEAPON"}
TELEPORT_M = 15.0  # horizontal jump between samples => respawn/teleport, split trajectory


def _d2(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])


def _d3(a, b):
    return math.sqrt((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2 + (a[2] - b[2]) ** 2)


def trajectories(run):
    """entity -> list of segments; a segment is a list of (t, pos, state, health)."""
    out = defaultdict(list)
    cur = {}
    for kind, t, d in run.events:
        if kind == "spawn" or kind == "revive":
            cur.pop(d["entity"], None)
            continue
        if kind != "telemetry":
            continue
        e = d["entity"]
        if d["health"] <= 0:
            cur.pop(e, None)
            continue
        p = (t, d["pos"], d["state"], d["health"])
        seg = cur.get(e)
        if seg and (_d2(seg[-1][1], d["pos"]) > TELEPORT_M or t - seg[-1][0] > 5.0):
            seg = None
        if seg is None:
            seg = []; out[e].append(seg); cur[e] = seg
        seg.append(p)
    return out


def sample_period(run):
    dts = []
    last = {}
    for kind, t, d in run.events:
        if kind == "telemetry":
            e = d["entity"]
            if e in last and 0 < t - last[e] < 5:
                dts.append(t - last[e])
            last[e] = t
    dts.sort()
    return dts[len(dts) // 2] if dts else None


def stalls(run, min_seconds=4.0, max_net=1.0, max_path=1.6):
    """Windows where a live bot in a moving state did not move.

    A stall is >= min_seconds of consecutive moving-state samples whose net horizontal
    displacement stays < max_net and total path < max_path. Standing still while
    ATTACKING is legitimate and is excluded by the state filter.
    """
    found = []
    for ent, segs in trajectories(run).items():
        for seg in segs:
            i = 0
            n = len(seg)
            while i < n:
                if seg[i][2] not in MOVING_STATES:
                    i += 1; continue
                j = i; path = 0.0
                while j + 1 < n and seg[j + 1][2] in MOVING_STATES:
                    step = _d2(seg[j][1], seg[j + 1][1])
                    if _d2(seg[i][1], seg[j + 1][1]) >= max_net or path + step >= max_path:
                        break
                    path += step; j += 1
                dur = seg[j][0] - seg[i][0]
                if dur >= min_seconds:
                    states = Counter(s[2] for s in seg[i:j + 1])
                    found.append(dict(entity=ent, start=seg[i][0], seconds=round(dur, 2),
                                      pos=seg[i][1], path=round(path, 2),
                                      state=states.most_common(1)[0][0]))
                    i = j + 1
                else:
                    i += 1
    return found


def motion_index(run):
    return {(d["entity"], t): d for k, t, d in run.events if k == "motion"}


def _motion_stats(run, entity, t0, t1, idx=None):
    idx = idx if idx is not None else motion_index(run)
    rows = [d for (e, t), d in idx.items() if e == entity and t0 <= t <= t1]
    if not rows:
        return dict(motion_samples=0)
    req = [d for d in rows if d["requested"] > 0]
    return dict(motion_samples=len(rows),
                requested_move_percent=round(100 * len(req) / len(rows), 1),
                blocked_when_requested_percent=round(100 * sum(d["blocked"] for d in req) / len(req), 1) if req else None,
                mean_actual_speed=round(sum(d["actual"] for d in rows) / len(rows), 2))


def pinned(run, min_samples=5, max_path=0.3):
    """Whole live trajectory segments that never moved (path < max_path metres).

    Distinct from `stalls`: a pinned segment is a bot that did not move at all for
    its whole continuous life segment, in ANY state, including ATTACKING.
    """
    out = []
    idx = motion_index(run)
    for ent, segs in trajectories(run).items():
        for s in segs:
            if len(s) < min_samples:
                continue
            path = sum(_d2(a[1], b[1]) for a, b in zip(s, s[1:]))
            if path < max_path:
                out.append(dict(entity=ent, start=s[0][0], seconds=round(s[-1][0] - s[0][0], 1),
                                pos=s[0][1], path=round(path, 3), samples=len(s),
                                state=Counter(x[2] for x in s).most_common(1)[0][0],
                                **_motion_stats(run, ent, s[0][0], s[-1][0], idx)))
    return out


def traps(run, min_seconds=60.0):
    """Long stalls (default >= 60 s): a bot that is alive, wants to move and cannot."""
    idx = motion_index(run)
    return [dict(s, **_motion_stats(run, s["entity"], s["start"], s["start"] + s["seconds"], idx))
            for s in stalls(run) if s["seconds"] >= min_seconds]


def oscillations(run, window=6.0, min_path=4.0, max_net=1.5):
    """Bots that walked >= min_path but ended < max_net from where they began over `window` s."""
    found = []
    for ent, segs in trajectories(run).items():
        for seg in segs:
            i = 0
            while i < len(seg):
                j = i
                while j + 1 < len(seg) and seg[j + 1][0] - seg[i][0] <= window:
                    j += 1
                if seg[j][0] - seg[i][0] >= window * 0.8 and all(s[2] in MOVING_STATES for s in seg[i:j + 1]):
                    path = sum(_d2(a[1], b[1]) for a, b in zip(seg[i:j], seg[i + 1:j + 1]))
                    if path >= min_path and _d2(seg[i][1], seg[j][1]) < max_net:
                        found.append(dict(entity=ent, start=seg[i][0], seconds=round(seg[j][0] - seg[i][0], 2),
                                          pos=seg[i][1], path=round(path, 2), state=seg[i][2]))
                        i = j + 1; continue
                i += 1
    return found


def fallback_profile(run):
    """Classify NAV fallback queries (FindPath == false) by geometry of the request.

    The log does not say WHY FindPath failed. BotNav::FindPath fails when the start or
    goal has no projectable node (Nearest == -1: no node within 2 columns, |dz|>2.5,
    or the 0.9 m ray to every candidate is blocked), when A* exhausts 6000 expansions,
    or when the graph is disconnected. Short queries can only plausibly be projection
    failures, because Nearest searches +-2 columns (~6-7.5 m).
    """
    rows = [d for k, t, d in run.events if k == "nav_fallback"]
    c = Counter()
    goals = Counter(); starts = Counter()
    for d in rows:
        h = _d2(d["start"], d["goal"]); dz = d["goal"][2] - d["start"][2]
        c["total"] += 1
        if h < 3.0: c["under_3m"] += 1
        elif h < 7.5: c["3_to_7.5m"] += 1
        elif h < 40: c["7.5_to_40m"] += 1
        else: c["over_40m"] += 1
        if abs(dz) > 2.5: c["dz_over_2.5"] += 1
        goals[_bin(d["goal"])] += 1
        starts[_bin(d["start"])] += 1
    return dict(counts=dict(c),
                top_goals=[dict(pos=k, queries=v) for k, v in goals.most_common(12)],
                top_starts=[dict(pos=k, queries=v) for k, v in starts.most_common(12)])


def _bin(p, cell=3.0):
    return (round(p[0] / cell) * cell, round(p[1] / cell) * cell, round(p[2]))


def cluster(points, cell=3.0, top=15):
    c = Counter(); sec = Counter(); states = defaultdict(Counter)
    for p in points:
        k = _bin(p["pos"], cell)
        c[k] += 1; sec[k] += p.get("seconds", 0); states[k][p.get("state", "?")] += 1
    return [dict(pos=k, events=v, seconds=round(sec[k], 1), states=dict(states[k]))
            for k, v in c.most_common(top)]


def occupancy(run, grid):
    """Visited BotNav columns (ix, iy) and z layers, from live telemetry positions."""
    if not grid:
        return None
    cols = Counter(); layers = defaultdict(set)
    for kind, t, d in run.events:
        if kind == "telemetry" and d["health"] > 0:
            ix = int((d["pos"][0] - grid["min_x"]) / grid["cell"])
            iy = int((d["pos"][1] - grid["min_y"]) / grid["cell"])
            cols[(ix, iy)] += 1; layers[(ix, iy)].add(round(d["pos"][2] / 2.5))
    return dict(columns_visited=len(cols), columns_total=grid["nx"] * grid["ny"],
                multi_floor_columns=sum(1 for v in layers.values() if len(v) > 1))


def dataset_quality(run):
    """Checks the existing TACTIC SAMPLE stream against what a contextual bandit needs."""
    rows = [d for k, t, d in run.events if k == "tactic"]
    tele_states = Counter(d["state"] for k, t, d in run.events if k == "telemetry")
    act = Counter(r["action"] for r in rows)
    known_states = set(tele_states) | {"DEAD", "RESPAWNING"}
    action_is_state = sum(v for a, v in act.items() if a in known_states)
    bots = {b.entity for b in run.bots.values()}
    unknown_actor = sum(1 for r in rows if r["entity"] not in bots)
    targeted = [r for r in rows if r["target"]]
    issues = []
    if rows and action_is_state / len(rows) > 0.9:
        issues.append("TACTIC SAMPLE action= is the FSM state name, not a chosen tactical action; "
                      "no alternatives or propensities are logged, so off-policy bandit "
                      "evaluation is impossible from these rows.")
    if any(r["action"] == "DEAD" for r in rows):
        issues.append("samples are emitted while DEAD; they must be dropped or marked as non-decisions.")
    if run.unattributed:
        issues.append("BOT SUIT / JUMP / SPRINT / burst lines carry no actor id "
                      f"({sum(run.unattributed.values())} lines); per-bot suit and jump features cannot be joined. "
                      "MOTION INTENT and ABILITY PHYSICS are joined by adjacency to the preceding telemetry line.")
    return dict(samples=len(rows), actions=dict(act),
                action_equals_state_percent=round(100 * action_is_state / len(rows), 1) if rows else None,
                with_target=len(targeted),
                target_visible_percent=round(100 * sum(r["visible"] for r in targeted) / len(targeted), 1) if targeted else None,
                unknown_actor_rows=unknown_actor,
                withdraw_events=sum(1 for k, t, d in run.events if k == "withdraw"),
                unattributed_lines=dict(run.unattributed), issues=issues)


def summarize(run, grid=None):
    period = sample_period(run)
    st = stalls(run); osc = oscillations(run)
    bot_seconds = 0.0
    for segs in trajectories(run).values():
        for s in segs:
            bot_seconds += s[-1][0] - s[0][0]
    fb = fallback_profile(run)
    pins = pinned(run); tr = traps(run)
    minutes = bot_seconds / 60 if bot_seconds else None
    events = Counter(k for k, t, d in run.events)
    return dict(
        run=run.index, build=run.build, seconds=round(run.end - run.start, 1),
        bots=len({b.slot for b in run.bots.values()}), resets=run.resets,
        profiles=sorted({f"{b.style}/{b.skill}" for b in run.bots.values()}),
        nav=run.nav, grid_from_log=run.grid, memory_loaded=run.memory_loaded,
        telemetry_period_s=period, live_bot_minutes=round(minutes, 1) if minutes else None,
        events=dict(events),
        stalls=dict(count=len(st), seconds=round(sum(s["seconds"] for s in st), 1),
                    per_bot_minute=round(len(st) / minutes, 3) if minutes else None,
                    by_state=dict(Counter(s["state"] for s in st)), clusters=cluster(st)),
        pinned=dict(count=len(pins), bot_minutes=round(sum(p["seconds"] for p in pins) / 60, 1),
                    live_percent=round(100 * sum(p["seconds"] for p in pins) / bot_seconds, 1) if bot_seconds else None,
                    clusters=cluster(pins)),
        traps=dict(count=len(tr), bot_minutes=round(sum(p["seconds"] for p in tr) / 60, 1),
                   live_percent=round(100 * sum(p["seconds"] for p in tr) / bot_seconds, 1) if bot_seconds else None,
                   worst=sorted(tr, key=lambda s: -s["seconds"])[:10]),
        oscillations=dict(count=len(osc), per_bot_minute=round(len(osc) / minutes, 3) if minutes else None,
                          by_state=dict(Counter(s["state"] for s in osc)), clusters=cluster(osc)),
        nav_fallback=dict(fb, per_bot_minute=round(fb["counts"].get("total", 0) / minutes, 2) if minutes else None),
        unreachable_weapons=dict(Counter(d["item"] for k, t, d in run.events if k == "unreachable").most_common(10)),
        occupancy=occupancy(run, grid),
        dataset=dataset_quality(run),
    ), st, osc
