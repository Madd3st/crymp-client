# DS118 — offline diagnosis: why the owned bot's native top exit never gets root motion

Owner is playing the preview: this is a code/docs-only analysis. No game, server, probe, install or data action.
Basis: attempt-4 archive `c154-route-abac72a9-v2`, source `abac72a9` (`QA/build/ds076-code`, clean), extracted
`PlayerFullBody.xml` (`QA/build/c154-exit-assets`), DS117 findings.

## 1. Confirmed facts (from the attempt-4 log, no interpretation)

- Prepare worked: `3.85 LADDER GRAPH prepare … activeBefore=1 stanceBefore=combat defaultBefore=0`; graph
  already active, only the animation `Stance` was overridden. Physical actor/capsule/speed unchanged.
- Climb: 270 `LADDER NATIVE` rows `input=(0,1,0) velocity=(0,0,0.900) animationReady=0`, z `90.33 -> 97.86`.
- Top: `12.40 LADDER EXIT … output='1' state=RelaxedIdleNW+NothingNW`, `12.43 … output=default
  state=RelaxedRunNW+NothingNW`, feet `(2116.125,1880.625,97.920)` for the next 2 s, `14.45 blocked_top`,
  `LADDER GRAPH restored active=1`.
- Between the exit and the cancel the log contains **exactly one** movement line:
  `12.43 MOTION INTENT gait=0 requestedBase=1.30 actual=0.00 blocked=1`. There is no further move/stop request
  while the native exit is pending.
- `PlayerMovement.cpp:1587-1620` (exiting branch) sets no `m_request`, by design; the native exit is meant to be
  animation/root-motion driven. `BotBrain.cpp:1639-1642` (WaitExit) deliberately sends **nothing** ("Do not send
  Stop or a sideways target").

## 2. Native vs bot writers — the asymmetry

- Human/server player: the movement controller is fed from live input every frame (client) or from the native
  ladder code; on a ladder the ladder owns the frame and the crouch/move input is consumed by the ladder branch.
  A zero or ladder-bound request is present each frame, so `PseudoSpeed`/desired velocity cannot stay at a stale
  locomotion value.
- Owned bot on a dedicated server: the only writer is the AI. `CPlayer::UpdateWarsBotMotion`
  (`Player.cpp:5082-5142`) rotates the body and updates actions, but its **server** branch (`:5116-5114`) never
  issues a `RequestMovement`; the request path (`CMovementRequest` with `SetPseudoSpeed(...)`) exists only in the
  client-replica branch (`:5129-5140`). So after the AI stops calling `BotMovement::Move`, the movement
  controller keeps the **last** request (walk, base 1.30) until someone overwrites it.
- Consequence: while WaitExit "sends nothing", the graph keeps receiving the stale locomotion input
  (`PseudoSpeed > 0`), the locomotion states `RelaxedIdleNW`/`RelaxedRunNW` (`PlayerFullBody.xml:1119-1127`,
  `:1140-1145`) stay eligible, and the one-shot `Signal(exit_ladder_top)` state `relaxed_ladderOffTop_nw`
  (`:5054-5060`, set once by `Player.cpp:7335/7449`) cannot hold. Result: no exit clip, no root motion, capsule
  frozen, strict egress guard refuses — exactly what the log shows.

## 3. Hypotheses, kept separate

- **H-A (primary, input-level): stale locomotion request keeps the graph out of the exit state.** Confirmed
  indirectly by the single 12.43 request and the state switch; would be proven by the exit window showing
  `PseudoSpeed/desiredVelocity = 0` while the state is the ladder exit state.
- **H-B (ordering): the exit signal is consumed before the state can hold.** `LeaveLadder` sets the signal during
  the movement/physics update; if the graph re-evaluates in the same frame with a locomotion state winning, the
  signal is lost. Fixing H-A would also cover H-B, so H-B is not separately actionable yet.
- **H-C (root-motion policy): PerAnimGraph/animation-controlled view on the off-top state needs a specific AC
  movement/collider state.** No evidence in the log (the state never ran), so unproven; do not change collider or
  movement mode on this basis.
- Explicitly not assumed: that `Stance` is a hard filter (DS115: `forceGuard=0` soft preference), that graph flags
  equal animation `CA_*` flags, or that physical `STANCE_RELAXED` (-2 in `player.lua`) is usable.

## 4. Minimal candidate (own branch proposal; Codex owns integration)

Change only the owned ladder WaitExit branch (`BotBrain.cpp:1639-1642`): instead of "send nothing", issue one
explicit zero request through the existing helper `BotMovement::Stop(actor)` (`BotMovement.cpp:12-23`:
`ClearMoveTarget`, `SetDesiredSpeed(0)`, `SetPseudoSpeed(0)`, real `RequestMovement`). Latch it once on the
transition to WaitExit (repeat only if the movement state drifts). Scope guards: `WarsBotLadderServer`,
`isOnLadder && isExitingLadder`, alive, not frozen, not ragdoll, `wb_bot_ladder==1`, BotManager owns.
Nothing else changes: stock top ray, stock exit signal, `NativeLadderExitPhysicalReady` guard, empty-output rule,
graph prepare/restore, physical stance/capsule/speed. No SetPos/impulse/timer, no manual `ReachedEnd`, no new cvar.

Expected observable: `state=` in `LADDER EXIT` becomes the ladder exit state (or at least not Idle/Run), `output`
stays `1` while the exit clip runs, feet leave the shaft, and only then does the physical guard allow `ReachedEnd`.
If the state still does not hold with a zeroed request, H-C becomes the next investigation (and only then).

## 5. Actual-caller oracle (engine-free, no game)

Extend the DS112/DS115 pattern: extract the real `BotBrain` WaitExit branch and the real `BotMovement::Stop`
body, mock `CPlayer`/`CMovementController`:
- RED on `abac72a9`: WaitExit issues **zero** `RequestMovement` calls → mock request stays `desired=1.30,
  pseudo=0.29` (the stale value) → assertion "exit must zero the movement request" fails (exit 1).
- GREEN with the candidate: exactly one `RequestMovement` with cleared target, `desiredSpeed=0`, `pseudoSpeed=0`,
  no move target, aim retained.
- Negative controls: human / `wb_bot_ladder=0` / non-owner / not on ladder / not exiting / dead / frozen / ragdoll
  → no request; a second tick must not spam (latched once); Stop must not call `RequestLeaveLadder` or any
  position/timer API; leaving the exit must not restore a stale nonzero request.
