# PlaytestReport event definitions vs log semantics (DS-012 #3)

File: `Tools/PlaytestReport/playtest_report.py` at `666cc9d0`. Input: `*.bots.log` (MOVE TRACE 4 Hz, AI state,
INTENT WHY, HOLD/LOOT/LINK/TRAP, ARBITER).

## stuck (`:145-158`)

Rule: speed <0.3, `grounded`, position drift <1 m, duration ≥3 s; state not in
`DEAD/SPAWNING/ATTACKING/HOLDING`.
Problems:
- `HOLDING` is **not a BotBrain state** (`BotBrain.h:21-22` has `SEARCHING_ENEMY/CHASING_ENEMY/...`); the hold
  episode is inside `CHASING_ENEMY` (`BotBrain::UpdateHoldAngle` sets that state). The exclusion is dead; a deliberate
  angle hold is reported as stuck.
- `LOOTING` is also not a state (`MOVING_STATES:31` lists it, but `stateAt` never returns it); standstill at a pickup
  can be stuck.
- No `intent.move`/`requestedBase` check: a bot intentionally stopping at a pickup/loot is flagged.
Fix: require `MOTION INTENT requestedBase>0` (or `blocked=1`) for the window; exclude ±N s around `HOLD angle from` and
`LOOT item=`; keep `ATTACKING`/`DEAD`/`SPAWNING`. Do not rely on `HOLDING`/`LOOTING` strings.

## dither (`:159-173`)

Rule: ≥3 direction reversals >120° within 6 s, speed >1 m/s, state not `ATTACKING/DEAD/SPAWNING`.
Problems:
- Same dead-state issue: **HOLD** is `CHASING_ENEMY`, so a held angle with small back-and-forth is dither; **LOOT** is
  a roam substate.
- No exclusion of `TARGET CHOICE`/`HOLD`/`LINK` events; a bot turning to a new target or link take-off can produce
  reversals.
Fix: mask intervals after `HOLD angle from` (until visible/target change or state change), after `LOOT item=`, and
around `LINK start`; keep ATTACKING excluded. The question "also exclude HOLD/LOOT?" is **yes**, but via event
intervals, not state names.

## flip (`:174-186`)

Rule: ≥4 `ARBITER goal` changes within 10 s, no reason filter, no normalization.
Problems:
- A monotonic `roam→weapon→fight→hunt→roam` counts as a flip; `reason=GoalGone`/`Start` are expected.
Fix: require A→B→A within 10 s (same as DS-009 decision flips), exclude `GoalGone`/`Start`, and report per bot-minute;
keep the sequence in `detail`.

## Other

- `strength` (`:109-113`) attributes every Strength takeoff to the last `INTENT WHY`; link jumps (line 1059) are
  included. Exclude if `LINK start` is within 1 s, otherwise `jumpSanity` is judged on the wrong set.
- `sniper` = `sniperHold*2` assumes one `TACTIC SAMPLE` per 2 s; missed samples undercount. Use demo/position samples.
- `stuck` requires `grounded`; a bot pinned in air is invisible.
