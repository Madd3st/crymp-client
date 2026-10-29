# DS114 (v2, corrected) — C154 attempt 3 on `272a4306`: why the native top exit does not select

Status: offline diagnosis + bounded candidate design. No engine/QA/game/data/install/launcher/restore writes.
This file replaces the v1 diagnosis; the corrections are listed first because v1 contained wrong inferences.

## 1. Corrections to my v1 report (all accepted)

- **`motionFlags=796` was misread.** 796 = `0x31C` is `EAnimatedCharacterFlags`
  (`Code/CryCommon/CryAction/IAnimatedCharacter.h:56-83`): `PerAnimGraph 0x4 | ZCoordinateFromPhysics 0x8 |
  EnableMovementProcessing 0x10 | ImmediateStance 0x100 | UseHumanBlending 0x200`. It is **not**
  `CAnimation::m_AnimParams.m_nFlags`, so it says nothing about `CA_MANUAL_UPDATE` or the ladder FIFO.
  "Flags lack CA_MANUAL_UPDATE" is withdrawn.
- **`UpdateLadderAnimation(eLS_ExitTop)` is not a FIFO gate.** It sets `Signal=exit_ladder_top`
  (`Player.cpp:7449`) and `Action=idle` (`:7453`), skips the `eLS_Climb` manual-seek block and returns false
  (`:7465-7484`); `LeaveLadder` ignores that return (`Player.cpp:7335`). The exit is triggered by the **Signal**,
  so completion depends on the graph selecting the off-top state, not on the climb FIFO.
- **Empty `OnLadder` means "no current state defines this output"**, not "the graph never left the ladder state".
  `QueryOutput` resolves per current state/layer (`IAnimationGraph.h:497-499`), so a state without
  `<Output name="OnLadder">` yields `""` (first byte NUL, logged as `0`).
- **Geometry**: native AABB top `100.291` (owner/report value) and nav endpoint `b.z=99.768` are different
  datums; feet `97.927` is ~**2.36 m below the native top**, not 1.83 m below the nav endpoint.
- **Option D (height/`ReachedEnd`-only detach) is withdrawn.** A height threshold alone is not physical proof,
  and `grounded=1` while `flying=1` on a ladder does not prove support. No physical-trajectory invention.

## 2. Confirmed runtime evidence (archive `c154-route-272a4306-v2`, audited)

- Climb fix works: `4.00–12.41` native `input=(0,1,0)`, stock `velocity=(0,0,0.900)`, z `90.33 → 97.84`,
  `animationReady=0` throughout (271 samples).
- Exit: `12.47` top ray (`PlayerMovement.cpp:1650-1672`) called `RequestLeaveLadder(eLAT_ExitTop)`; from then
  `LADDER EXIT graph=1 output=<NUL> motionFlags=796 pos=(2116.125,1880.625,97.927)`, 21 consecutive frames,
  frozen; `14.53 blocked_top` + cancel. Actor 65481, HP 100. `MOVE TRACE … stance=0 weapon=0 flying=1 mode=3`.
- Motion during exit is deliberately absent: `BotLocomotion::ExecuteLadder` returns early when
  `isExitingLadder` (`BotLocomotion.cpp:154`), and the exiting branch of `ProcessMovementOnLadder` sets no
  `m_request` (`PlayerMovement.cpp:1587-1620`). The exit is by design animation/root-motion driven.

## 3. Asset selectors (installed `Animations.pak`, extracted XML, verified line by line)

- Climb state `relaxed_ladderClimbNoZ_nw` (`5037-5044`): `extend="Alive"`,
  `SelectWhen = Action(climbLadderUp) + Item(nw)`, template `ManualUpdateIdle`, `<Output OnLadder=1>`.
- Exit state `relaxed_ladderOffTop_nw` (`5054-5060`): `extend="RelaxedNW"`,
  `SelectWhen = Signal(exit_ladder_top)`, `<Output OnLadder=1>`.
- Extension `RelaxedNW` (`1133-1139`): `SelectWhen = Item(nw) + Stance(relaxed) + UsingLookIK[0..1]`.
  So every `relaxed_*` ladder state that lives under `RelaxedNW` (including the off-top exit) requires
  **stance = relaxed** and **item = nw**.
- There is **no** `combat_*`/`crouch_*` ladder variant: `Select-String '<State id="[^"]*ladder[^"]*"'` returns only
  `relaxed_*` ladder states (5034..5064). A `combat` stance actor therefore has no ladder animation state.

## 4. Code path that sets the two inputs

- `Item`: `CPlayer::GrabOnLadder` calls `HolsterItem(true)` (`Player.cpp:7249`), and V3 shows `weapon=0` during the
  whole climb, so the item side looks satisfied (the graph input value itself is still unlogged).
