# PS T-0 contract: modes, 3–4 bots per team, vehicles, PS minimum

Date: 2026-10-26. Owner decisions D-018/D-019: IA first, PS designed in parallel; **3–4 bots per team, built to
scale**; modes with and without vehicles; **entering/exiting a vehicle is mandatory** in PS v1. This is the one-page
contract. Details: `T0_GAMEMODE.md` (interface/team/spawn/registration), `VEHICLES.md` (entry/exit/driving),
`PS_MIN.md` (first five minutes + acceptance tests), `PROCESS-CYCLE-2026-10-26.md` (v5 process).

## 1. What must exist before PS code

1. `IBotGameMode` with `InstantActionMode`, `TeamInstantActionMode`, `PowerStruggleMode`; no `strcmp("InstantAction")`
   decisions in BotMatch/BotManager/BotMoveTrace/CryMPEngineAdapter (`T0_GAMEMODE.md` §4).
2. A team slot per bot, assigned before spawn: 3 or 4 per team, balanced by lowest count, capped by `wb_maxbots`:
   `teamCount=2`, `perTeam=wb_ps_bots_per_team` (3/4), `desired=teamCount*perTeam`, `limit=min(desired,wb_maxbots)`.
3. `SetTeam` + `SetPlayerSpawnGroup` after actor creation and before `ChooseSpawn`; `GetSpawnLocation` filters by
   `GetTeam(id)` (`GameRules.cpp:2473-2481`). `BotMatch::Register` must stop refusing `USE_SPAWN_GROUPS`
   (`BotMatch.cpp:34-35`). PS spawn groups anchor at bunkers/HQ: `SpawnGroup2` tan / `SpawnGroup_US5` black on Shore,
   `SpawnGroup_US`/`SpawnGroup2` on Plantation (`PS_MAPS.md:44-47,67-69`); use the team default group, never the
   enemy HQ group.
4. A bot-specific team path: `SetTeam` writes shared `m_channelteams[0]` and sends objectives/voice for every
   channel-0 bot (`GameRules.cpp:1721-1801,1904-1915`). Either add a bot-specific path or forbid `GetChannelTeam(0)` in
   bot decisions; test two bots on different teams.
5. PS participant registration: `PowerStruggle.lua` hooks + score keys; no fake net channel, bot remains
   `channelId=0`, `IsPlayer=false` (`BotInstance.cpp:288,371`).
6. `wb_ps_vehicles` (default 1 for PS v1, 0 = no-vehicle mode); vehicle entry/exit behind the T-0 API.

## 2. Interface (minimum)

```cpp
struct BotSlot { int number; BotSkill skill; BotStyle style; int team; };
struct BotSpawn { uint32_t location, group; float pos[3], rot[3]; };
class IBotGameMode {
    virtual const char* Name() const = 0;
    virtual int TeamCount() const = 0;
    virtual bool IsTeamBased() const = 0;
    virtual bool IsPowerStruggle() const = 0;
    virtual bool SupportsVehicles() const = 0;
    virtual int DesiredBotsPerTeam(int requested) const = 0;
    virtual int AllocateTeam(int number, const int* assignedPerTeam, int teamCount) const = 0;
    virtual bool IsParticipant(uint32_t id) const = 0;
    virtual void CollectBots(std::vector<uint32_t>&) const = 0;
    virtual bool RegisterParticipant(uint32_t id, const BotSlot&) = 0;
    virtual void UnregisterParticipant(uint32_t id) = 0;
    virtual bool ApplyTeam(uint32_t id, int team) = 0;
    virtual int TeamOf(uint32_t id) const = 0;
    virtual BotSpawn FirstLocation(int team) const = 0;
    virtual BotSpawn ChooseSpawn(uint32_t id, int team, uint32_t avoid) = 0;
    virtual bool PlaceAtSpawn(uint32_t id, const BotSpawn&) = 0;
    virtual void Equip(uint32_t id, const BotSlot&) = 0;
    virtual const char* AttachScript() const = 0;
    virtual bool CanInspect() const = 0;
    virtual BotScoreKeys ScoreKeys() const = 0;
};
```

