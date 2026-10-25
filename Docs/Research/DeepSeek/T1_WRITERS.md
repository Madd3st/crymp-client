# T-1 writer inventory: every control write in BotBrain and its modules

Build `5b90bfb6`. Scope: runtime writes that can decide or emit a bot action: `BotIntent` fields, the native movement/aim
sinks, fire, suit, jump, stance, weapon selection and zoom. Internal caches (`m_blackboard.*`, `m_combat.*`,
`m_look.*`) are listed only where they directly overwrite one of those outputs.

There is no `BotIntent::action` field. The action channels are `move/destination`, `aim`, `gait`, `jump`, `stance`,
`suitMode`, `combat` (fire intent) and the module-side native calls. `BotIntent::AimAt` (`BotIntent.h:31`) is unused.

## 0. Who can write

- **BotBrain** writes `m_intent.*` directly, or through `WBI` / `WBI_MOVETO` (`BotBrain.cpp:23-24`). These are immediate
  assignments: there is no priority, mask, or "highest wins" mechanism. The last assignment before `ExecuteIntent`
  wins that field.
- **BotMovement** writes the native pose: `SetWarsBotMotion` (`BotMovement.cpp:14,28,130`), `SetWarsBotAim`
  (`BotMovement.cpp:44`), and the movement request (`:16-23,29-38,131-141`).
- **BotLocomotion** writes native action bits, stance and jump request: `SetWarsBotActions` (`BotLocomotion.cpp:135`),
  `RequestMovement` with `SetStance`/`SetJump` (`:136-139`), and `PublishWarsBotJump` on takeoff
  (`BotLocomotion.cpp:43` -> `Player.cpp:4968`).
- **BotCombat** writes fire: `StartFire` (`BotCombat.cpp:272`), `StopFire` (`:36-42`), called from `Attack` (`:235-277`)
  and target/observe resets (`:104,219,223`).
- **BotWeapons** writes item selection and zoom: `SetActorItem` (`BotWeapons.cpp:77,185,233`), `StartZoom`/`StopZoom`
  (`:272-273`), `Reload` (`:245`).
- **BotSuit** writes the native suit mode: `suit->SetMode(mode)` (`BotSuit.cpp:40`); it can also force Defense when
  cloak is disabled (`:29-33`).
- **BotSuitPolicy / BotGoalArbiter / BotVantage / BotTactics / BotAimModel** return decisions/points. They do not write
  `m_intent`; BotBrain copies or applies them. The only module that writes an intent-like value is BotBrain.

## 1. Call order inside `BotBrain::Update`

`Update` starts at `BotBrain.cpp:300`; `dt` is clamped at `:304`.

| # | Call | Lines | Writes / effect | Early return |
| --- | --- | --- | --- | --- |
| 1 | blackboard sample | 308 | reads pose, health, energy, suit | no |
| 2 | `m_locomotion.Observe` | 309 | jump takeoff/landing logs, `PublishWarsBotJump`, `state.nextJump` | no |
| 3 | telemetry | 310 | reads only (MOTION INTENT, TACTIC SAMPLE) | no |
| 4 | `UpdateDead` | 311 | `StopFire`, `m_locomotion.Stop`, intent reset on death, 5 s respawn | yes |
| 5 | `UpdateTurnCap` (only `wb_bot_turn_cap=1`) | 312 | native `SetWarsBotAim` every frame, using the **previous** `m_intent.aim` | no |
| 6 | 10 Hz gate | 313-317 | accumulates `m_tick`; returns below 0.1 s | yes |
| 7 | `m_intent=BotIntent()` | 320 | clears every field to defaults | no |
| 8 | default aim | 321 | forward 10 m + 1.4 m | no |
| 9 | `UpdateTest` | 322 | test move/aim/gait/jump/suit; `ExecuteIntent` | only while test active |
| 10 | `UpdateWeaponSeek` | 325 | weapon selection; movement to a weapon; `ExecuteIntent` | only on the no-primary/QA branches |
| 11 | `FindEnemy` | 326 | target selection; `StopFire` on switch/clear; no intent write | no |
| 12 | `UpdateArbitrated` (if `wb_bot_goal_arbiter=1`) | 340 | calls upgrade/combat/roam, each executes; returns true | yes, when enabled |
| 13 | `UpdateUpgrade` | 341 | moves to a better weapon; `ExecuteIntent`; returns true | only when it owns the goal |
| 14 | `UpdateRoam` (no enemy) | 342 | vantage, loot, heard gunshot, patrol; `ExecuteIntent`; returns | yes, no enemy |
| 15 | `UpdateCombat` | 343 | fight movement/aim/gait/suit/jump/combat; `UpdateCombatPost` | no; every branch reaches an executor |

