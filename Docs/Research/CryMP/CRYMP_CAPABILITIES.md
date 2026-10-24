# CryMP capabilities for WarsBots: source audit

Branch `warsbots/research/crymp-native-audit`, based on `warsbots/feature/weapon-goals` @ `d32e1b9a`.
This is a read-only research document. No game code, build configuration or shared document was changed.

Principle: **CryMP is the foundation.** For every subsystem this file records what the engine
already does for an actor with `channelId == 0`, where it does it, and what WarsBots still has to
write. Applicability claims are graded:

| Grade | Meaning |
|---|---|
| **SRC** | Proven by reading the open source call chain in this repository. |
| **LIVE-IA** | Observed in earlier WarsBots InstantAction playtests (per project history). Not re-run here. |
| **UNPROVEN** | Depends on closed code (CryAction.dll RMI/net, CryAISystem.dll) or on multiplayer replication not yet tested. |
| **NO** | Proven not usable as-is. |

Tools used: [`Tools/CryMPResearch/lua_channel_audit.py`](../../../Tools/CryMPResearch/lua_channel_audit.py)
(effective Lua resolution + call graph + channel sinks) and
[`Tools/CryMPResearch/level_inventory.py`](../../../Tools/CryMPResearch/level_inventory.py)
(navigation exports and objective entities of installed levels). Generated, redacted outputs are in
[`generated/`](generated/).

## 0. Provenance: what is open, what is closed

| Layer | Status | Evidence |
|---|---|---|
| CryGame (GameRules, Actor/Player, NanoSuit, Items/Weapons, Vehicles movement, ShotValidator, HUD) | Open, built by CryMP | `Code/CryGame/**` |
| CryAction subsystems: ActorSystem, ItemSystem, EquipmentManager, VehicleSystem/Vehicle/VehicleSeat, GameRulesSystem, GameplayRecorder, CallbackTimer, LevelSystem | Open *reimplementations*, patched into the original DLL | `Code/CryAction/*.cpp`; e.g. `GameObject.cpp` patches at `CRYACTION_BASE + offset` |
| RMI dispatch (`IGameObject::DoInvokeRMI`), GameServerChannel/NetContext, `IsChannelOnHold`, aspect delegation | **Closed** (original `CryAction.dll`/`CryNetwork.dll`) | `CryCommon/CryAction/IGameObject.h:271-348` only declares; no implementation in repo |
| CryAI (`CryAISystem.dll`) | **Closed**; CryMP patches the MP check out | `Launcher/MemoryPatch.cpp:205` `AllowMultiplayerAI`, commit `f0c6f3aa` "Allow AI.RegisterWithAI in multiplayer"; `CryAction/ScriptBind_AI.cpp` is a 16-line stub |
| Game Lua | Effective set = `Pak/CryMP/Scripts` > `Pak/Scripts` > ZPatch1.pak > GameData.pak | `CryScriptSystem/ScriptSystem.cpp` ExecuteFile `CryMP/` override; `CrySystem/CryPak.cpp` `AddFileAlternative` priority; see `generated/lua_channel_audit.md` "Effective sources" |

Consequence: every statement about what a per-client RMI does when addressed to channel 0 is
**UNPROVEN** until tested, because the dispatcher is closed.

## 1. The single most important fact: `IsPlayer()` means "has a channel"

`CryCommon/CryAction/IActorSystem.h:123`:
`IActor::IsPlayer()` returns `GetChannelId() != 0`.

So a WarsBots actor is, to the whole engine, **an AI actor of the Player class**. CryEngine 2
code was written with exactly that kind of actor in mind (single-player AI soldiers are
channel-less `Player`-class actors), and therefore contains *native AI branches* that apply to our
bots automatically:

* `CActor::IsPlayerClass()` (`Actors/Actor.h:1298`) stays true, so `ShouldUseMPParams()`
  (`Actor.cpp:454`) still gives bots multiplayer movement/health parameters. **SRC**
* Vanilla per-client notifications are guarded by `IsPlayer()` before they address a channel:
  `BasicActor.Server:OnHit` (`Pak/CryMP/Scripts/Entities/Actor/BasicActor.lua:1500-1507`) and the
  C++ binding `CScriptBind_GameRules::SendHitIndicator` (`ScriptBind_GameRules.cpp:2300-2304`).
  The same idiom is the correct, native way to guard any new bot-facing call. **SRC**
* Gameplay rules that use `IsPlayer()` as a filter silently skip bots (scores, PP, capture HUD).
  These are listed in [CHANNEL_ZERO_AUDIT.md](CHANNEL_ZERO_AUDIT.md). **SRC**

## 2. Subsystems

