# DS117 follow-up — offline exit-state diagnosis for attempt 4 (`abac72a9`, `blocked_top` 14.45)

Read-only analysis of `build/c154-route-abac72a9-v2`. No game, no rebuild, no QA writes.

## What the run actually shows

- Graph preparation worked: `3.85 LADDER GRAPH prepare actor=65481 activeBefore=1 stanceBefore=combat
  defaultBefore=0` — the graph was already active; only the animation `Stance` was overridden (physical actor
  unchanged).
- Climb unchanged: 270 `LADDER NATIVE` samples `input=(0,1,0)`, `velocity=(0,0,0.900)`, `animationReady=0`,
  z `90.33 -> 97.86`, `vz=0.89`.
- Top: `12.40 LADDER EXIT … output=49('1') … state=RelaxedIdleNW+NothingNW`,
  `12.43 … output=0(default) … state=RelaxedRunNW+NothingNW`, feet `(2116.125,1880.625,97.920)` for the whole
  remaining 2 s, `14.45 blocked_top`, then `LADDER GRAPH restored … active=1`.
- Exactly **one** locomotion request appears in the exit window: `12.43 MOTION INTENT: gait=0 strafe=0
  requestedBase=1.30 actual=0.00 lateral=0.00 facingDot=1.00 blocked=1`; the next one is at `14.45` (cancel,
  `gait=1 requestedBase=0.00`). Between them only the ladder phase/exit lines.
- So the actor is not "waiting for animation": the animation graph is sitting in **locomotion** states, not in the
  ladder exit state, and the native exiting branch returns before any `m_request` is built (`PlayerMovement.cpp
  :1587-1620`), so nothing can move the capsule.

## Why the exit state is not held (installed XML)

- `RelaxedIdleNW` (`PlayerFullBody.xml:1119-1127`) and `RelaxedRunNW` (`:1140-1145`) are ordinary locomotion
  states selected by `PseudoSpeed 0..1.5` (plus Item/Stance/LookIK for Idle; Run has `cost=200`).
- The exit animation lives in `relaxed_ladderOffTop_nw` (`:5054-5060`), selected by `Signal(exit_ladder_top)` and
  extending `RelaxedNW` (`:1133-1139`).
- `LeaveLadder(eLAT_ExitTop)` sets that signal once (`Player.cpp:7335` -> `UpdateLadderAnimation(eLS_ExitTop)` ->
  `SetInput("Signal","exit_ladder_top")`, `:7449`). A signal is consumed when the state is entered; if a
  continuously matching locomotion state keeps winning the selection (any residual `PseudoSpeed`), the exit state
  cannot stay active, and the signal is gone. That matches the observed `IdleNW -> RunNW` and the disappearing
  `OnLadder` output.
- The bot code deliberately sends nothing during `WaitExit` (`BotBrain.cpp:1639-1642`, "Do not send Stop or a
  sideways target"), so the last locomotion request is never cleared. A human does not have this problem because
  ladder movement input is suppressed while exiting.

## Confirmed vs unproven

Confirmed: preparation is active; the graph leaves the ladder exit state for locomotion states; there is no
physical egress; the strict guard correctly refuses completion; restore ran.
Unproven (needs one instrumented run, not assumed): which exact selection rule prefers `RelaxedRunNW` over the
signal state (cost vs. `PseudoSpeed` re-evaluation), and whether a single zeroed input is enough to hold the exit
state for its full clip.

## Candidate direction for the next review (bounded, no new flags)

1. During `m_stats.isExitingLadder` for the owned ladder bot, make the movement input **explicitly zero**
   (Stop with the aim retained, or an idle request) instead of "send nothing", so `PseudoSpeed` cannot keep
   `RelaxedIdleNW`/`RelaxedRunNW` selected; optionally zero the graph `PseudoSpeed` input for that actor only.
2. Keep everything else: stock top ray, stock exit signal, strict `NativeLadderExitPhysicalReady` egress guard,
   empty-output completion only with real egress, no SetPos/impulse/timer/manual `ReachedEnd`, physical stance
   unchanged.
3. Oracle: with the new `state=`/`output=` telemetry, the exit window must log the ladder off-top state (or a
   ladder state emitting `OnLadder=1`) and the feet must leave the shaft before any `ReachedEnd`; a zeroed input
   without egress must still fail closed (`blocked_top`).

This is a design note only; the runtime hypothesis stays unproven until a new exact-SHA CI + review + one shared
route. No unchanged rerun of attempt 4 is proposed.
