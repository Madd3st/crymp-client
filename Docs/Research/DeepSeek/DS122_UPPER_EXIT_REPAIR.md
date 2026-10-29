# DS122 — guarded upper-ladder exit repair proposal (offline, no runtime)

Status: proposal only. Source of truth `warsbots/feature/claude-sprint` at
`abac72a9a1eb02793a2f4ab2b1a72f7c243dd72e` (full CI `40890940918`). Nothing here is an implemented fix, no game,
server, client, probe, QA, build, install or live-data action was taken. Patch/test files on this branch only.

## 1. What the evidence actually pins down

- Attempt 4 (`build/c154-route-abac72a9-v2`, `C154-abac72a9-v2.bots.log`): `12.40 LADDER EXIT output=49('1')
  state=RelaxedIdleNW motionFlags=796 pos=(2116.125,1880.625,97.920)`; `12.43 output=0 state=RelaxedRunNW`;
  feet stay exactly `97.920` until `14.45 blocked_top`. Owner preview (`build/c154-owner-video-20261029/archive`):
  `12.30 output=1 state=RelaxedIdleNW`, `12.33 output=0 state=RelaxedWalkNW`, then `walkToIdle`, same frozen feet
  (`97.932`). The graph is active (`active=1`) and never holds a ladder state in the exit window.
- Human at the same shaft (`QA 3e0d9713`, DS119 `735f052d`): trigger `186.398 z=97.934`, then `99.75` with
  lateral `y 1880.625 -> 1879.727` over ~2 s. That is the **client-side** exit animation/root motion; it does not
  prove the server graph can produce it for a non-client actor.
- `motionFlags=796` = `eACF_PerAnimGraph(4) | ZCoordinateFromPhysics(8) | EnableMovementProcessing(16) |
  ImmediateStance(256) | UseHumanBlending(512)`. The DS115 ladder-exit override is live
  (`Player.cpp:1578-1585`): the exit branch is already graph-motion-driven, not `AlwaysPhysics`. So "the AC mode
  suppresses the exit" is **not** the live defect; the missing piece is state selection/hold.
- Installed graph `Pak/Animations/graphs/PlayerFullBody.xml` (hash pinned `80232bb2…`):
  `relaxed_ladderOffTop_nw` is selected **only** by `Signal = exit_ladder_top`, emits `OnLadder=1`,
  `animationControlledView=1`, template `IdleSignalPlayerAnimation` (`:5054-5060`). The climb state
  `relaxed_ladderClimbNoZ_nw` is selected by `Action=climbLadderUp` + `Item=nw` (`:5037-5044`). From the climb
  state the graph has `-> RelaxedIdleNW` (`:49535`) **before** `-> relaxed_ladderOffTop_nw` (`:49536`);
  `RelaxedIdleNW` matches at any `PseudoSpeed 0..1.5` (`:1119-1127`). `RelaxedRunNW` has `cost=200` (`:1140-1145`).
- Stock exit chain: top ray `PlayerMovement.cpp:1654-1677` -> `RequestLeaveLadder(eLAT_ExitTop)` ->
  `LeaveLadder` sets `isExitingLadder` and calls `UpdateLadderAnimation(eLS_ExitTop)` (`Player.cpp:7333-7337`) ->
  `SetInput("Signal","exit_ladder_top")` + `Action=idle` (`Player.cpp:7498-7519`). The exit branch then returns
  before building any `m_request` (`PlayerMovement.cpp:1587-1624`), so only graph root motion can move the actor.
  Completion is stock `RequestLeaveLadder(eLAT_ReachedEnd)` when `OnLadder != '1'` (`:1620-1622`), gated for the
  bot by `NativeLadderExitPhysicalReady` (`BotLadder.h:18-29`).
- The one-shot signal is set exactly once for both human and bot. A client reaches its graph update from the
  input/movement frame; the owned server bot's only graph writer after the signal is the 10 Hz brain (which
  deliberately sends nothing in `WaitExit`, `BotBrain.cpp:1639-1642`). The signal is therefore exposed to being
  consumed/lost one frame before the off-top state is selected; the observed `IdleNW -> RunNW` is consistent
  with that race. Whether it is a **time race** or a **selection-rule/link-order** loss is not proven; the
  diagnostics below are designed to distinguish them.

## 2. Pseudo correction (verified, not a root cause)