### 2.1 Actor creation, registration, identity

| | |
|---|---|
| Capabilities | `ActorSystem::CreateActor(channelId=0, name, class, pos…)` spawns a networked `Player` actor; every actor gets `ENTITY_FLAG_TRIGGER_AREAS` (`CryAction/ActorSystem.cpp:123`), channel-less actors are not `NEVER_NETWORK_STATIC` (`:129-132`). `LOCAL_PLAYER_ENTITY_ID` is only used for a local channel (`:101-104`). |
| Files/functions | `CryAction/ActorSystem.cpp` CreateActor/CreateActorHook; `CryGame/Bots/CryMPEngineAdapter.cpp` CreateServerActor; `Bots/BotInstance.cpp:40-60` (direct creation, avoids `SpawnPlayer` lookup by channel 0). |
| Reuse | Already used. **LIVE-IA**. |
| Limits | No `INetChannel` → no client-side input, no per-client RMIs, no `ShotValidator` queue, no voice. `GetActorByChannelId(0)` is meaningless. |
| Write ourselves | Nothing new; keep EntityId + owned registry as identity (unchanged WarsBots invariant). |

### 2.2 GameRules, teams, participation, stats

| | |
|---|---|
| Capabilities | `CGameRules::SetTeam` (`GameRules.cpp:1721-1801`) puts **every actor** into `m_entityteams` and `m_playerteams` (`isplayer = pActor != 0`, `:1737`), so native `GetTeam`, `GetTeamPlayers`, `GetTeamPlayerCount` already include bots. **SRC**. `CGameRules::ChangeTeam(actor, team)` on the server calls Lua `Server:OnChangeTeam` directly (`GameRules.cpp:1286-1299`) — the native, entity-keyed join path that PS uses for humans too. **SRC**. `RevivePlayer` (`:1003`), `KillPlayer`, `ServerHit/ProcessServerHit` (`GameRulesClientServer.cpp:220-273`) work on actors, not channels. Revive/kill **LIVE-IA**. |
| Files/functions | `GameRules.cpp` SetTeam, ChangeTeam, RevivePlayer, KillPlayer, GetTeamChannelCount (`:1668`), UpdateObjectivesForPlayer (`:1904`); `ScriptBind_GameRules.cpp` GetPlayers (`:501`), GetTeamPlayers (`:1326`), SendTextMessage (`:1522`); WarsBots `GetMatchParticipants` (`GameRules.cpp:1383`). |
| Reuse | Team membership, team counts by entity, team-kill detection (`TeamInstantAction:IsTeamKill` uses `GetTeam`), native kill/revive. |
| Limits | `SetTeam` also writes `m_channelteams[0]` and sends objective RMIs to channel 0; `GetTeamChannelCount`, `IsTeamLocked`, `PlayerCountOk` count connections only; WarsBots `BotMatch::IsBot/Collect` return false unless the rules class is `InstantAction` (`Bots/BotMatch.cpp:14,23`), so in PS `GetPlayers()` excludes bots. Score/PP awards skip non-`IsPlayer` shooters. |
| Write ourselves | A PS-aware participant predicate, a guarded SetTeam side-effect path (proposal P2 in [INTEGRATION_OPPORTUNITIES.md](INTEGRATION_OPPORTUNITIES.md)), bot-aware scoring hooks. |

### 2.3 Movement, character control, nanosuit, jumps

| | |
|---|---|
| Capabilities | `CPlayerMovementController::RequestMovement(CMovementRequest&)` is the **native AI → actor interface** (look/aim/fire/body target, stance, desired speed, pseudo-speed, strafing, jump). It has explicit non-player branches: forwarding to a vehicle passenger controller (`Actors/Player/PlayerMovementController.cpp:103-115`), AI aim cone 70° vs 180° for players (`:1080`), eye direction handling (`:1360-1371`). `CPlayerMovement` turns it into physics; nanosuit modes via `CNanoSuit::SetMode`, energy rules native (`NanoSuit.cpp`; WarsBots forces `isAI=false` at `:426` so bots get MP recharge/medical rules). |
| Files/functions | `PlayerMovementController.cpp`, `PlayerMovement.cpp`, `Player.cpp` (WarsBots motion/sprint replication `:4907-5250`), `NanoSuit.cpp`, `Bots/BotMovement.cpp`, `Bots/BotLocomotion.cpp:125-128`, `Bots/BotSuit.cpp`. |
| Reuse | WarsBots already uses `RequestMovement` for aim/stance/jump and native nanosuit; sprint/Speed and Strength jumps go through native suit energy. **LIVE-IA**. |
| Limits | `PlayerInput`/`NetPlayerInput` do not exist for channel 0 (actions come from input devices or the network). Sprint requires `m_actions` bits, which WarsBots sets through its own `SetWarsBotActions` hook — that hook *is* the replacement for absent input and must stay. |
| Write ourselves | Nothing new for PS infantry. For vehicles, see 2.6. |