`UpdateCombat` order (`BotBrain.cpp:683`): range weapon select `:692-693`; aim model/tracking `:694-709`; desired
range `:711`; withdraw `:714-730`; vantage hold `:731-741`; hold angle `:745`; attack/chase `:748-753`; post
`:754`. The withdrawing branch calls `ExecuteIntent` directly at `:729`; the vantage-hold branch calls
`UpdateCombatPost` at `:739`; normal combat calls it at `:754`; hold angle calls it at `:786`.

`ExecuteIntent` order (`BotBrain.cpp:959`): trap recovery `:961`; look policy `:962`; test suit `:963-964`; link jump
reset `:966`; link executor `:967`; obstacle jump `:971-981`; fall/landing suit `:983-986`; damage/combat armor
`:987-990`; suit policy `:991`; corner slow `:992-996`; suit apply `:997`; turn-cap copy and locomotion `:1002-1007`;
source log `:1010-1014`; fire `:1015-1020`; weapon handling (laser/zoom) `:1021-1027`.

## 2. `m_intent` writers by field

Line numbers are `BotBrain.cpp` unless stated. "reset" means the field returns to its `BotIntent` default at `:320`.

### move / destination / arrive / meshPath

| Field | Writers | Trigger / note |
| --- | --- | --- |
| `move` | reset `:320`; `WBI(move,false)` `:435,457,465,469,543,678,736,784`; `WBI_MOVETO` (`MoveTo`, sets true) `:418,461,548,632,677,724,782,798,827,851,949`; `WBI(move,true)` `:1074` | `MoveTo` is `BotIntent.h:30`; `WBI_MOVETO` also sets destination and aim |
| `destination` | `MoveTo`: `:418,461,548,632,677,724,782,798,827,851,949`; direct `:1075` | `:1075` is the link executor landing point, after all state logic |
| `arrive` | `:153` = 0.8; `:154` = 0.2 | set inside `Steer`; 0.2 only when navmesh corners own arrival |
| `meshPath` | `:150` (true if path active); `:149` (false if path empty); reset `:320` | stairs clearance mode for Movement; no `why` line |

`m_intent.arrive` and `m_intent.meshPath` are path-shape controls with no `why` channel. They are not the only direct
writes without `why`: combat, strafe, stance, destination and linkJump also lack one (section 2, "why").

### aim

| Writer | Lines | Note |
| --- | --- | --- |
| default forward | `:321` | overwritten by the state that runs |
| `WBI_MOVETO` look argument | `:418,461,548,632,677,724,782,798,827,851,949` | travel/patrol/weapon/vantage/combat points |
| `WBI(aim,...)` | `:736,784,1131` | vantage hold, hold-angle hold, look policy |
| `MoveTo` | `BotIntent.h:30` | same writes as the `WBI_MOVETO` lines above |
| `ApplyLookPolicy` | `:1089-1131` | last intent aim writer in `ExecuteIntent`; **returns without writing** if `wb_bot_look=0`, during a test, with a visible target, or while a vantage is held (`:1091-1092`) |
| `CappedLook` copy | `:1004,1188-1196` | does not write `m_intent`; it replaces the copy passed to `BotLocomotion` when turn cap is on |

### gait

| Writers | Lines | Note |
| --- | --- | --- |
| `TravelGait` | `:88,92,95` | pace/sprint selection; `:92` is the no-Speed return, `:95` the normal choice |
| `UpdateTest` | `:418` | test override |
| vantage | `:677` | Walk while moving to a spot |
| hold angle | `:782` | Run |
| trap recovery | `:949-950` | MoveTo then Run |
| chase | `:872` | sprint/run by distance, energy, current mode |
| cloak approach | `:876` | Walk |
| link executor | `:1077` | Sprint during link act except drop |
| corner slow | `:994` | forces Run |
| suit fallback | `:1000` | forced Run if sprint suit request failed |
| `UpdateChasing` | `:851-873` | `TravelGait`; last writer before post |

