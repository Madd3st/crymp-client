# Why bots twitch and rush (DS-034)

Date: 2026-10-26. Read-only at `claude-sprint` `2ffc95fa`. Log: C-106 owner playtest 3
(`build/owner-playtests/C106-757f3ddc-20261026-162613/…-Server.bots.log`, 36.4 min, 8 bots, owner present 21.7 min).

## Verdict

The twitch is **not** target selection and **not** the goal arbiter. It is the **state writer**: two functions write two
different `AI state` values in the same 10 Hz tick (`UpdateRoam` → `SEARCHING_ENEMY`, then `UpdateUpgrade` →
`MOVING_TO_WEAPON`), and `Change()` has no per-tick guard or dwell. The body then alternates its movement destination
with the state, which shows up as 4.3× the owner's direction reversals.

## Measurements (C-106)

| Metric | Bots | Owner | Ratio |
| --- | ---: | ---: | ---: |
| Direction reversals / min | **34.5 per bot** | **8.06** | 4.3× |
| `AI state` changes / min | 108 per bot | — | — |
| `AI state` A→B→A within 2 s / min | **89.1 per bot** | — | — |
| `ARBITER goal` changes / min | 8.4 per bot | — | — |
| `ARBITER` A→B→A within 2 s / min | 0.44 per bot | — | — |
| `TARGET CHOICE` switches / min | 0.73 per bot | — | — |
| Target A→B→A within 2 s | **0** | — | — |

State transition pairs (whole session):

| Pair | Count |
| --- | ---: |
| `SEARCHING_ENEMY → MOVING_TO_WEAPON` | **13,004** |
| `MOVING_TO_WEAPON → SEARCHING_ENEMY` | **12,999** |
| `CHASING_ENEMY → ATTACKING` | 1,450 |
| `ATTACKING → CHASING_ENEMY` | 1,318 |
| `SEEKING_WEAPON → MOVING_TO_WEAPON` | 375 |
| `MOVING_TO_WEAPON → ATTACKING` | 316 |

The two dominant pairs are the same two states in opposite order, ~45 per bot-minute each. `TARGET CHOICE` never does
A→B→A, so the target hysteresis is working; the problem is the state/movement writer.

## Root cause 1 — `UpdateRoam` calls `UpdateUpgrade` after setting `SEARCHING_ENEMY`

Code (`BotBrain.cpp`):

```
567 void BotBrain::UpdateRoam(...)
570     Change(State::SEARCHING_ENEMY);
...
585     if (BotConfig::Get().tactics==1 && m_upgradeTarget && UpdateUpgrade(..., true)) return;
```

`UpdateUpgrade` starts with `Change(State::MOVING_TO_WEAPON)` at `:546`. In the same tick the log prints both states
with the same timestamp, e.g.:

```
5.89 [WarsBots] #2 AI state=SEARCHING_ENEMY.
5.89 [WarsBots] #2 AI state=MOVING_TO_WEAPON.
```

Why does `m_upgradeTarget` exist when the arbiter did not choose Weapon? `UpdateArbitrated` sets it at `:1327` and only
clears it when the item disappears (`:1329`); it is **not** cleared when `d.goal != Weapon`. `UpdateArbitrated` may
choose Fight/Hunt, fall to `UpdateRoam` (`:1351`), and `UpdateRoam`’s tactics branch then consumes the stale target.
So every tick: SEARCHING, then MOVING; destination = roam point, then weapon; next tick back.

## Root cause 2 — `ATTACKING`/`CHASING_ENEMY` flips at the engage/hold boundary

```
762 if (visible && distance <= (m_state == ATTACKING ? m_profile.holdRange : m_profile.engageRange))
764     UpdateAttacking(...); else UpdateChasing(...);
```

Universal/Normal: `engageRange=18`, `holdRange=22`. A target at 20-21 m moving ±1 m flips the branch each decision;
`UpdateAttacking` strafes/holds, `UpdateChasing` closes, so the bot visibly oscillates. 2,768 transitions in 36 min
(9.5 per bot-minute).

## Root cause 3 — `Change()` has no tick guard and no dwell

`Change()` (`BotBrain.cpp:50-59`) only dedupes equal states. Any caller may change the state every 0.1 s; there is no
"one state per decision" rule and no minimum dwell. The 10 Hz loop (`:316`) means a 0.1 s floor, but nothing prevents
two writers in one tick.

## Root cause 4 — movement destination is rebuilt every tick and written twice