### 2.4 Navigation and AI interfaces

| | |
|---|---|
| Capabilities | CryAISystem is loaded (`Launcher.cpp:1191`) and CryMP disables its multiplayer check (`MemoryPatch.cpp:205`), so `AI.RegisterWithAI` works in MP. Headers expose the graph/navigation API (`CryCommon/CryAISystem/IAISystem.h:815` `IsSegmentValid`, `IAgent.h:1849` `GetEnclosing`, hidespots, generic shapes `:773-774`). |
| **Measured limit** | All five stock PS maps ship **header-only navigation exports**: `netmission0.bai` 96 B, `hidemission0.bai` 96 B, `areasmission0.bai` 36 B, `roadnavmission0.bai` 8 B, `waypt3dsfcmission0.bai` 8 B on Mesa, Beach, Plantation, Refinery and Shore (`generated/level_inventory_*.md`). The mission XMLs contain no `AIAnchor`, `TagPoint`, `AIPath` or `AINavigationModifier` entities. → CryAI pathfinding, cover/hidespots and road navigation have **no data** on PS maps. **NO** (as shipped). |
| Reuse | Generic shapes and `AreaShape` entities (92 on Mesa) are usable as *map semantics* through the entity/area system, not CryAI. |
| Write ourselves | Keep WarsBots `BotNav` (own graph/memory per map+version). Regenerating CryAI navigation would require the Sandbox editor and modified level paks for every client — out of scope and a compatibility risk. |

### 2.5 Weapons, firing, damage, ShotValidator

| | |
|---|---|
| Capabilities | `IWeapon::StartFire/StopFire`, `IFireMode` (ammo, reload, spread, recoil, rate) are fully native; WarsBots calls them (`Bots/BotCombat.cpp:32-36`, `Bots/BotWeapons.cpp`). Server-created bullets reach `CGameRules::ServerHit → ProcessServerHit` like any other (`GameRulesClientServer.cpp:220-273`). `CWeapon` has AI branches: non-player Drop handling (`Items/Weapons/Weapon.cpp:1098`), no rocket auto-drop for AI (`:2813`), dual-SOCOM AI only in SP (`:1260-1264`). `CMovementRequest::SetFireTarget/SetAimTarget` drive native aim. |
| ShotValidator | Indexes shots/hits **by channel** (`ShotValidator.cpp:36-160`); `Connected/Disconnected(channelId)` from GameRules (`GameRules.cpp:581,633`). WarsBots adds `CanHit` acceptance for owned server-side projectiles (`ShotValidator.cpp:233-239`) and rejects other channel-0 hits (`:101-106`). |
| Reuse | Entire weapon stack; damage tables and hit types unchanged (required by the "no damage changes" rule). |
| Limits | Vehicle weapons (`VehicleWeapon.cpp`) and mounted guns have their own fire paths not yet exercised by bots. TAC (HQ damage) requires the TAC launcher/vehicle and an `explosion` hit of type `tac` (HQ.lua, GameData). |
| Write ourselves | Weapon choice per objective (anti-vehicle, TAC). No new firing primitive. |

### 2.6 Vehicles