- `BotMovement::Move` signature: `probe` is parameter **7**, `pseudo` is parameter **10**
  (`BotMovement.h:14`, `BotMovement.cpp:82`). The tail uses
  `SetPseudoSpeed(pseudo>=0 ? pseudo : (speed<2.0 ? 0.3 : 0.5))` (`BotMovement.cpp:138`).
- `BotLocomotion::ExecuteLadder` passes `probe=1.5f`, `jumpTraverse=nativeAttached`, `strafe=false`,
  `pseudo=-1.f`, `arrive=0.5f` (`BotLocomotion.cpp:159-160`). With stance max speed >= 2 this becomes **0.5**.
  The old DS118v2 "constant pseudo 1.5 in ExecuteLadder" claim is **false** (that 1.5 is the probe).
- Native non-sprint input is `CPlayer::CalculatePseudoSpeed(false) == 1.0` (`Player.cpp:2503-2515`), used by
  `CPlayerInput` whenever there is movement input (`PlayerInput.cpp:896-910`). So 0.5 vs 1.0 is a real input-shape
  gap, but both satisfy the `PseudoSpeed 0..1.5` SelectWhen of `RelaxedIdleNW/WalkNW/RunNW`, so it is **not**
  sufficient to explain the lost exit state. It is kept as a separate, scoped parity commit (P2), not sold as the fix.
- Late `Stop` in `WaitExit` stays withdrawn (`150feb4b`): it writes `SetPseudoSpeed(0)`/clears the move target
  (`BotMovement.cpp:12-24`) after the exit signal and overwrites aim intent; and zero pseudo still matches
  `RelaxedIdleNW`, so it cannot win the state selection by itself.

## 3. Candidate P1 — native exit-signal hold (primary, `ds122_exit_signal_hold.patch`)

Scope: `WarsBotLadderServer(player)` && `isOnLadder` && `isExitingLadder` && alive/not frozen/not ragdoll
(the existing helper already gates owned bot + `wb_bot_ladder==1`, `PlayerMovement.cpp:43-47`). Humans, clients,
other actors, OFF and non-owned actors are untouched.

- In the stock exit branch, while the current graph state is **not** the installed off-top state
  (`GetCurrentStateName()` lacks `ladderOffTop`, hash-pinned), re-issue the **same stock signal**
  `Signal="exit_ladder_top"`. This is the only new write. It stops as soon as the off-top state is current, so
  the clip cannot be restarted; if the state never holds, the policy still times out to `blocked_top`.
- No position, velocity, impulse, collider, health or completion write is added. The stock `OnLadder` read,
  `NativeLadderExitPhysicalReady`, `ReachedEnd` and the fail-closed policy are unchanged.
- The existing `WB_LOG(2)` exit line is extended (same line count) with `held`, `sig` (`IsSignalledInput`),
  `pseudo` (`GetInputAsFloat`), `upd` (`CS_FLAG_UPDATE_ALWAYS` on the character instance) and keeps `state`,
  `output`, `motionFlags`, `pos`. A second one-shot `WB_LOG(2)` in `LeaveLadder(eLAT_ExitTop)` records
  `state`/`output` at the exact moment the signal is issued (`ds122_exit_signal_hold.patch`, Player.cpp).
- Why this is the minimal native lever: the off-top state's only trigger is that signal; re-asserting it is
  idempotent, needs no new flag, and adds no physical authority. It directly tests the "one-shot signal lost
  before the server graph update" hypothesis without a standalone instrumentation run.
- Patch validated with `git apply --check` against the exact `abac72a9` blobs of `Player.cpp` and
  `PlayerMovement.cpp` (scratch copy, not the game tree). Compile not run here.

## 4. Candidate P2 — ladder-scoped native pseudo parity (separate, `ds122_ladder_pseudo_parity.patch`)

One line in `BotLocomotion::ExecuteLadder`: pass `actor.CalculatePseudoSpeed(false)` (1.0) instead of `-1.f`
(0.5). This makes the climb request's graph input match a non-sprint human on the same ladder. It changes no
speed, guard or completion rule. Assessed as a real contract gap, **not** as the exit root cause; integrate only
as a separately reviewed commit if the batch owner wants input parity, and re-check the climb state in the same
run.

## 5. Fallback F — only if P1's trace proves the server graph cannot hold the exit

Not implemented, not authorized by this document. Contract if it is ever proposed:

- Frame: native `m_stats.ladderBottom/ladderTop/ladderOrientation/ladderUpDir` only, never human global
  coordinates or a replayed curve.
