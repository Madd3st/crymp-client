# PS map semantics: Shore and Plantation (DS-004)

Read-only extraction from `E:\Steam\...\Crysis\Game\Levels\Multiplayer\PS\<Map>\level.pak`
(`mission_mission0.xml`, `leveldata.xml`) into `%TEMP%\ps-levels`; game files are not committed.
Full lists: `ps_shore.json`, `ps_plantation.json` (schema `ps-map/1`, world coords, metres, Z up).
Rules context: `Docs/Research/DeepSeek/PS_RULES.md`.

## Method / caveats

- Every entity in the mission XML was scanned; the JSON keeps bot-relevant classes:
  `HQ`, `Factory`, `AlienEnergyPoint`, `BuyZone`, `SpawnGroup`, `SpawnPoint`, `AutoTurret`,
  `Door`, placed vehicles (`Asian_ltv`, `US_hovercraft`, `US_smallboat`, `Civ_*`), and areas
  with `Area Id > 0` (`AreaShape`/`AreaBox`/`AreaSphere`).
- `teamName` is what the level stores; **blank = neutral/capturable at runtime**, not "no team".
- Capture zones are `AreaShape` shapes; `captureAreaId` is an *Area Id*, not an entity id. The JSON
  attaches the nearest area with the same id (`captureArea.center/radius`, radius = half-extent in XY).
  Treat it as an approximate circle, not a polygon.
- There is **no `Objective` entity** in either level; the 4 PS objectives are gamerules/HUD logic, not
  placed objects. There is **no ladder/elevator class** in these levels — only `Door` (dynamic entries)
  and terrain/stairs. `PressurizedObject`/`InteractiveEntity` are props, not lifts.
- "Alien crash site" is not a separate class; the alien/energy objects are `AlienEnergyPoint` (power
  points). `Prototype-Factory` is the alien/TAC production building.

## Shore (1314 entities; 261 bot-relevant)

**Bases / HQs** (`nHitPoints=5000` each)
| Team | HQ | Pos | Type |
| --- | --- | --- | --- |
| tan | `HQ_NK` | 1785.8, 1270.1, 111.0 | bunker |
| black | `HQ12` | 2181.6, 1884.1, 105.8 | submarine |

**Factories** (each has 6 vehicle slots; `buyOptions` ammo/weapons/equipment/vehicles all on)
| Name | Team | Pos | Capture | Builds |
| --- | --- | --- | --- | --- |
| `Small_Vehicle_Factory_NK` | tan | 1812.3, 1177.0, 111.1 | — | light4wd, us4wd, usgauss4wd, nktruck, ussupplytruck |
| `Naval_Factory_NK` | tan | 1758.5, 1343.2, 108.3 | — | usboat, nkboat, nkgaussboat, ushovercraft, nkapc |
| `Prototype-Factory6` | neutral | 2145.2, 1484.6, 110.0 | 30 s, req 1, area r≈14.2 | ussingtank, ustactank, usmoac4wd, usmoar4wd; prototypes=1 |
| `Vehicle_Factory_Neutral` | neutral | 2236.8, 1325.8, 124.3 | 30 s, req 1, area r≈18.3 | us4wd, usgauss4wd, nktruck, ussupplytruck, usapc, nktank, ustank, usgausstank, nkaaa, nkapc |

**Power points (alien energy)** — `powerIndex=1` ⇒ 0.1125/s each
`AlienEnergyPoint11` (1965.0, 1387.8, 105.0), (2075.4, 1285.4, 126.2), (1799.4, 1686.3, 102.8);
capturable, 15 s, req 1, neutral.

**Spawns** — 2 default groups + 4 neutral capturable (10 s, 23 points each)
`SpawnGroup2` tan (1839.8, 1238.6, 112.0, 20 pts) · `SpawnGroup_US5` black (2109.3, 1884.3, 104.4, 23 pts)
· neutral: (2150.5, 1292.7, 125.4), (1964.0, 1310.0, 119.4), (1969.0, 1535.9, 102.2), (2291.6, 1534.5, 102.6).

**Other**: 1 `BuyZone` black `BuyZone_US13` (2128.5, 1883.8, 100.5) ammo/weapons/equipment; 18
`AutoTurret` (layer `Submarine_US`/`Base_NK`/`Port` tell the base); 31 `Door` (building entries);
34 placed vehicles; 4 capture-zone shapes (≈26.5 m half-extent). 124 `SpawnPoint`.

## Plantation (917 entities; 130 bot-relevant)

**Bases / HQs** (5000 hp each)
| Team | HQ | Pos |
| --- | --- | --- |
| black | `HQ_US` | 817.9, 1307.4, 252.9 |
| tan | `HQ_NK` | 1261.9, 1332.5, 238.0 |

**Factory**
`Prototype-Factory-small1` neutral (1077.1, 1258.8, 246.3), capture 20 s req 1, area `Shape_PF` r≈14.0,
`prototypes=1`, no vehicle list (alien/TAC production). Only one factory on this level.

**Power points** — `AlienEnergyPoint14` (1025.5, 1308.2, 247.7) and (1112.1, 1378.3, 247.0);
neutral, 15 s, req 1, powerIndex 1.

**Spawns** — black default `SpawnGroup_US` (847.0, 1316.0, 125.3, 19 pts), tan default `SpawnGroup2`
(1232.5, 1330.2, 239.0, 20 pts), 2 neutral capturable 10 s (1056.9, 1169.6; 1040.1, 1401.3, 23 pts each).

**Other**: 2 black `BuyZone` (848.3, 1311.8; 853.3, 1312.6); 9 `AutoTurret` (5 NK, 4 US by layer);
9 `Door`; 8 placed vehicles; 2 capture-zone shapes (≈26.5 m). 79 `SpawnPoint`.

## What this means for the commander (T-6/T-10)

- The decisive PS loop is visible in the data: **hold your HQ → capture the neutral Prototype-Factory
  (PT) → earn team power from `AlienEnergyPoint`s → buy TAC/alien tech → destroy the enemy HQ**. Shore
  gives 3 power points and 2 capturable factories; Plantation only 2 power points and 1 factory, so the
  single PT is a much sharper choke point.
- Spawn groups are first-class and capturable (10 s); `USE_SPAWN_GROUPS=true` in PS, so `BotMatch.cpp:34-35`
  refusing spawn-group registration is a hard blocker, not a detail.
- Capture zones are ~26.5 m half-extent squares around factories/spawn groups; `captureTime` is 10 s for
  spawn groups, 15 s for power points, 20–30 s for factories, `captureRequirement=1` on this data
  (the 2-player default in `PowerStruggleUtils.lua:43-49` is overridden per entity; verify with a live
  `GetEntitiesByClass` dump before relying on it).
- Turrets/doors are the only base-interior geometry data here; no lifts. Vehicle "spawners" are factory
  slots (6 each) plus placed vehicles, so the road graph in T-6 must connect factory exits, not just
  walkable navmesh.
