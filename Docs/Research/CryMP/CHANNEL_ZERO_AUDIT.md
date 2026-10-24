# channelId = 0 audit

Read-only research. Lists every place found in Power Struggle-relevant code where a WarsBots
actor (EntityId valid, `channelId == 0`, `IsPlayer() == false`) meets channel-dependent logic.
Generated Lua evidence: [`generated/lua_channel_audit.md`](generated/lua_channel_audit.md)
(219 sink rows; PAK lines are cited by location only).

## 1. Ground truth

* `IActor::IsPlayer()` ≡ `GetChannelId() != 0` (`CryCommon/CryAction/IActorSystem.h:123`).
* Real client channels are non-zero: `ActorSystem::CreateActor` queries `GetChannel(channelId)`
  only for a non-zero id (`CryAction/ActorSystem.cpp:95-113`), and every `IsPlayer()` test in the
  engine relies on that. So **key 0 never collides with a human**; it collides only *between bots*.
* The RMI dispatcher (`IGameObject::DoInvokeRMI`, `IGameObject.h:271-348`) is closed. What
  `eRMI_ToClientChannel` with channel 0 does in the shipped binary is **not provable from source**.
  The later CryENGINE lineage looks the channel up in the server nub and does nothing when it is
  absent; that is a plausible expectation, not evidence.
* Vanilla code treats channel-less actors as AI and guards per-client RMIs with `IsPlayer()`
  (`BasicActor.lua:1500-1507`, `ScriptBind_GameRules.cpp:2300` and `:2324`). PS-specific code
  (revive waves, buying, factories, capture HUD) has **no such guards**, because in the original
  game only humans could reach it.

## 2. Classification

| Class | Meaning | Bot effect | Action |
|---|---|---|---|
| **E** | state keyed by EntityId | correct | reuse |
| **B** | broadcast RMI / synched global | correct | reuse |
| **C-RMI** | RMI addressed to `GetChannelId(bot)` = 0 | dropped or undefined (closed) | guard with `IsPlayer()`, bot gets a local event |
| **C-KEY** | table/map keyed by channel | all bots share slot 0 | guard writes; never read bot state from it |
| **C-COUNT** | counts connections | bots invisible (or one phantom) | replace with participant count where gameplay needs it |
| **P** | `IsPlayer()` filter in gameplay | bots silently skipped | decide per rule: include bots or keep human-only |
| **ENUM** | `GetPlayers()` | bots excluded in PS today | decide per call |

## 3. Findings: C++ (open source, citable)