- Collider lifecycle first: `GrabOnLadder` sets `eColliderMode_PushesPlayersOnly` (`Player.cpp:7257`), which is
  not a wall-safe capsule. Before any egress step, request a normal collider mode for the owned bot and restore
  `PushesPlayersOnly` on an in-place abort, or let stock `LeaveLadder` restore `eColliderMode_Undefined`
  (`Player.cpp:7344-7345`) on a real leave.
- Clearance must be a **swept capsule/shape** of the actual character collider (radius/height from the stance
  info) along the step, plus a downward shape query for the landing floor. Feet/mid/head point rays are **not**
  sufficient and must not be presented as a capsule sweep.
- Phases: stock climb to the native top (existing), lateral egress along `ladderOrientation` at a bounded speed
  and distance, separate vertical correction; any sweep hit, missing floor, timeout or bounds violation ->
  `blocked_top`, no partial teleport, no retry.
- Completion: stock `RequestLeaveLadder(eLAT_ReachedEnd)` only after `NativeLadderExitPhysicalReady` **and** a
  grounded landing on the upper floor band derived from the native top; no manual early `ReachedEnd`.
- Controls: human/client, OFF, non-owned, dead, frozen/ragdoll, both-flag modes -> zero calls; no `SetPos`,
  `SetWorldTM`, impulse, timer-only detach or height-only success.

## 6. Red/green controls (source-contract, `ds122_exit_signal_contract_test.py`)

Compares the block extracted from a revision (`--rev abac...` vs `WORKTREE`/patched) and asserts:

- RED on `abac72a9`: the exit branch issues no exit-signal re-assert and `ExecuteLadder` passes `-1.f`.
- GREEN on P1: exactly one scoped `Signal="exit_ladder_top"` write inside `if (warsBot)`, guarded by
  `pAGState` and `!exitHeld`, still followed by the stock `NativeLadderExitPhysicalReady` guard and the single
  `RequestLeaveLadder(eLAT_ReachedEnd)`; no `SetPos/SetWorldTM/AddImpulse/ForceSkeletonUpdate/PushForcedState`,
  no new completion path, no `Stop` in the block.
- GREEN on P2: the `Move(...)` pseudo argument is `actor.CalculatePseudoSpeed(false)`, probe still `1.5f`,
  arrive still `0.5f`.
- Diagnostic fields present once in the exit log (`held`, `sig`, `pseudo`, `upd`), and the one-shot
  `LADDER EXIT SIGNAL` line present in `LeaveLadder(eLAT_ExitTop)`.
- Test not executed in this session (no Python in the research worktree); Codex/CI runs it. The assertions were
  checked by hand against the patched copy and the `abac72a9` blob.

## 7. What the single authorized run must decide (folded diagnostics, no standalone probe)

With P1 integrated and the normal exact-SHA/full-green-CI/fresh-review gates passed, the one shared route should
show, in the exit window:

1. `LADDER EXIT SIGNAL` state at the signal (climb state vs locomotion state);
2. whether `held=1` and `state=…ladderOffTop…` is reached after the re-assert, and for how long;
3. `upd=1` (CS_FLAG_UPDATE_ALWAYS) and `sig`/`pseudo` values;
4. feet actually leaving the shaft before any `ReachedEnd`; then the grounded upper floor.

Decision table: held + physical egress -> native path repaired, no fallback. Signal re-asserted but still
`held=0` -> selection-rule/link-order loss; the next candidate is a reviewed forced native state
(`PushForcedState`, bounded/cleared) or fallback F, not another unchanged rerun. `upd=0` -> character update
lifecycle (render-proxy condition) is the missing observation, folded as a scoped fix for the owned bot.

## 8. Gates and non-claims

- No game/QA/build/install/live-data action; owner may launch the classic package, so game-free is re-checked
  before any runtime.
- No default/flag promotion; graph source hash and identity (`IsPlayer`, channel 0) preserved; stock top checks,
  physical egress and fail-closed policy stay.
- Not claimed: that P1 works at runtime, that the server graph can or cannot own the exit, that pseudo is the
  root cause, or that the human curve is reproducible server-side. Cross-client/PS/default behaviour unchanged.
- Integration path: Codex reviews/merges, new exact full-green CI pair, fresh independent review, then **one**
  lower-545 -> ladder -> upper floor -> water -> shore acceptance only when the owner is free.
