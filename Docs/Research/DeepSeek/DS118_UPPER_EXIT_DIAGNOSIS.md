# DS118 (v2, corrected) — upper exit: what is *not* proven, and what the ordering rules out

Owner is playing the preview: offline only, no game/server/probe/install/data action. This file replaces the
v1 diagnosis; v1's primary hypothesis and its patch/oracle are withdrawn.

## 1. Corrections accepted (all from the owner's review, verified in source)

1. `BotBrain.cpp:492-494` `MOTION INTENT requestedBase=…` is **telemetry computed from `m_intent`**
   (`m_intent.move ? (gait==Walk ? 1.3 : 2.5) : 0`), not a `RequestMovement` event and not the real desired
   speed. Its 12.43 line therefore does not prove a "walk 1.3 writer"; v1's inference is withdrawn.
2. `BotLocomotion::ExecuteLadder` (`BotLocomotion.cpp:151-164`) uses `GetStanceMaxSpeed(STANCE_STAND)` plus a
   **constant pseudo 1.5**, and returns immediately when `stats->isExitingLadder` (`:154`). So during the exit the
   last real request is the climb request (stance speed, pseudo 1.5), not a walk request; "stale walk 1.3" is
   wrong. A stale request remains a plausible *code risk*, but the physical cause is a hypothesis, not a finding.
3. `BotMovement::Stop` (`BotMovement.cpp:12-23`) calls `SetWarsBotMotion(forward,0,lookAhead)` **first**
   (`:14-15`), which overwrites the aim, then sends `ClearMoveTarget/DesiredSpeed 0/PseudoSpeed 0`. v1's oracle
   claim "aim retained" is withdrawn.
4. Ordering: the exit signal is emitted **inside the movement update** —
   `ProcessMovementOnLadder` top ray -> `RequestLeaveLadder(eLAT_ExitTop)` -> `LeaveLadder` ->
   `UpdateLadderAnimation(eLS_ExitTop)` -> `SetInput("Signal","exit_ladder_top")`
   (`PlayerMovement.cpp:1650-1672`, `Player.cpp:7332-7336`, `:7449`). The brain learns `exiting` only at its next
   10 Hz tick (`BotBrain` decision cadence ~0.1 s), i.e. 6-20 graph updates later. A `Stop` issued at the
   transition to `WaitExit` (`BotBrain.cpp:1639-1642`) is therefore **after** the signal window and cannot be the
   fix. The observed `output='1'/IdleNW` -> `output=default/RunNW` already happened before any WaitExit action.
5. Native path is not "always zero": `CNetPlayerInput::PreUpdate` (`NetPlayerInput.cpp:15-68`) rebuilds the
   request **every frame** from `m_curInput.deltaMovement` (`AddDeltaMovement`), with
   `pseudoSpeed = deltaMovement.len2()>0 ? CalculatePseudoSpeed(sprint) : 0`. A human holding forward on a ladder
   has a non-zero pseudo too, so "the human request is zero" is not established. The difference, if any, is in the
   *request type/values* or in the graph environment, not simply in zero vs non-zero.
6. Graph environment: the native local player is `IsPlayer() == true` (first-person graph path), the owned bot is
   not; the off-top template `IdleSignalPlayerAnimation` is implemented in closed code. This is a hypothesis
   area only — nothing here proves it.
7. Force-skeleton/render plumbing already exists for bots: `SetForceSkeletonUpdate(4)` under `m_warsBotPose`
   (`Player.cpp:1718-1723`) and `CS_FLAG_UPDATE_ALWAYS` in `PostPhysicalize` (`:4463-4467`). No blind enabling is
   proposed.
8. Installed graph matches the extracted asset (`PlayerFullBody.xml` sha
   `80232bb29f917ba2ae92700cd01b8a07fb2b64e4e399e976632d3b1634bcc8fa` == `QA/build/c154-exit-assets`); no
   override/mismatch claim.

## 2. What the log still establishes (no interpretation)

- Prepare worked (`activeBefore=1, stanceBefore=combat`), climb unchanged (270 native rows, z 90.33 -> 97.86).
- At the top: `output='1' state=RelaxedIdleNW` then `output=default state=RelaxedRunNW`, feet pinned `97.920`,
  `blocked_top` at 14.45, graph restored. The exit state never holds; no egress; guard correctly refuses.
- Nothing in the log shows a `RequestMovement` after the exit; the graph sits in locomotion states for ~2 s.

## 3. Candidate causes, kept separate and untested

- **C-1 request shape**: the bot's last request is a *move-target* request (`SetMoveTarget` + `SetDesiredSpeed`,
  pseudo 1.5) while the human's is a *delta-movement* request with `CalculatePseudoSpeed`. The AG may derive
  `PseudoSpeed`/movement inputs differently for the two shapes (e.g. pseudo 1.5 saturating a locomotion state).
  Test: compare the controller's `params` (desiredVelocity/pseudo) and the AG inputs for the same climb/exit.
- **C-2 graph environment**: first-person/`IsPlayer` differences or the closed `IdleSignalPlayerAnimation`
  template (`animationControlledView=1`) may make the off-top state unavailable/zero-root-motion for a non-player
  actor. Unverifiable offline; needs the AG input/state trace.
- **C-3 one-shot signal consumed before the ladder state can hold**: the signal is set once; if any locomotion
  state outranks it in that same graph update, the signal is gone before the next brain tick (see 1.4). This is an
  ordering fact; whether it is *the* cause depends on C-1/C-2.
- Explicitly not assumed: `Stance` as a hard filter (`forceGuard=0`), graph flags = animation `CA_*` flags,
  physical `STANCE_RELAXED` (-2), height/timeout detach.

## 4. Consequence for the next patch (no behaviour change proposed yet)

- The v1 "Stop in WaitExit" patch is **withdrawn**: it is late by construction (1.4) and its aim side effect
  contradicts its own oracle (1.3).
- Since C-1/C-2 are not distinguishable from the current log, the coherent narrow step is **instrumentation
  only** in the existing `WB_LOG(2)` exit line for the owned bot: state name (already), `PseudoSpeed`,
  `desiredVelocity`, `Action` string, `FirstPersonMode`, `Item`/`Stance` graph inputs. No new cvar, no behaviour
  change, no forced `Update()`. That single run would separate C-1 from C-2 and only then justify a patch.
- If the owner wants a behaviour change without that data, the only defensible ordering fix is to neutralise the
  movement request **in the same frame and before** `RequestLeaveLadder(eLAT_ExitTop)` for owned bots
  (`PlayerMovement.cpp:1671`), so the request shape seen by the graph at signal time is not the climb request.
  This is explicitly unproven and must not be labelled a root-cause fix.

## 5. Actual-caller checks that are meaningful (not self-fulfilling)

1. **Ordering refutation test** (supports no patch): extract the real top-ray/`RequestLeaveLadder` block and the
   real `BotBrain` WaitExit branch; drive a mock 10 Hz brain clock against a mock per-frame graph tick and assert
   that the signal timestamp always precedes the first WaitExit tick. This proves the WaitExit-Stop class of patch
   cannot act in time — a negative result, not an assertion of a proposed patch.
2. **Input-shape comparison test**: extract the real `CNetPlayerInput::PreUpdate` and the real
   `BotMovement::Move` request construction; assert the two produce different `CMovementRequest` shapes for the
   same ladder situation (delta vs move target, pseudo source). This documents C-1 without claiming a fix.
3. Only after a discriminating run may a fix be proposed, with its own RED/GREEN against the *observed* state
   transition (ladder exit state must hold and feet must leave the shaft), not against the patch's own intent.