| # | Location | Class | Trigger for a bot | Risk |
|---|---|---|---|---|
| C1 | `CGameRules::SetTeam` → `UpdateObjectivesForPlayer(GetChannelId(id))` `GameRules.cpp:1755,1904-1916` | C-RMI | every team assignment | low/unknown |
| C2 | `CGameRules::SetTeam` `m_channelteams[0]=team` `GameRules.cpp:1764-1774` | C-KEY | every team assignment | **medium**: one phantom "connection" whose team is that of the last-assigned bot; erased by `SetTeam(0)` of *any* bot, even while other bots remain (last writer wins) |
| C3 | `GetTeamChannelCount` `GameRules.cpp:1668` (iterates `m_channelteams`) | C-COUNT | TIA `PlayerCountOk`, PS `IsTeamLocked` | **high**: bot-only team blocks match start (`g_minteamlimit` default 1, `GameCVars.cpp:335`); C2 adds ±1 skew |
| C4 | `GAMERULES_INVOKE_ON_TEAM` (`GameRules.h:47-56`) used by `SetObjectiveStatus/SetObjectiveEntity` `GameRules.cpp:1846,1858` | C-RMI | every PS objective change while a bot is in the team (`m_playerteams` contains bots) | unknown, frequent |
| C5 | `AddTaggedEntity` `GameRules.cpp:910-937` loops `m_playerteams` → `ClTempRadarEntity/ClTaggedEntity` to each member's channel | C-RMI | a human teammate scans/tags an enemy | unknown, frequent |
| C6 | `MovePlayer` → `ClMoveTo` `GameRules.cpp:1249` | C-RMI | only if a rule moves the bot | low |
| C7 | `SendTextMessage(type,msg,ToClient,playerId)` → `GetChannelId` `ScriptBind_GameRules.cpp:1536-1544,1586` | C-RMI | buy errors, team-change limits, factory messages, unclaimed vehicle | low |
| C8 | `ScriptBind_GameRules` EMP → `CPlayer::ClEMP` to `GetChannelId()` `:2466` | C-RMI | bot hit by EMP | low (suit effect itself is applied server-side first) |
| C9 | `CPlayer` spectator → `ClSetSpectatorMode` to own channel `Player.cpp:6518`; `:272` on init | C-RMI | spectator transitions (Codex area) | low |
| C10 | `ShotValidator` per-channel queues `ShotValidator.cpp:36-160` | C-KEY | every bot shot | **handled** by WarsBots `IsOwnedProjectileHit` + `channelId==0` reject |
| C11 | `IsChannelOnHold(GetChannelId())` in `RevivePlayer` `GameRules.cpp:1027` | closed | each revive | handled in practice (**LIVE-IA**) |
| C12 | `IsPlayerInGame` (WarsBots short-circuit) `GameRules.cpp:1397-1405` | E | — | ok |
| C13 | `BotMatch::IsBot/Collect` IA-only `Bots/BotMatch.cpp:14,23` | ENUM | all PS `GetPlayers()` | by design today; must change for PS opt-in |
| C14 | `m_pRadio->SetTeam` only `IsClient()` `GameRules.cpp:1776-1777` | guarded | — | ok |

## 4. Findings: Lua (by flow)

`[CryMP]` lines are in this repository; `[ZPatch1]`/`[GameData]` lines refer to locally
inspected PAK scripts and are not reproduced.

### Join / team
| Location | Class | Note |
|---|---|---|
| PS.Server:OnChangeTeam `PowerStruggle.lua:1094` [CryMP] → SendTextMessage(ToClient, playerId) | C-RMI | team-change rate limit warning |
| PS:IsTeamLocked `PowerStruggle.lua:1022-1070` [CryMP] | C-COUNT | `g_teamlock` ignores bots; humans can be steered into a bot-heavy team or locked out |
| IA.Server:OnClientConnect `InstantAction.lua:477-504` [CryMP], TIA:639 | — | never runs for bots (no connection); do not emulate |
| IA.Server:OnClientEnteredGame `:544`, PS `:334-345` (ClSetupPlayer/ClEnterBuyZone) | — | same; bots never "enter game" through the network |
| TIA AutoTeamBalance `TeamInstantAction.lua:246-374` | E (entity counts) | **can switch bots** between teams when `g_autoteambalance` ≠ 0; PS commander must tolerate or opt bots out |

### Spawn group / revive
| Location | Class |
|---|---|
| IA.Server:RequestSpawnGroup `InstantAction.lua:1293` (GetPlayers(true) seat count) | ENUM |
| same `:1315-1318` ClSetSpawnGroup(channelId) | C-RMI |
| IA:UpdateSpawnGroupSelection `:1332-1344` ClSetPlayerSpawnGroup(channelId) | C-RMI |
| PS:QueueRevive `PowerStruggle.lua:781` `channelSpectatorMode[GetChannel()]=nil` | C-KEY (write of nil to slot 0: harmless for humans) |
| PS:ResetRevive `:870` ClReviveCycle | C-RMI |
| PS:UpdateReviveQueue `:920`, `:969-970` ClReviveCycle/ClSpawnGroupInvalid | C-RMI |
| PS:UpdateReviveQueue `:941-947` dead>5 s → RequestSpectatorTarget over GetPlayers() | ENUM (bots excluded → good) |
| PS:RevivePlayerInQueue `:986` RevivePlayer(GetChannel()) → IA:837 | parameter only; C++ revive is entity-based |
| IA.Server:OnChangeSpectatorMode `InstantAction.lua:596-604` `channelSpectatorMode[...]=mode` | C-KEY (Codex spectator area) |

