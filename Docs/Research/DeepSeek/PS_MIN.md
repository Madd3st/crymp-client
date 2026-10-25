# PS v1 minimum: the first five minutes

Date: 2026-10-26. Read-only spec; no code. Owner decision D-018/D-019: IA first, PS in parallel, **3–4 bots per team,
scalable**, PS v1 includes vehicle entry/exit and a `vehicles off` mode. This file defines what a bot must do, the
honest inputs it may use, and the acceptance tests. Interface/spawn/team details are in `T0_GAMEMODE.md`; vehicle
entry is in `VEHICLES.md`.

## 1. Rules the bot may assume (facts from the extracted PS scripts)

- Two teams, `tan` and `black` (`PowerStruggle.lua:44`); PS is a `TeamInstantAction` (`:17-18`).
- Team spawn groups are real: `TEAM_SPAWN_LOCATIONS=true`, `USE_SPAWN_GROUPS=true`,
  `NEUTRAL_SPAWN_LOCATIONS=false` (`PowerStruggle.lua:31-33`). Spawn group validity is checked by the rules.
- Capture: `captureRequirement` default 2, `captureTime` 15 s (`PowerStruggleUtils.lua:43-49`), but the level data
  overrides both per entity (e.g. Shore factories 20–30 s / req 1 in `PS_MAPS.md:37-38`). A player counts while
  `inside` the capture area, alive, not spectator, and of one team; a mixed team is contested and no progress
  (`PowerStruggleUtils.lua:173-230`).
- PP is per player (`PowerStruggle.lua:36`); SCAR costs 150 PP (`PowerStruggleBuying.lua:41`); buy is allowed only in
  a buy zone and after `OnEnterBuyZone` has populated `inBuyZone[playerId]` (`PowerStruggleBuying.lua:851-930,1138-1170`).
  `BuyItem` iterates `inBuyZone[playerId]` without a nil check (`:899-902`), so a server-side bot must enter/register
  the zone before calling it.
- Vehicles: claim window 90 s, owner/team rule (`PS_RULES.md:77-79`); `PowerStruggle:CanEnterVehicle` is the
  authority (`ZPatch1...PowerStruggle.lua:571-584`); PS factories build the classes listed in `VEHICLES.md`.
- Win condition: enemy HQ destroyed (`PowerStruggleAlert.lua:436-444`) or time/score tiebreak. The first five minutes
  should not try to destroy an HQ; capture/build the first TAC path is enough.

## 2. Honest inputs for PS goals

The bot may know:

- own entity id, team, class, health/energy, PP/CP, current weapon/ammo, position, velocity, stance;
- static PS map data: factory/power/spawn/HQ positions, capture radii, team owner, buy-zone flags, vehicle classes
  (`PS_MAPS.md`, `ps_shore.json`, `ps_plantation.json`);
- team power, own team score, captured-node state as exposed by GameRules;
- enemies only through LOS/perception/hearing and last-seen memory; no enemy PP, health through walls, or hidden
  positions;
- a vehicle's public state (team, owner, free seats, position) when within interaction range;
- `CanEnterVehicle` result, `GetLinkedVehicle`, seat/passenger state.

It must **not** read the enemy buy list, hidden PP, exact enemy HP through walls, or the command layer's intentions.

## 3. Goal set for the arbiter

Add four PS goals to the existing `Fight/Hunt/Weapon` set. The utility is deliberately simple; T-9 owns the final
numbers.

```cpp
struct PSGoal {
    enum Kind { Capture, Defend, Buy, Vehicle, Fight, Hunt, Weapon };
    Kind kind;
    EntityId target;        // capture node / HQ / vehicle / weapon
    float utility;          // higher wins
    float commitUntil;      // anti-flip window, like the current arbiter
};
```

| Goal | Utility (first cut) | Commit / abort |
| --- | --- | --- |
| `Buy` | `2.0` if no SCAR and PP ≥ 150 and in own buy zone; `1.2` if PP ≥ 150 and at base but not in zone | one purchase attempt; no repurchase on success; abort if no PP |
| `Capture` | `nodeValue * 1.0 / max(20, distance)`; neutral ×1.2, enemy-owned ×1.0, own ×0.2 | commit until captured/contested 3 s; abort if enemy superior in area |
| `Defend` | `1.5` if own factory/power/spawn within 30 m of a visible enemy or took damage in last 3 s | 8 s; return to capture after clear |
| `Vehicle` | `1.0` if free friendly/neutral vehicle within 15 m and target > 30 m; `0` if `wb_ps_vehicles=0` | keep until reached; exit 10–20 m before target |
| `Fight` | existing combat utility, only if enemy visible and within weapon range | existing hysteresis |
| `Hunt` | last-seen enemy within 60 m and no capture/buy/defend goal | 3 s memory |
| `Weapon` | existing weapon-goal utility, but PS must not chase a weapon through the enemy base | 8 s / 60 s cap |

Tie-break: `Buy` before `Capture` at spawn (so the bot has a rifle), then `Capture`, then `Defend`, then `Fight`.
`Defend` and `Fight` may temporarily override `Capture`; they must not cancel the committed capture node.

## 4. The five-minute script

1. **Spawn (0–3 s).** Use the team spawn group from T-0; do not spawn at an enemy/neutral group. Face the nearest own
   factory/buy zone. If the spawn is hot (enemy visible within 25 m), choose `Fight` or `Defend`, otherwise `Buy`.
