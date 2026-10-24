# Power Struggle: native call chains, traced for a channel-0 bot

Read-only research. Verifies [`Docs/POWER_STRUGGLE_DESIGN.md`](../../POWER_STRUGGLE_DESIGN.md)
(owned by Codex, **not modified**) against sources and traces each PS flow.

Notation in chains: **[E]** state keyed by EntityId (bot-safe), **[C]** uses a channel id
(0 for a bot), **[P]** filtered by `IsPlayer()` (skips bots), **[B]** broadcast (bot-neutral).
Sources: `[CryMP]` = `Pak/CryMP/Scripts` in this repo (citable), `[ZPatch1]`/`[GameData]` = game
PAK scripts, inspected locally, cited by function name and line, **never copied**.
The complete machine-generated sink list is [`generated/lua_channel_audit.md`](generated/lua_channel_audit.md).

## 1. Effective script set

| Script | Effective source |
|---|---|
| GameRules/PowerStruggle.lua, PowerStruggleAlert.lua, TeamInstantAction.lua, InstantAction.lua | CryMP |
| GameRules/PowerStruggleBuying.lua | ZPatch1 |
| GameRules/PowerStruggleRank.lua, PowerStruggleUtils.lua | GameData |
| Entities/Multiplayer/Factory.lua | ZPatch1 |
| Entities/Multiplayer/HQ.lua, SpawnGroup.lua, AlienEnergyPoint.lua, BuyZone.lua | GameData |

Resolution order verified in code: `ScriptSystem::ExecuteFile` prefers `CryMP/<file>`; CryMP's
internal pak has `CLIENT_PAK` priority over game paks; ZPatch1 overrides GameData by
`AddFileAlternative` (equal priority, opened later).

## 2. Map facts (all five PS maps)

From `generated/level_inventory_*.md` (counts/positions only):

| Map | SpawnGroup | Factory | AlienEnergyPoint | HQ | AutoTurret | AutoTurretAA | BuyZone | CryAI nav data |
|---|---|---|---|---|---|---|---|---|
| Mesa | 5 | 7 | 3 | 2 | 18 | 10 | 0 | none |
| Beach | 7 | 7 | 3 | 2 | 18 | 10 | 1 | none |
| Shore | 6 | 4 | 3 | 2 | 18 | 0 | 1 | none |
| Refinery | 5 | 2 | 2 | 2 | 14 | 0 | 2 | none |
| Plantation | 4 | 1 | 2 | 2 | 9 | 0 | 2 | none |

Factories carry `captureAreaId`, `buyAreaId`, `serviceAreaId` properties; buy/service zones are
mostly *parts of factories/bunkers*, not separate entities. Plantation and Refinery are the smallest
objective graphs: the best candidates for a first prototype.

## 3. Flows

### 3.1 Join team

```
C++ CGameRules::ChangeTeam(actor, teamId)            GameRules.cpp:1286   [E] server → Lua directly
 └ PowerStruggle.Server:OnChangeTeam(playerId, team) PowerStruggle.lua:1094 [CryMP]
    ├ TEAM_CHANGE_MIN_TIME → SendTextMessage(ToClient, playerId)          [C] (warning only)
    ├ IsTeamLocked(team, playerId)  → GetTeamChannelCount                 [C] bots invisible
    ├ KillPlayer if alive                                                 [E]/[B]
    ├ QueueRevive(playerId)         reviveQueue[playerId]                 [E]
    │    └ channelSpectatorMode[GetChannel()] = nil  (:781)               [C] slot 0, see audit
    ├ game:SetTeam(team, playerId)  → CGameRules::SetTeam                 GameRules.cpp:1721
    │    ├ m_entityteams / m_playerteams                                  [E] includes bots
    │    ├ UpdateObjectivesForPlayer(GetChannelId(id)) ClResetObjectives/ClSetObjective  [C] RMI→0
    │    ├ ReconfigureVoiceGroups(id)                                     closed, entity-based
    │    ├ m_channelteams[0] = team                                       [C] phantom slot
    │    ├ OnSetTeam (server+client Lua), CheckSpawnGroupValidity         [E]
    │    └ ClSetTeam ToRemoteClients                                      [B]
    ├ Server.RequestSpawnGroup(player, GetTeamDefaultSpawnGroup, force)   see 3.2
    └ factory:CancelJobForPlayer(playerId) for all factories              [E] (+ Message [C])
```

Verdict: **the native join path accepts an EntityId and works for a bot**, provided the four [C]
side-effects are guarded. `TeamInstantAction.Server:OnChangeTeam` (TIA:662) differs — it revives
immediately via `RevivePlayer(GetChannel())`; PS overrides it with the queued version above.

### 3.2 Spawn group selection