No module writes `m_intent.gait`; `BotLocomotion` only consumes it (`BotLocomotion.cpp:123-132`).

### jump

| Writer | Lines | Note |
| --- | --- | --- |
| reset | `:320` | `None` |
| random travel hop | `:106` | Strength; only when `wb_bot_jump_sanity != 1` |
| test | `:419` | test jump |
| jump-strafe | `:832` | Normal after being hit |
| dodge | `:842` | Strength |
| trap recovery | `:954` | Normal; level 3 |
| obstacle ray | `:979` | from `m_locomotion.Obstacle` |
| link executor | `:1078` | Strength/Normal/None |
| suit policy | `:1270` | downgrades Strength to Normal if the policy chose non-Strength |

Actual jump is decided later by `BotLocomotion::Execute` (`BotLocomotion.cpp:107-121`): refused if not grounded,
pending/airborne, prone, cooldown, Strength not active/energy, Normal while in Strength, or no clearance. A refused
jump leaves `m_intent.jump` set for the log but does not call `SetJump`; it is cleared only by the next tick reset.

### suitMode

| Writer | Lines | Note |
| --- | --- | --- |
| `TravelGait` | `:89,97` | Speed or Defense |
| random travel hop | `:106` | Strength |
| test | `:420-421` | test jump/gait |
| withdraw | `:726,728` | Defense; Speed only when hidden and energy allows |
| trap | `:950` | Defense |
| obstacle jump | `:980` | Strength for a Strength jump, else Defense |
| landing | `:984` | Defense before landing |
| airborne Strength | `:985` | Strength while a Strength jump is active |
| damage/combat | `:988,990` | Defense when damaged, in combat, or low energy |
| link | `:1080-1081` | Strength if `wantStrength`, else Defense if `wantArmor` |
| suit policy | `:1265-1268` | last policy word when `wb_bot_suit_policy=1` |
| corner slow | `:995` | Defense if Speed into a sharp turn |
| suit-not-ready fallback | `:999` | Defense if the sprint suit request failed |

The last **syntactic** writer is `:999` (when it fires), else `:995`, else the suit policy `:1265-1268` when enabled;
with suit policy disabled, the last earlier branch (usually damage/combat armor at `:988`) stands. The last **actual**
writer is `BotSuit::Apply` (`BotSuit.cpp:40`); it can refuse for suit inactive, mode unavailable, energy, disabled
cloak, or the 2 s change cooldown (`BotSuit.cpp:21-40`). Defense may interrupt the cooldown; other modes cannot.

### stance

| Writer | Lines | Note |
| --- | --- | --- |
| default | `:320` | `STANCE_STAND` |
| vantage hold | `:678` | `STANCE_PRONE` when `m_traits.prone > 0.5` |
| vantage combat | `:737` | `STANCE_PRONE` on the spot |

Only two runtime writers. `BotLocomotion` passes it to `SetWarsBotActions` and `RequestMovement`
(`BotLocomotion.cpp:135-139`); `BotMovement::Aim` internally asks for `STANCE_STAND`
(`BotMovement.cpp:36`), but `BotLocomotion` sends the final request with `intent.stance` afterwards, so the intent
stance is the last request. Native animation can still override it while jumping: `BotLocomotion` requests
`STANCE_STAND` when jumping (`BotLocomotion.cpp:137`).

### combat / fire intent

| Writer | Lines | Note |
| --- | --- | --- |
| default | `:320` | `None` |
| withdraw | `:725,728` | Aim; None when sprinting hidden |
| vantage hold | `:738` | Fire/Aim after reaction |
| hold angle | `:785` | Aim |
| attacking | `:799,835` | Aim on retreat; Fire/Aim by reaction |
| chasing | `:857` | Fire/Aim within `fireRange`; `:859` StopFire otherwise |
| trap recovery | `:948` | None |