2. **Buy SCAR (3–15 s).** If PP ≥ 150 and no SCAR:
   - move to the own buy zone (factory/bunker) if not inside;
   - ensure the zone is registered (`OnEnterBuyZone`/native trigger); if `inBuyZone[playerId]` is nil, do not call
     `BuyItem` — log and retry after entering;
   - call the PS buy path for `macs`/`SCAR` once (`PowerStruggleBuying.lua:41,851-930`);
   - verify `HasItem`/current item; on failure, do not loop the same purchase every tick.
   If PP < 150, skip `Buy` and go to `Capture`.
3. **First capture (15 s–3 min).** Choose the nearest neutral `AlienEnergyPoint`/`Factory`/`SpawnGroup` by
   value/distance. Walk to the capture area, stay inside (not at the edge), and hold until `GetTeamId` changes or the
   area becomes contested. If contested by a superior force, leave and pick the next node; do not stand in a crossfire.
4. **Defend (on event).** If the own HQ/factory/power is damaged or an enemy is visible within 30 m of it, return and
   fight from cover. Resume capture after 8 s of clear.
5. **Vehicle leg (optional, `wb_ps_vehicles=1`).** If a free friendly/neutral land vehicle is within 15 m and the
   chosen node is >30 m away, enter as driver (or passenger if the driver seat is taken), drive the road/waypoint,
   exit 10–20 m before the node, then capture on foot. If entry/exit fails twice, blacklist that vehicle for 30 s and
   walk.
6. **Death/revive.** Preserve the goal, team and capture target. On revive, re-buy only if the SCAR is gone and PP
   allows; do not buy a second primary if one is in the virtual inventory (`PowerStruggle.lua:806-831`).
7. **No vehicles.** With `wb_ps_vehicles=0`, no `Vehicle` goal, no buy vehicle, no enter; same capture/buy script on
   foot. This is the regression mode for T-0.

## 5. Capture details that matter

- `captureRequirement` on Shore/Plantation data is often 1 (`PS_MAPS.md:37-38,61-62`), not the script default 2; the
  bot must read the entity/area value, not assume 2.
- The captured node's `GetTeamId` is `g_gameRules.game:GetTeam(self.id)` (`PowerStruggleUtils.lua:163-166`); a
  successful capture changes the entity team. Use that as the completion signal, not a timer alone.
- `inside` is populated by native area enter/leave callbacks (`PowerStruggleUtils.lua:111-146`); the bot must
  physically enter. A server-side teleport into the area is acceptable only for a probe, not normal play.
- Mixed teams set `contested` and return without progress (`:188-194`); if the bot is alone against two enemies, leave.
- A captured spawn group changes team and becomes a spawn option; `SpawnGroup.lua` is the entity script
  (`PS_RULES.md:19,72-74`). Do not capture a group and then spawn at the old team's HQ.

## 6. Acceptance tests (PS v1)

| # | Test | Pass |
| --- | --- | --- |
| 1 | `ps_team_spawn` | 3/3 or 4/4 bots, every `GetTeam(id)` matches its slot; no bot within 15 m of an enemy HQ/spawn; no `IA registration refused` |
| 2 | `ps_buy_scar` | Bot with 150 PP enters own buy zone, `BuyItem("macs")` once, inventory has SCAR, PP reduced by 150; no Lua `inBuyZone` nil error; no repeat purchase |
| 3 | `ps_capture_neutral` | One bot enters a neutral energy/factory area; after the entity's `captureTime`, `GetTeamId` == bot team; `OnCapture` observed; no capture while contested |
| 4 | `ps_capture_contest` | One bot vs one enemy in the same area: no progress, both survive/leave; no stuck at the edge |
| 5 | `ps_vehicle_driver` | Free `US_ltv` within 15 m: bot enters seat 1, `IsDriver`, drives 50 m on a road waypoint, exits 10–20 m before the node; `GetLinkedVehicle()==null` after exit |
| 6 | `ps_vehicle_passenger` | Seat 2 entry works; bot is not driver; exit restores a valid position |
| 7 | `ps_vehicles_off` | `wb_ps_vehicles=0`: no vehicle goal/buy/enter in 60 s; same capture/buy results on foot |
| 8 | `ps_first_5min` | In 5 min: no crash; each bot has a primary (SCAR or spawn SOCOM if PP short); ≥1 capture attempt per team; ≥1 vehicle entry if enabled; no enemy spawn; no `TRAP RECOVERY` cluster >2 m |
| 9 | `ps_no_wallhack` | All enemy goals use LOS/last-seen only; no enemy PP/health through walls; capture target from static map/visible state |

Metrics to log per bot: team, PP, CP, primary, `Capture` target/time, captures, buys, vehicle entries/exits, distance
driven, stalls, deaths. The run summary must include effective `wb_ps_*` cvars and the T0/GAMEMODE build hash.

## 7. Out of scope for v1

Commander orders/squads, full economy optimisation, TAC/alien tech, HQ destruction, air/boat driving, vehicle combat,
and dynamic spawn-group capture strategy are later. PS v1 is: **team spawn → buy SCAR → capture one node → defend →
optional land-vehicle leg → repeat**, with a deterministic vehicles-off baseline.