| | |
|---|---|
| Capabilities | **Native AI driving exists.** `CVehicleMovementBase::ProcessMovement` calls `ProcessAI()` whenever the driver is not `IsPlayer()` (`Vehicles/Movement/VehicleMovementBase.cpp:346-355`). `ProcessAI` consumes a `CMovementRequest` (move target, desired speed, look target) given to `IVehicleMovement::RequestMovement` — implemented for wheeled (`VehicleMovementStdWheeled.cpp:1421,1714`), tank (`VehicleMovementTank.cpp:424,472`), boat, helicopter, VTOL, hovercraft/warrior. Passenger/gunner aiming is forwarded by the player movement controller (`PlayerMovementController.cpp:105-115`). Seats: `CryAction/Vehicles/VehicleSeat.cpp` (open reimplementation). |
| Reuse | Driver steering/throttle/brake loop; seat enter/exit; gunner aim. |
| Limits | **UNPROVEN** in MP: network authority for a channel-less driver (client authority is normally delegated to the driver's channel), smoothing on clients, vehicle weapon firing by AI. No road/path data (2.4). |
| Write ourselves | Vehicle route planning on top of BotNav, unstuck logic, claiming/ownership via PS rules. |

### 2.7 Items, equipment, pickups

`EquipmentManager` and `ItemSystem` are open reimplementations (`CryAction/EquipmentManager.cpp`,
`ItemSystem.cpp`); `GiveEquipmentPack`, `CItem::CanPickUp` and inventory work for bots
(**LIVE-IA**, `Bots/BotWeapons.cpp:96-128`). PS purchases go through Lua `PowerStruggle.Server:SvBuy`
(ZPatch1 Buying.lua), which is entity-keyed except for its final `ClBuyOk/ClBuyError(channel)` RMI.

### 2.8 Areas, triggers, objectives

All actors trigger areas (2.1), so `Server:OnEnterArea/OnLeaveArea` of capture, buy and service
zones fire for bots. PowerStruggleUtils `MakeCapturable` replaces those callbacks and inserts
`entity.id` into `self.inside` **without an IsPlayer filter**; `UpdateCapture` counts living,
non-spectator occupants per team by `GetTeam(playerId)`. → **Bots capture natively.** **SRC**
(GameData PowerStruggleUtils.lua, `MakeCapturable` OnEnterArea/UpdateCapture; not reproduced).
Mission objective HUD (`ClSetObjective`) is per-channel and irrelevant for bots.

### 2.9 Auto-turrets

`CGunTurret::IsTargetHostile` decides by species and `GetTeam(entity)` (`Items/Weapons/GunTurret.cpp:590-597`).
Team-assigned bots are therefore shot by enemy base turrets and ignored by own ones. Every PS map
has 9–18 `AutoTurret` and up to 10 `AutoTurretAA` (`generated/level_inventory_*.md`). Bots must
treat turret coverage as danger zones. **SRC**

### 2.10 Events, timers, debugging

* `CGameRules` emits GameplayRecorder events (`eGE_ChangedTeam` `GameRules.cpp:1800`, kills, hits)
  that listeners can consume without touching Lua — usable for bot telemetry/commander input.
* `CryAction/CallbackTimer.cpp` (open) and Lua `SetTimer` are available.
* `PersistantDebug`, `DebugHistoryManager` are open and can draw bot routes/goals for debugging.

### 2.11 Spectator (research only — implemented by Codex)

Entry points: `CGameRules::ChangeSpectatorMode` (`GameRules.cpp:1254`), RMI `SvRequestSpectatorMode`
(`GameRulesClientServer.cpp:1635`), `CActor::ClSetSpectatorMode` (`Actor.cpp:4058`); `CPlayer`
sends it to all clients (`Player.cpp:6472,6494`) and once to its own channel (`:6518`, channel 0
for a bot). Lua keeps `channelSpectatorMode[channelId]` (IA). For PS: `PowerStruggle.Server:OnChangeSpectatorMode`
resets PP/CP/revive when `resetAll` — a bot must never be put into spectator mode by PS's
"dead > 5 s" rule; that rule enumerates `GetPlayers()` (`PowerStruggle.lua:941-947`), which in PS
currently excludes bots (2.2), so it is safe today *by accident*. No spectator code was touched.

### 2.12 HUD, radar, radio, voice

Per-client: `ClTempRadarEntity/ClTaggedEntity` (`GameRules.cpp:926-937`), objectives, `ClHitIndicator`.
Radio only for `IsClient()` (`GameRules.cpp:1777`). Voice groups via `ReconfigureVoiceGroups(id)`
(entity-based, closed NetContext voice). None of these give bots useful information; bots need
local events instead.

## 3. Summary matrix

| Area | Native for channel-0? | Grade | WarsBots must write |
|---|---|---|---|
| Spawn/identity | yes | LIVE-IA | — |
| Team membership/count by entity | yes | SRC | PS participant predicate |
| Join team (`ChangeTeam` → Lua) | yes, with channel side-effects | SRC + UNPROVEN RMIs | guard side-effects |
| Revive waves (PS queue) | yes, entity-keyed | SRC | queue request + group choice |
| Capture | yes | SRC | objective selection |
| Buying | yes except final RMI | SRC + UNPROVEN RMI | guarded server call |
| Scoring/PP/rank awards | **no** (IsPlayer gate) | SRC | bot-aware award hook |
| Movement/aim/stance/jump | yes (`CMovementRequest`) | LIVE-IA | — |
| Sprint | needs WarsBots action hook | LIVE-IA | keep hook |
| Navigation (CryAI) | **no data on PS maps** | NO | keep BotNav |
| Weapons/damage | yes | LIVE-IA | weapon/objective choice |
| Vehicle driving | native AI path exists | SRC + UNPROVEN net | route planner |
| Turrets | team-aware | SRC | danger avoidance |
| HUD/radar/radio | n/a for bots | — | local events |
