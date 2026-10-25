# Power Struggle rules — for the bot commander (DS-003)

Extracted read-only from `E:\Steam\...\Crysis\Game\GameData.pak` + `ZPatch1.pak` into `%TEMP%\ps-scripts`
(not committed; Crytek scripts). Where both paks hold a file, **ZPatch1 wins**. Refs below are
`<pak>.Scripts...lua:<line>`.

## Mode / teams / states

- `PowerStruggle = new(TeamInstantAction)` which is `new(InstantAction)`;
  states `Reset/PreGame/InGame/PostGame` (`ZPatch1.pak.Scripts.GameRules.PowerStruggle.lua:17-18`).
- Two teams **tan / black** (`PowerStruggle.lua:44`); `TEAM_SPAWN_LOCATIONS=true`, `USE_SPAWN_GROUPS=true`,
  `NEUTRAL_SPAWN_LOCATIONS=false` (`:31-33`) — **this is exactly the spawn-group path `BotMatch.cpp:34-35`
  refuses today.**

## Entity classes a commander must know

`GatherEntities` (`PowerStruggle.lua:156-197`) collects, by class:
`Factory` (production/buy) · `Objective` (mission goals) · `AutoTurret` + `AutoTurretAA` (base defenses)
· `HQ` (base, destructible). Spawn groups are `SpawnGroup.lua`.

## Capture (prototype factories / objectives)

`MakeCapturable` sets the defaults (`GameData.pak.Scripts.GameRules.PowerStruggleUtils.lua:43-49`):
`captureRequirement = 2` (players in the area to capture), `captureTime = 15` s, `captureAreaId`,
`nCaptureIndex`. State machine in `UpdateCapture` (`:173-257`): contested when both teams present,
else capture/uncapture; **uncapture is faster** — rate is `1 + min(3, count - requirement)*0.33`
(`:297-311`). Capture/uncapture cascade via `UncaptureLinks("capture"/"setteam")` (`:326-328`).
Gamerules hooks for alerts/scoring: `OnEnter/LeaveCaptureArea`, `OnStart/Step/Capture/Uncapture`
(`GameData.pak.Scripts.GameRules.PowerStruggleAlert.lua:711-773`).

## Factories / bunkers / HQ

- **Factory** produces purchased vehicles/items with a `buildtime` (5–60 s) and tracks production per
  team/player (`Scripts/Entities/Multiplayer/Factory.lua`; `GetProductionFactory` in
  `PowerStruggleBuying.lua:1067,1096`). "Prototype factory" = alien tech source.
- **HQ** = base; `nHitPoints = 500` (`GameData.pak.Scripts.Entities.Multiplayer.HQ.lua:28,137`); on
  damage ≤ 0 it `Destroy()`s and calls `g_gameRules:OnHQDestroyed` (`:232-252`), which ends the match:
  `PowerStruggle:OnHQDestroyed → OnGameEnd(teamId, 1, shooterId)`
  (`PowerStruggleAlert.lua:436-444`).
- **Objective** entities encode the 4 HUD goals via `Condition` (`Objective.lua:20-33`):
  `szCaptureBuilding` (capture X), `szDestroyBuilding` (destroy X), `bProduceTAC`; re-evaluated on
  `OnCapture/OnUncapture/OnHQDestroyed/OnItemBought(tacgun)/OnVehicleBuilt(tactank)` (`Objective.lua:48-97`).
  HUD order: Obj1_CapturePT → Obj2_SecureAliens → Obj3_BuildTAC → Obj4_DestroyHQ
  (`PowerStruggle.lua:211-218`).

## Economy: PP, CP, rank, alien energy (team power)

- **PP (prestige)** per player, synched key 200 (`PowerStruggle.lua:36`). `ppList`
  (`PowerStruggleBuying.lua:176-200`): start 100, kill 100, headshot +50, melee +50, turretkill 100,
  teamkill −200, suicide 0, repair 20/1000 dmg, lockpick 25, vehicle refund ×0.75.