### Score / Prestige / rank
| Location | Class |
|---|---|
| IA:ProcessScores `InstantAction.lua:1231,1235` | P |
| TIA:ProcessScores `TeamInstantAction.lua:415,425`; TIA:IsTeamKill `:874-878` | P |
| **PS:ProcessScores `PowerStruggle.lua:1188`** (AwardKillPP/CP only for IsPlayer shooter) | P |
| PS:ProcessVehicleScores `:1201-1222` | E (not gated: bots get vehicle-kill PP) |
| PS:AwardPPCount Buying:283-296 [ZPatch1] ClPP(channel) | E + C-RMI |
| UpdateTeamRanks Rank:163-192 [GameData] ClRank(channel) | E + C-RMI |
| TIA:CheckScoreLimit `:526` ClPlayAlert | C-RMI |

### Buying
| Location | Class |
|---|---|
| PS.Server:SvBuy Buying:455-483 [ZPatch1] ClBuyOk/ClBuyError(channelId) | E + C-RMI |
| SvBuyAmmo Buying:834-844 [ZPatch1] | E + C-RMI |
| BuyItem Buying:850-1003, BuyVehicle :1105-1113 [ZPatch1] SendTextMessage(ToClient, playerId) | C-RMI |
| OnEnter/LeaveBuyZone :1146-1171, Service zone :1211-1219 [ZPatch1] | E + C-RMI |

### Capture
| Location | Class |
|---|---|
| MakeCapturable OnEnterArea/OnLeaveArea, UpdateCapture [GameData Utils] | **E** (no IsPlayer filter) |
| MakeCapturable ClInit(channelId) on PostInitClient [GameData Utils:77] | real clients only |
| PS.Server:OnEnterCaptureArea / OnLeaveCaptureArea `PowerStruggleAlert.lua:654,664` | P (HUD only) |
| OnPerimeterBreached `PowerStruggleAlert.lua:746-751` | P/B |

### Factory / vehicle
| Location | Class |
|---|---|
| Factory:AddToQueue :720, CancelJobForPlayer :483-497, Uncapture :819, Message :384 [ZPatch1] | E + C-RMI |
| PS:UpdateUnclaimedVehicles `PowerStruggle.lua:449` SendTextMessage(ToClient, ownerId) | C-RMI |

### Win / misc
| Location | Class |
|---|---|
| TIA:PlayerCountOk `TeamInstantAction.lua:578-588` | **C-COUNT (blocker)** |
| IA:PlayerCountOk `InstantAction.lua:313` | C-COUNT |
| PS:OnGameEnd / OnHQDestroyed `PowerStruggleAlert.lua:344-373` | B |
| PS.Server:OnTimer NUKE spectate `PowerStruggle.lua:708-722` GetPlayers | ENUM (bots not forced to spectate) |
| BasicActor:DoPainSounds `BasicActor.lua:2185` | P (cosmetic) |

## 5. Rules derived from the audit

1. Identity stays EntityId + owned registry. Never add a fake channel, `OnClientConnect(0)`,
   global `IsPlayer=true`, or read anything back from a channel-keyed table for a bot.
2. Guard **C-RMI** at the source with the vanilla idiom (`IsPlayer()` / channel ≠ 0) and give the
   bot a local event instead. Prefer one guard in a CryMP-owned Lua override or a single C++
   helper over many call-site edits (see INTEGRATION_OPPORTUNITIES P2/P3).
3. **C-COUNT** in start conditions: either documented server configuration (`g_minteamlimit 0`
   for bot test servers) or an explicit participant count; never by inserting fake channels.
   Fix C2 by not writing `m_channelteams` for channel 0.
4. **P** rules are product decisions: PS kill PP/CP for bots (needed for economy), score board
   kills/deaths (cosmetic), capture HUD (irrelevant).
5. Until each C-RMI is tested on a real dedicated server with two real clients, treat it as
   *undefined*: no claims of safety from source review alone.