`combat` is the intent, not the shot. Actual fire is emitted in `ExecuteIntent`: Fire only if
`m_intent.combat==Fire` and `m_blackboard.mode!=NANOMODE_CLOAK` (`BotBrain.cpp:1015-1018`), then
`BotCombat::Attack` must pass target/LOS/muzzle/ammo/reaction/fire-direction checks (`BotCombat.cpp:235-277`).
Otherwise `StopFire` runs (`BotBrain.cpp:1020`). `BotCombat::FindEnemy` also stops fire on target switch/clear
(`BotCombat.cpp:219,223`).

### strafe

| Writer | Lines | Note |
| --- | --- | --- |
| test | `:418` | test override |
| withdraw | `:725` | visible enemy |
| hold angle move | `:782` | true |
| retreat | `:800` | true |
| attacking | `:828` | true |
| chasing | `:856` | true while firing distance |
| trap | `:950` | false |

No module writes `m_intent.strafe`; `BotLocomotion` passes it to `BotMovement::Move`
(`BotLocomotion.cpp:132`), and `SetWarsBotMotion` stores it only while speed > 0 (`Player.cpp:4981`).

### linkJump

| Writer | Lines | Note |
| --- | --- | --- |
| reset | `:966` | false on every `ExecuteIntent` |
| link executor | `:1079` | true when the intent jump came from a link |

This bypasses the `JumpSpace` clearance probe in `BotLocomotion::Execute` (`BotLocomotion.cpp:114`).

### why / source-line fields

`WBI` writes `why.<field>` (`BotBrain.cpp:23`); `WBI_MOVETO` writes `why.move` and `why.aim` with the same line
(`:24`). `BotIntent::Why` has only `suitMode, jump, gait, aim, move` (`BotIntent.h:29`).

**Not covered by `why`:** `combat` (all 8 writes), `strafe` (all 7), `stance` (2), `destination` at `:1075`,
`linkJump`, `meshPath`, `arrive`, and direct `suitMode`/`jump` writes where the WBI macro was not used. The comment at
`BotBrain.cpp:21-22` says every control write records its source, but these channels have no corresponding field or
no macro call. This is the first T-1 diagnostic gap: the log cannot say why the bot fired or strafed.

## 3. Native sinks and last-writer result

### movement and aim

| Sink | Lines | What it writes |
| --- | --- | --- |
| `BotMovement::Stop` | `BotMovement.cpp:12-24` | `SetWarsBotMotion(currentRotation,0,currentForward)`, then `RequestMovement` clear/0 |
| `BotMovement::Aim` | `:26-40` | `SetWarsBotMotion(target-pos,0,target)`, request look/aim/fire/body, **stance stand** |
| `BotMovement::Look` | `:42-53` | `SetWarsBotAim(target)`, request look/aim/fire |
| `BotMovement::Move` | `:78-142` | arrive/unsafe -> Stop+Aim (`:88-93,129`); safe -> `SetWarsBotMotion(direction,speed,lookAt,strafe)` (`:130`) + request (`:131-141`) |
| `BotLocomotion::Stop` | `BotLocomotion.cpp:27-32` | `m_movement.Stop`, `SetWarsBotActions(false,false,STAND)` |
| `BotLocomotion::Execute` | `:100-141` | `m_movement.Move` or `Stop+Aim`; `SetWarsBotActions`; `RequestMovement` with stance and optional `SetJump` |

The native `CPlayer::SetWarsBotMotion` overwrites `m_warsBotAim` (`Player.cpp:4974-4985`); `SetWarsBotAim` overwrites
it too (`Player.cpp:5005-5011`). On a normal decision tick the last of these is `SetWarsBotMotion`/`SetWarsBotAim`
called by `BotLocomotion` after `ApplyLookPolicy`. With `wb_bot_turn_cap=1`, `BotLocomotion` receives the `capped`
copy, so its aim is the capped `m_lookDir`; without it, the state aim is used raw. On non-decision frames
`UpdateTurnCap` is the only aim writer (`BotBrain.cpp:1185`).

If `!m_blackboard.canMove`, `BotLocomotion::Execute` calls `Stop` immediately (`BotLocomotion.cpp:103`), so the state
`move`/`aim` intent is ignored that tick; `m_blackboard.blocked` is not updated by this path. This is the freefall /
ragdoll / no-move case and is why a jump or aim request can have no visible effect.