The interface is engine-free; the adapter wraps `CGameRules`/`IActorSystem`. Call order:
`AllocateSlot → FirstLocation → CreateServerActor → RegisterParticipant → ApplyTeam → ChooseSpawn → PlaceAtSpawn →
Equip`. `GetSpawnLocation` needs the actor id, so the old `AddWith` pre-actor spawn (`BotManager.cpp:331-339`) must be
reordered.

## 3. Vehicle entry/exit contract

- **Entry:** `IVehicleSystem::GetVehicle`, `IVehicle::GetSeatById(1)` (or a free seat), `IVehicleSeat::Enter(botId,
  false)` (`IVehicleSystem.h:645,1167`). The proven server path is the revive-in-vehicle code:
  `Actor.cpp:3763-3770` calls `pSeat->Enter(GetEntityId(), false)` for a channel-0 actor.
- **Permission:** PS `CanEnterVehicle` checks owner/team (`ZPatch1...PowerStruggle.lua:571-584`); apply it before
  entering. A team-0 bot or an enemy vehicle is refused.
- **Exit:** `IVehicleSeat::Exit(false,false)` (`:1168`) or `IVehicle::OnAction(eVAI_Exit,...)`; fallback
  `IVehicle::GetExitPositionForActor` (`:809`); verify `GetLinkedVehicle()==null` and a free seat.
- **Drive:** `IVehicle::OnAction(eVAI_MoveForward/TurnLeft/Brake, eAAM_OnHold/OnRelease, value, botId)`
  (`:603,70-100`); `GetMovement()->StartEngine(botId)/ResetInput()` (`:686,953,959`). Do not call
  `SetWarsBotMotion` while linked; one pose owner.
- **Classes:** Shore has placed `Asian_ltv`/`US_hovercraft`/`US_smallboat`/`Civ_*`; factories build
  `US_ltv`/`Asian_truck`/`US_apc`/`Asian_apc`/tanks; Plantation has placed `Asian_ltv`/`Civ_car1` and only the
  Prototype factory. Full table in `VEHICLES.md` §5; buy id→class map `PowerStruggleBuying.lua:76-99`.
- **No-vehicle mode:** `wb_ps_vehicles=0` clears the vehicle goal, rejects entry, never buys vehicles; same map/bots.

## 4. PS v1 minimum checklist

| # | Behaviour | Acceptance |
| --- | --- | --- |
| 1 | Team spawn | 3/3 or 4/4; own `GetTeam(id)`; no enemy HQ/spawn within 15 m |
| 2 | Buy SCAR | 150 PP, enter own buy zone, one successful `BuyItem("macs")`, inventory SCAR, PP −150, no Lua nil error |
| 3 | Capture | nearest neutral/enemy node; after its `captureTime`, `GetTeamId` changes; no progress while contested |
| 4 | Defend | own HQ/factory attacked → return/fight; resume capture after 8 s clear |
| 5 | Vehicle driver | enter seat 1, drive 50 m, exit 10–20 m before node; no linked vehicle after exit |
| 6 | Vehicle passenger | seat 2 entry/exit, not driver, valid exit position |
| 7 | Vehicles off | same run with `wb_ps_vehicles=0`, no vehicle goal/enter/buy |
| 8 | First 5 min | no crash/trap cluster; each bot has a primary; ≥1 capture attempt/team; no enemy spawn |
| 9 | Honesty | only LOS/last-seen/static map; no enemy PP/health through walls |

PS v1 does not include commander orders, full economy, TAC/HQ destruction, air/boat driving or vehicle combat. Those
are later T-9/T-10 work; the first five minutes are buy → capture → defend → optional land vehicle.

## 5. Tests

- `BotGameModeTests` (engine-free): IA 6 FFA; PS 3/3 and 4/4, no 5th/team; team spawn filter; `SetTeam` before spawn;
  score keys; two teams share channel 0 but own `GetTeam`; missing group fallback; vehicles-off.
- `BotVehicleTests`/server probe: driver/passenger entry, exit position, enemy/owner refusal, 50 m road drive, destroyed
  vehicle unlink, `wb_ps_vehicles=0`.
- PS probes: `ps_team_spawn`, `ps_buy_scar`, `ps_capture_neutral`, `ps_capture_contest`, `ps_vehicle_driver`,
  `ps_vehicle_passenger`, `ps_vehicles_off`, `ps_first_5min`.
- IA regression: 6-bot IA before/after T-0; kills/stalls within noise; no `IA registration refused`.