```
InstantAction.Server:RequestSpawnGroup(playerId, groupId, force)   InstantAction.lua:1263 [CryMP]
 ├ team check, vehicle group destroyed/submerged check               [E]
 ├ vehicle seat capacity: GetPlayers(true) count of users            [C/P] bots not counted in PS
 ├ game:SetPlayerSpawnGroup(playerId, groupId)  GameRules.cpp:2887    [E]
 ├ onClient:ClSetSpawnGroup(channelId, groupId) (:1315-1318)         [C] RMI→0
 └ UpdateSpawnGroupSelection → ClSetPlayerSpawnGroup(channel,…) (:1332-1344)  [C] RMI→0
```
`CanRevive` (IA:822) needs `player.spawnGroupId` non-null; TIA:651 adds `team ~= 0`.

### 3.3 Death and wave revive

```
CGameRules::KillPlayer → ClKill ToAllClients                          [B]
PowerStruggle.Server:OnPlayerKilled (PS:670) → TIA → IA:1066 (death_time) → ProcessScores [P]
                                         → VehicleOwnerDeath                [E]
Server:RequestRevive(playerId)   PS:756  dead > 2.5 s, not queued → QueueRevive   [E]
   (for humans this is a client RMI; for a bot the server calls it — nothing else queues a bot)
PS:UpdateReviveQueue  PS:911 (InGame tick)
 ├ per active entry: ClReviveCycle(channel,true) (:920)               [C] RMI→0
 ├ dead > 5 s & GetPlayers(): RequestSpectatorTarget (:941-947)       [C/P] bots excluded in PS
 └ at wave time: ResetReviveCycleTime, UpdateTeamRanks, for reviveQueue: [E]
      CanRevive → RevivePlayerInQueue (PS:983)
          ├ RevivePlayer(GetChannel(), player) PS:1007 → TIA → IA:837
          │     → game:RevivePlayer / RevivePlayerInVehicle → C++ CGameRules::RevivePlayer [E]
          │       (IsChannelOnHold(0): exercised by IA bots — LIVE-IA)
          ├ rank min_pp → AwardPPCount → ClPP(channel)                 [C]
          └ CommitRevivePurchases (PS:882) → BuyItem…                  [E] + [C] text
      else (no group): ClReviveCycle/ClSpawnGroupInvalid(channel) (:969-970)  [C]
```
Verdict: **the wave queue itself is entity-keyed and native**; a bot needs (a) a valid team,
(b) a spawn group, (c) a server-side `RequestRevive` after death. Do not also run IA auto-respawn.

### 3.4 Participation and Prestige (PP/CP/rank)

* `ProcessScores` gates: IA:1231/1235, TIA:415/425 (`Award` kills/deaths), **PS:1188**
  (`AwardKillPP`/`AwardKillCP` only if shooter `IsPlayer()`) — [P]. Bot kills give no PP/CP.
* `ProcessVehicleScores` (PS:1201) has **no** IsPlayer gate: bots earn PP for vehicle kills.
* `AwardPPCount` (Buying:283 [ZPatch1]) stores by EntityId [E] and sends `ClPP(channel)` [C].
* Capture rewards: `UpdateCapture` / capture completion iterates `inside` occupants and awards by
  EntityId [E] (Utils [GameData]); notifications [C].
* Ranks: `UpdateTeamRanks` (Rank:163 [GameData]) sorts team players by CP [E], `ClRank(channel)` [C].
* Team power: `PowerTick` from `Server.InGame:OnTick` (PS:1451) → `SetTeamPower` → synched global [B].

### 3.5 Buy item