- `Stance`: `STANCE_STAND` maps to graph name `combat`; `STANCE_RELAXED` maps to `relaxed`
  (installed `BasicActor.lua`, stance table: `name="combat"` line 767, `name="relaxed"` line 850).
  The actor stance is only changed through `CActor::SetStance` → `m_desiredStance`
  (`Actor.cpp:1284-1286`) → transition in `CActor::Update` (`:1692-1709`), which calls
  `m_pAnimatedCharacter->RequestStance(m_stance, GetStanceInfo(m_stance)->name)` (`:1709`) — that is what feeds the
  graph `Stance` input. Movement-request stance (`SActorFrameMovementParams::stance`) does not by itself change
  `m_stance`.
- `BotLocomotion::ExecuteLadder` (`BotLocomotion.cpp:151-164`) hard-codes `STANCE_STAND` **every climb tick**:
  base `actor.GetStanceMaxSpeed(STANCE_STAND)` (`:159`) and `SetWarsBotActions(false, accepted, STANCE_STAND)`
  (`:161`), called from `BotBrain.cpp:1650`. The bot therefore stays `stance=0` (`combat`) on the ladder.
- Normalisation: `PlayerMovementController.cpp:847` divides by `GetStanceMaxSpeed(m_pPlayer->GetStance())`, then
  `:856 params.desiredVelocity = desiredMovement * desiredSpeed / stanceSpeed`. Passing the *same stance's* max
  speed as base keeps the ratio at 1.0, i.e. the current climb speed.

## 5. Bounded candidate (owned server ladder only) — to be implemented by Codex, not by me

While `WarsBotLadderServer(actor)` and the native ladder is attached:
1. `actor.SetStance(STANCE_RELAXED)` once on attach (idempotent; remember the previous stance). This is the stock
   public path that ends in `RequestStance(..., "relaxed")`, so the graph `Stance` input becomes `relaxed`.
2. Keep `ExecuteLadder` moving with base `actor.GetStanceMaxSpeed(actor.GetStance())` (transition-safe: ratio 1.0
   in every frame, same climb speed; the owner's `GetStanceMaxSpeed(RELAXED)` is the steady-state case) and
   `SetWarsBotActions(..., STANCE_RELAXED)`.
3. Hold the relaxed stance through the native exit (`isExitingLadder`), then restore the previous stance exactly
   once when the bot is no longer `onLadder` or the ladder phase is Done/Failed/Banned.
4. Approach/Settle keep the ordinary executor and `STANCE_STAND`; humans/OFF/non-owner/client/dead/frozen/ragdoll
   keep the current path untouched. The DS112 animation-gate bypass stays as a safety net.
5. Existing diagnostic only: add `GetCurrentStateName()` and the `Item`/`Stance` graph input values to the existing
   `WB_LOG(2)` climb/exit lines (no new cvar, no new flag). One line tells us whether the off-top state became
   `relaxed_ladderOffTop_nw` and whether `Item` really is `nw`.

Explicitly **not** in the candidate: `SetPos`, impulse, timer/height detach, manual `ReachedEnd` without graph or
physical support, manifest bypass, changes to the exit gate for anyone but the owned bot, new cvars.

Optional follow-up only if the new log shows the graph still not selecting after the stance fix:
`IAnimationGraphState::SetAnimationActivation(true)` is a real CryAction vtable call. The per-entity API
`IAnimationGraphSystem::SetAnimationGraphActivation(EntityId,bool)` is an **empty stub** in this rebuild
(`Code/CryAction/AnimationGraphSystem.cpp:65-67`), so that route cannot be used without an engine patch.

## 6. Real-caller oracle (engine-free, extracted actual code; RED on 272 / GREEN on candidate)

Extend the DS112 actual-caller pattern (extract `ExecuteLadder` + the ladder adapter prefix, mock `CPlayer`/graph):
- attached owned climb → exactly one `SetStance(STANCE_RELAXED)`; movement base == `GetStanceMaxSpeed(current)`;
  `desiredSpeed/base == 1.0`; actions stance `RELAXED`;
- exit/Done/Failed/leave → previous stance restored exactly once; no per-frame flip-flop;
- Approach/Settle (not attached) → identical to 272 (STAND, ordinary executor);
- human/`wb_bot_ladder=0`/non-server/non-owner/client/dead/frozen/ragdoll → zero new stance calls;
- zero and down input unchanged; no `SetPos`/impulse/timer/manifest-bypass calls in the extracted block;
- existing DS112 climb-caller and DS108 motion-mode tests must stay green.

## 7. What remains unknown and what would prove it

- The graph `Item` input value on a server-only bot (holstered) and whether the climb state ever activated; the new
  `GetCurrentStateName()`/input log decides without a new flag.
- Whether `UsingLookIK` for a WarsBot is inside `[0..1]` (guard of `RelaxedNW`); also visible in the same log if added.
- Only after that, and only if the graph path is proven impossible, does a *physically validated* exit contract
  become the option — and it must show real lateral egress plus actual support (not height, not `grounded` while
  `flying`), which is the owner's explicit condition.