`m_intent=BotIntent()` at `:322`; both `UpdateRoam` and `UpdateUpgrade` call `WBI_MOVETO` in the same tick (last writer
wins). `TravelGait` (`:84-100`) then picks sprint/run from the new distance (`:90`), so direction and gait alternate.
The owner's 8.06 reversals/min is the human baseline; the bots' 34.5 is the body following a destination that changes
every 0.1 s.

## Root cause 5 — gait writers in combat/travel

Top `INTENT WHY` gait sources in C-106: `BotBrain.cpp:998` (obstacle jump) 373, `:851` (jump-strafe) 372, `:1316`
(suit-policy downgrade) 36. These are legitimate micro-decisions, but they overwrite the travel gait because there is
no single owner; the result is extra stop/sprint edges, not state flips.

## Fix

### P0 — one owner for the weapon goal (minimal, no arbiter rewrite)

- In `UpdateArbitrated`, when `d.goal != ArbiterGoal::Weapon`, set `m_upgradeTarget = 0` (or a
  `m_upgradeOwnedByArbiter=false` flag). The bot should not chase a weapon the arbiter did not choose.
- In `UpdateRoam`, move `Change(State::SEARCHING_ENEMY)` **after** the tactics block, or remove the
  `UpdateUpgrade` call and let `Update`/the arbiter own weapon goals. The first five lines of `UpdateRoam` must not
  change state before deciding whether it delegates.

### P1 — commit gate in `Change()`

Use `proto/decision_commit.h`:

```
void BotBrain::Change(State s, bool init, DecisionOwner owner) {
    if (!init && !m_commit.Offer(owner, ToDecision(s), m_now, IsUrgent(s))) {
        if (m_commit.ShouldLogConflict(m_now))
            WB_LOG(1, "#%d STATE CONFLICT first=%s second=%s", ...);
        return;
    }
    ... existing body ...
}
```

- One accepted change per `m_decisionSerial`.
- Non-urgent dwell 0.3 s; `ATTACKING` may not fall back to `CHASING` for 0.5 s.
- Urgent: `DEAD`, `RESPAWNING`, a new visible target → `ATTACKING`.
- `BeginTick(m_decisionSerial, m_now)` at the top of the 10 Hz block.

### P1 — combat band hysteresis

- Use `enter=engageRange`, `leave=holdRange+2` (20/24 for Universal, 30/36 for Elite), and the 0.5 s attack-leave
  dwell. Do not switch to `CHASING` just because the target stepped 1 m outside `holdRange`.
- Keep `TARGET CHOICE` as is: 0 target flips means it is not the problem.

### P2 — travel commitment

- `MoveCommit` (same header): hold the destination for 0.75 m / 2.5 s; only the owning state may replace it.
- Hold sprint/run for 0.5 s; do not recompute `TravelGait` from every new destination.
- Do not let `TravelGait` fire on a zero-length move (`distance < 3` already Run, but the call still writes).

### P2 — conflict observability

- Log `STATE CONFLICT` at most once per second with first/second state + owner; count per bot-minute in
  `summary.json`. This is the metric that proves the fix, not the owner's impression.

## Target metrics

| Metric | C-106 | Target |
| --- | ---: | ---: |
| State A→B→A / bot-min | 89.1 | < 2 |
| Direction reversals / bot-min | 34.5 | < 12 (owner 8.1) |
| `ATTACKING`↔`CHASING` / bot-min | 9.5 | < 2 |
| Goal A→B→A / bot-min | 0.44 | < 0.5 (keep) |
| Target A→B→A | 0 | 0 (keep) |

## Module + tests

`Docs/Research/DeepSeek/proto/decision_commit.h` (engine-free, C++17):

- `DecisionCommit`: one change per tick, 0.3 s dwell, 0.5 s attack-leave, conflict counters, rate-limited log.
- `MoveCommit`: hold a destination until it moves > 0.75 m or 2.5 s.

`decision_commit_test.cpp` covers: first offer, same-tick conflict, same-state writer change, dwell, urgent override,
attack-leave, 100 Hz input gated to ≤ 4 changes/s, move hold, conflict log rate limit. Built with MSVC 19.44 and run:

```
decision_commit: all tests passed
```

Integration is a draft only; no game code was edited.

## Evidence limits

- C-106 is one session, 8 bots, 36 min; the owner was present 21.7 min. Reversals are a proxy for "twitch", not a
  direct perception metric; combine with the owner's marks (DS-032 detectors) before/after.
- The 10 Hz log does not show every write; a conflict counter in `Change()` is needed to measure the exact rate after
  the fix.
- `UPDATE ROAM`/`UPGRADE` order may differ on other builds; re-run the analyzer on the fixed head.