- **CP (career/rank points)** per player, key 201; `cpList` (`PowerStruggleRank.lua`): kill 5, turretkill 10,
  capture 10, vehicle kill min 5 / ×0.02 of price. CP gates rank.
- **Rank** 1–8, key 202 (`PowerStruggleRank.lua`): level `cp` thresholds (15/40/120/220/320/475/650) and
  `min_pp` floors; every rank equips only `SOCOM` (spawn loadout).
- **Team power (alien energy)**, keys 300+ (`PowerStruggle.lua:40`). Generated each second
  (`PowerTick`, `PowerStruggleBuying.lua:1316-1345`) from captured "powerpoints" by `powerValue`
  index 1/2/3 = 0.1125 / 0.1485 / 0.1575 per sec, scaled by cvar `g_energy_scale_income`. Spent on alien
  tech: `tacgun` energy=5 (`:65`), TAC tank energy=10 (`:71`); `@mp_AlienEnergyRequired` when short
  (`:745,931`).

## Buy list (prices in PP, `PowerStruggleBuying.lua:28-98`)

Weapons: SOCOM 50, Shotgun 50, SMG 75, FY71 125, SCAR 150, LAW 200, DSG1 200, Hurricane 250,
GaussRifle 600, AlienMount(MOAC) 300 / MOARAttach 100, TACGun 500 (rank 100). Ammo 5–50.
Vehicles (buildtime): light/medium ltvs 0–200 (5–10 s), APC 350–450, tanks 400–750 (30–60 s),
helicopter 400 / VTOL 600, boats 0–200, hovercraft 100, AAA 200; `ussupplytruck` 300 is a mobile
spawn (`spawngroup=true`, buyzone radius 6 / service radius 16) (`:82`). Buy zones grant
`BUY_AMMO/BUY_WEAPON/BUY_EQUIPMENT` flags (`:82`).

## Spawn / revive / vehicles

- Team default spawn group assigned on team change (`PowerStruggle.lua:1051`) and on revive via
  `RequestSpawnGroup`; invalid groups notify the client (`ClSpawnGroupInvalid`, `:1199`).
- Revive is queued (`QueueRevive`, `:693`) and flushed per revive cycle (`UpdateReviveQueue`, `:835-904`),
  with **purchase-on-revive** (`CommitRevivePurchases`, `:806-831`); death > 2.5 s → requestable
  (`:680-689`). Auto-balance switches dead players (`:340-350`).
- Vehicles: bought → `unclaimedVehicle` with 90 s claim window (`:24,353-364`), refund ×0.75 on expiry;
  team-only entry, owner-or-empty rule (`CanEnterVehicle`, `:571-584`); TAC tank / MOAC vehicles count
  as "produce TAC" for objectives (`Objective.lua:87-97`).

## Win conditions

1. **HQ destroyed** → `OnGameEnd(teamId, 1)` (`PowerStruggleAlert.lua:436-444`).
2. **Time limit** → total HQ health per team decides, draw otherwise (`PowerStruggle.lua:294-337`).
3. **Frag/team-score limit** → `OnGameEnd(teamId, 3)` (inherited `TeamInstantAction`).

## What the bot commander must model

Capturable `Factory`/`Objective` nodes with a team; capture cost (2 players, 15 s) vs uncapture
(faster); per-node `powerValue` (1/2/3) driving team power; rank→PP floor; PP spend on weapons/vehicles;
vehicle claim/build timers; HQ health as the timer/frag tiebreak; "produce TAC" and "destroy HQ" as the
two decisive objectives. Most commander decisions must respect fog of war (positions come from the team's
perception, not raw entities).

## Shore / Plantation capturables

Not enumerable from these paks: `Factory`/`Objective`/`HQ` placements are per-level (binary `.lyr`/`.cry`
+ terrain), not scripts. To list them: dump `System.GetEntitiesByClass("Factory"|"Objective"|"HQ"|
"AutoTurret")` per level (e.g. a `wb_ps_dump` console command) or parse the level entity tables. I did not
invent a list.