### fire

| Sink | Lines | What it writes |
| --- | --- | --- |
| `BotCombat::StopFire` | `BotCombat.cpp:36-42` | `IWeapon::StopFire`, clears `m_firingWeapon` |
| `BotCombat::Attack` | `:235-277` | checks enemy/LOS (`:238`), muzzle ray (`:240-254`), reload/ammo (`:255-256`), fire direction dot >=0.96 (`:260`), fire control (`:263-270`); `StartFire` (`:272`), pulse `StopFire` (`:273`) |
| `BotWeapons::ReloadIfNeeded` | `BotWeapons.cpp:238-249` | `IWeapon::Reload` (`:245`) |
| `BotWeapons::SelectAvailable` | `:63-81` | `SetActorItem` (`:77`) |
| `BotWeapons::SelectForRange` | `:216-236` | `SetActorItem` (`:233`) |

`BotBrain` never calls `StartFire` directly; only `ExecuteIntent` calls `Attack` (`BotBrain.cpp:1015-1018`). Thus the
fire-chain winners are: `combat` field -> cloak gate -> target exists -> `Attack` LOS/muzzle/dot/ammo/reaction ->
`CanFire`. A `combat=Fire` intent can still result in `StopFire` at any of those steps.

### suit

| Sink | Lines | What it writes |
| --- | --- | --- |
| `BotSuit::Apply` | `BotSuit.cpp:15-45` | `SetMode(mode)` (`:40`); if the current mode is Cloak and cloak is disabled, forces Defense (`:29-33`); refuses unavailable mode/energy/cooldown (`:21-40`) |
| `BotSuitPolicy::Update` | `BotSuitPolicy.h:55-72` | decision only; 1.5 s hold, energy hysteresis |
| `BotBrain::ApplySuitPolicy` | `BotBrain.cpp:1241-1271` | writes `suitMode` (`:1265-1268`), may downgrade `jump` (`:1270`) |

The actual suit is the last successful `BotSuit::Apply`; `suitMode` can be a request that is refused. The 2 s
`state.nextSuitChange` cooldown is enforced inside `BotSuit::Apply` (`BotSuit.cpp:39-42`), not by the intent.

### jump

| Sink | Lines | What it writes |
| --- | --- | --- |
| `BotLocomotion::Execute` | `BotLocomotion.cpp:107-121` | validates; `state.nextJump=now+0.5/3.0`; logs `BOT ACTION: JUMP` |
| `BotLocomotion::Execute` | `:138` | `request.SetJump()` only on acceptance |
| `BotLocomotion::Observe` | `:40-46` | `PublishWarsBotJump` on takeoff; `Player.cpp:4968` |
| `BotLocomotion::Obstacle` | `:60-69` | returns a jump decision, does not write intent |

A refused jump logs `ACTION REFUSED` and does not call `SetJump`; `m_intent.jump` remains set for that tick and is
cleared by the next tick. `linkJump` bypasses only the clearance probe (`BotLocomotion.cpp:114`), not the other checks.

### stance and action bits

`BotLocomotion::Execute` calls `SetWarsBotActions(sprint && accepted, move && accepted, intent.stance)`
(`BotLocomotion.cpp:135`; `Player.cpp:4957-4966`), then sends `SetStance`/`SetJump` (`:136-139`). So the actual action
bits are the *accepted* move/sprint plus the intent stance, not the raw intent. `BotMovement::Aim`'s internal
`SetStance(STAND)` (`BotMovement.cpp:36`) is superseded by the second request in `BotLocomotion`; a jump forces the
request stance back to STAND (`BotLocomotion.cpp:137`).

### weapon selection and zoom

| Sink | Lines | What it writes |
| --- | --- | --- |
| `BotWeapons::SelectAvailable` | `BotWeapons.cpp:63-81` | first usable primary, else current; `SetActorItem` (`:77`) |
| `BotWeapons::SelectForRange` | `:216-236` | best weapon for distance, current bonus 0.12; `SetActorItem` (`:233`) |
| `BotWeapons::PickUp` | `:151-168` | native pickup; `SelectForRange(20)` (`:166`) and `SelectAvailable` (`:167`) |
| `BotWeapons::GiveTestSCAR` | `:170-189` | `SetActorItem` (`:185`) |
| `BotWeapons::Handle` | `:251-274` | `StartZoom`/`StopZoom` (`:272-273`); LAM laser (`:258-262`) |

