# DecisionCommit P1 (`wb_bot_state_commit=2`) — DS-043 #2

Date: 2026-10-27. Patch: `patches/ds043_decision_commit_p1.patch` on `0fa9c1cb` (or `ds043_p1_plus_ps.patch`
with the PS revive fix). P0 (`stateCommit=1`) stays exactly as it is.

## What changes

| Piece | P0 (`=1`) | P1 (`=2`) |
| --- | --- | --- |
| state changes | roam ordered once, 0.5 s ATTACKING dwell | one accepted change per decision serial, 0.3 s non-urgent dwell, `STATE CONFLICT` log |
| attack band | leave at `holdRange` after 0.5 s | leave at `max(holdRange+4, preferredRange+6)` after **1.5 s** |
| movement | every writer can replace the destination each tick | `MoveCommit` holds it for 0.75 m / 2.5 s inside one state |
| urgent | — | `DEAD`, `RESPAWNING`, `SPAWNING`, and `ATTACKING` with a target bypass the dwell |

New header `Code/CryGame/Bots/BotDecisionCommit.h` (engine-free) and `Tests/BotDecisionCommitTests.cpp` (CI list
`BotDecisionCommit`). `BotBrain` additions:

- `m_commit` + `m_move` members;
- `Change(State,bool,DecisionOwner)`; owner inferred by state: weapon → Upgrade, search → Roam, chase/attack → Combat,
  death/respawn → Arbiter;
- `m_commit.BeginTick(m_decisionSerial,m_now)` at the 10 Hz block;
- `Change` returns **without** touching `m_stateTime`/`m_progress` when the offer is rejected (reaction/trap clocks do
  not restart on a rejected writer);
- `ExecuteIntent` holds `m_intent.destination` when another owner tries to replace it; reset on a state change, link
  start or trap level >= 2.

`STATE CONFLICT first=%s second=%s owner=%d actor=%u` is rate-limited to one line per second.

## Expected gate

Run the same 6-bot, 300 s recipe and roster as C-111, `state_commit=2` vs `=1`:

| Metric | C-111 `=1` | Target `=2` |
| --- | ---: | ---: |
| `bots.state_ab_a_2s_per_bot_min` | 6.91 | **< 2.0** |
| `bots.attacking_chasing_per_bot_min` | 14.01 | < 2 |
| `bots.reversals_per_bot_min` | 15.58 | < 10 |
| kills / stalls | 34 / 9 | within noise (39 / 11) |
| `bots.state_conflicts` | 0 (not logged) | visible, then trend to 0 |

Do not promote to default until two paired runs hit < 2 without a kill drop. If ATTACKING→CHASING is still the top
pair, raise `CommitParams.attackLeave` before touching `minDwell`. If the conflicts are mostly `Roam` vs `Upgrade`,
check `UpdateRoam`'s order first; the gate makes the race visible but does not choose the better writer.

## Verification done

- `BotDecisionCommitTests`: all pass with MSVC 19.44 (`/std:c++17`), including one-change-per-tick, 0.3 s dwell,
  1.5 s attack leave, urgent-death override, conflict log rate and `MoveCommit` hold.
- `git apply --check` on a clean `0fa9c1cb` worktree: P1 alone and the combined patch both apply.
- Full DLL not built here (read-only review); BotBrain changes are small and use existing members only.

## Notes / risks

- The first accepted writer wins; a later non-urgent writer is a conflict, not a state change. This is intentional, but
  if a low-priority roam write keeps winning over a combat write, move the combat write earlier in the 10 Hz block.
- `MoveCommit` is reset on every accepted state change, so a new state can pick its own destination immediately.
- `ATTACKING` with a target is urgent, but the one-per-tick rule still applies to non-urgent writers; a roam change
  earlier in the same tick can delay `ATTACKING` by at most one 10 Hz tick.
- The P1 patch does not change `wb_bot_state_commit` default (still 0); it only adds the `=2` path.