```
PowerStruggle:Buy(itemName) — client wrapper, uses g_localActorId   (never call for a bot)
PowerStruggle.Server:SvBuy(playerId, itemName)   Buying:455 [ZPatch1]
 ├ team ~= 0, not frozen, ItemExists, alive|dead                       [E]
 ├ vehicle & alive → EnoughPP → BuyVehicle                             [E]
 ├ (in buy zone & not frozen) or dead → EnoughPP → BuyItem             [E]
 │     inBuyZone[playerId] set by OnEnterBuyZone (Buying:1146)          [E] (areas fire for bots)
 │     BuyItem (Buying:850) text messages to playerId                   [C]
 └ ClBuyOk / ClBuyError(channelId)                                      [C] RMI→0
```
Verdict: calling `Server:SvBuy(botId, item)` server-side runs the **complete native validation**
(team, frozen, rank/availability, zone, PP) — exactly what the design doc requires ("never call
BuyItem alone"). Only the final notification and text messages need guarding. Dead-time buying
queues equipment for the revive (`CommitRevivePurchases`).

### 3.6 Buy vehicle / factory

```
SvBuy → BuyVehicle (Buying:1105): factory in range, limits, PP, team energy
 → Factory:Buy (Factory:938) → Queue/AddToQueue (:688/:713) → ClVehicleQueued(channel) [C]
 → UpdateQueue/UpdateSlot/StartBuilding/BuildVehicle (:733/:506/:398/:593)        [E]/[B]
 → PS.Server:OnVehicleBuilt (Alert:613) → unclaimed vehicle with ownerId          [E]
 → owner enters: PS:OnEnterVehicleSeat (PS:584) claims                             [E]
 → not claimed in time: UpdateUnclaimedVehicles (PS:444) refund AwardPPCount + text [E]+[C]
```
Vehicle *use* after the claim is a separate capability (see CRYMP_CAPABILITIES §2.6).

### 3.7 Capture (energy points, bunkers/spawn groups, factories)

```
Area trigger (all actors have ENTITY_FLAG_TRIGGER_AREAS, ActorSystem.cpp:123)
 → MakeCapturable's Server:OnEnterArea (Utils [GameData]) inserts entity.id into inside [E] — no IsPlayer filter
    → g_gameRules.Server:OnEnterCaptureArea  (Alert:654) — gated by IsPlayer, HUD only   [P]
 → Server:OnUpdate → UpdateCapture: occupants alive, spectator==0, GetTeam → contest/progress [E]
 → Capture(team) / Uncapture → g_gameRules Server:OnCapture / OnUncapture (Alert:673/700)  [E]/[B]
 → building SetTeam; Factory:Capture/Uncapture (:842/:807); power point registration       [E]
```
Verdict: **bots capture natively.** The [P] gate only suppresses the capture-area HUD message.

### 3.8 Victory

```
HQ.Server:OnHit (HQ [GameData]) accepts only explosion hits of type "tac" from enemy/neutral
 → OnHQDestroyed (Alert:363) → OnGameEnd (Alert:344): SendTextMessage ToAll, ClVictory [B],
   game:EndGame, PostGame
Time limit: TIA/IA timer path → OnGameEnd (no bot dependency)
Start condition: TIA:PlayerCountOk (TIA:578) needs GetTeamChannelCount(team) ≥ g_minteamlimit
   (default 1, GameCVars.cpp:335) for every team — a bot-only team never starts the match.
```

## 4. Verification of POWER_STRUGGLE_DESIGN.md

| Claim in design doc | Result | Note |
|---|---|---|
| PS inherits TIA, spawn groups, `reviveQueue[playerId]`, `inBuyZone[playerId]` | **Confirmed** | entity-keyed |
| `RequestSpawnGroup` validates team/vehicle/seats; `CanRevive` needs group | **Confirmed**, addition | seat capacity counts `GetPlayers(true)` → bots not counted in PS |
| `SetTeam` also does objective RMI, voice, `m_channelteams[channelId]` | **Confirmed**, addition | `m_playerteams` includes *all* actors, so `GetTeamPlayers`/`GetTeamPlayerCount` already include bots |
| `GetPlayers` distinguishes participants vs connections | **Correction** | on this branch participants include bots **only when the rules class is `InstantAction`** (`BotMatch.cpp:14,23`). In PS `GetPlayers()` returns connections only |
| `SvBuy` validates team, frozen/alive, availability, buy zone, PP; `AwardPPCount` → ClPP(channel) | **Confirmed**, addition | dead actors may buy without buy zone (revive purchases); vehicles require alive |
| MakeCapturable tracks occupants by EntityId; UpdateCapture checks alive/spectator/team | **Confirmed**, addition | OnEnterArea has no IsPlayer filter and all actors trigger areas → bots capture natively |
| Factory callbacks target actor channel | **Confirmed** | ClVehicleQueued, Message, CancelJobForPlayer |
| HQ accepts only TAC explosion damage | **Confirmed** (by function reading) | |
| PS ProcessScores IsPlayer check | **Confirmed** | PS:1188; vehicle scores are *not* gated |
| "Do not assume channel 0 is harmless or that every engine RPC dereferences a channel: prove each case" | **Agreed** | dispatcher is closed; see CHANNEL_ZERO_AUDIT §3 |
| Not mentioned | **Addition** | Native join = `CGameRules::ChangeTeam` → `Server:OnChangeTeam` (entity-keyed) |
| Not mentioned | **Addition** | `PlayerCountOk`/`g_minteamlimit` blocks match start for bot-only teams; `IsTeamLocked` (`g_teamlock`) ignores bots |
| Not mentioned | **Addition** | No CryAI navigation data on any PS map |
| Not mentioned | **Addition** | Turrets are team-aware for bots; native AI vehicle driving path exists |