`SelectForRange` is called from `UpdateCombat` every 2 s (`BotBrain.cpp:692-693`) when `wb_bot_weapon_rank=1`, and
from pickup (`BotWeapons.cpp:166`). It can change the held item *after* the intent was decided but *before* `Attack`,
so the fire result can use a different weapon than the one the intent was built around. `Handle` runs after the
fire gate (`BotBrain.cpp:1021-1027`) and only with `wb_bot_weapon_handling=1`.

## 4. Which rule wins today

1. **Per field, last write wins.** `m_intent` is a plain struct reset at `:320`; there is no arbitration or "priority"
   bit. The last writer before `ExecuteIntent` is what the native layer receives.
2. **Look policy is the usual aim winner.** `ApplyLookPolicy` (`:962`) overwrites aim at `:1131` after the state
   logic, except when `wb_bot_look=0`, a target is visible, a test is active, or a vantage is held
   (`:1091-1092`). In combat with a visible target, the state aim wins; with no visible target, the path/interest aim
   wins.
3. **Link executor overrides state movement when a link is active.** With `wb_bot_jump_links=1`,
   `ApplyLinkExecutor` (`:967`) writes move/destination/gait/jump/suit at `:1074-1081` after state logic; otherwise
   `m_link.Reset()` and state movement stands. An active link is the last movement writer before `BotLocomotion`.
4. **Suit policy is not literally last.** `wb_bot_suit_policy=1` writes at `:1265-1268`, but corner slow (`:995`),
   suit-not-ready (`:999`) can still overwrite it, and `BotSuit::Apply` can refuse the request. The actual mode is
   whatever `SetMode` accepts.
5. **Trap recovery overrides combat first.** `RecoverTrap` runs first inside `ExecuteIntent` (`:961`); at level 2-3
   it sets `combat=None` (`:948`) and writes move/gait/suit/jump. It returns false after applying the trap step, so
   later look/link/suit/corner writers can still modify the trap intent. At level 4 it calls native `KillPlayer` and
   returns true (`:934-943`).
6. **Native validation has the final say for jump/movement.** `BotLocomotion` can refuse a jump, stop on no-move, or
   reject an unsafe step; `BotMovement::Move` can stop 0.8 m short (or 0.2 with corners). An intent is a request, not a
   guarantee.
7. **Fire is a two-stage winner.** `combat=Fire` only reaches `Attack` if not cloaked; `Attack` then applies LOS,
   muzzle, ammo, dot, reaction and `CanFire`. `combat` alone does not fire.
8. **`why` is incomplete.** Only suit/jump/gait/aim/move have a source line. Combat, strafe, stance, destination,
   linkJump, meshPath, arrive and direct native calls are untraced; the `INTENT WHY` log can be misinterpreted as
   "the only writers". T-1 should add `combat`, `strafe`, `stance` and `destination` source fields before using the
   log as the refactor map.

## 5. Quick check list for T-1

- Add `why.combat`, `why.strafe`, `why.stance`, `why.destination` to `BotIntent::Why` (`BotIntent.h:29`) and use WBI
  for every direct assignment listed in section 2.
- Keep one executor: every early-return state already calls `ExecuteIntent` (test, weapon seek, upgrade, roam,
  vantage, combat post), so the T-1 frame can stay a single "decide -> execute" pass.
- Move `ApplyLookPolicy` from "inside ExecuteIntent" to an explicit aim channel with a recorded source, otherwise it
  silently outranks the state aim whenever no target is visible.
- Keep `BotSuit::Apply` and `BotLocomotion::Execute` as validation/commit, not as decision; record both requested and
  accepted mode/jump (`BOT SUIT`, `BOT ACTION: JUMP`, `ACTION REFUSED`) for the T-1 regression.
- Do not add a second movement writer for the turn-cap path: `CappedLook` already supplies a capped copy
  (`BotBrain.cpp:1002-1005`); a second `SetWarsBotAim` after locomotion would reintroduce the DS-013 ordering bug.
