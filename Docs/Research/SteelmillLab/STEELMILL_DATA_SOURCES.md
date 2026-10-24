# Steelmill: real data sources

Snapshot analysed on 2026-10-24 (copies, SHA256 in `build/steelmill-lab/SOURCES.sha256`):

| File (copy) | SHA256 (first 16) | Origin |
|---|---|---|
| `level.pak` | `c9daac8d6d5ed830` | `Game/Levels/Multiplayer/IA/Steelmill/` (installed game) |
| `Steelmill.xml` | `31382af5bc83c129` | same folder |
| `Steelmill.dds` | `66a18c6b45be9b9c` | same folder (2048×2048 minimap) |
| `Multiplayer_IA_Steelmill.mem` | `69d8d9a6ff960269` | `%USER%/WarsBots/` (Codex QA restores snapshots of it) |
| `WarsBots-Play11Elite-20261024-023225-Server.bots.log` | `d80df7b81147f74e` | 48 min human + 11 bots, build `5e8daa65` |
| `WarsBots-Play6-20261023-230958-Server.bots.log` | `ffc0d303af12add4` | 61 min human + 6 bots, build `5e8daa65` |
| 5 × `WarsBots-Tactics-*.bots.log`, `P0-Duel-01`, `Claude-wgfix15` | see manifest | automated QA, builds `eea973f0`, `0b70b24b`, `2fcb4c0f`, `8f0abd5f` (from the BotManager build line) |

Nothing was written to any original. Game-derived outputs stay in `build/` and are not in Git.

## 1. What each source can prove

| Question | Source | Status |
|---|---|---|
| Spawn points | mission XML in `level.pak` (36 `SpawnPoint`, EntityIds, positions) | **real**. Runtime ids are the mission ids: 29/29 and 35/35 spawn ids seen in logs exist in the mission |
| Weapon / ammo positions | mission XML, Layer `Weapons`: 40 weapons/explosives + 38 `CustomAmmoPickup` | **real for map-placed items**; dropped weapons get runtime ids (> 65000) without a position in any log |
| Map picture | `Steelmill.dds` + `Steelmill.xml` MiniMap bounds `x 833.7…1637.7, y 968.9…1772.9` | **real**; orientation from engine HUD code, plausible against routes, not verified in game |
| Other POI | 3 `SpectatorPoint`, 4 `AreaBox`, 2 `AreaSphere`, 30 `RopeEntity`, `ProximityTrigger` | real, positions only |
| CryAI navigation | `*.bai` in `level.pak`: net 96 B, hide 96 B, areas 36 B, roadnav 8 B, waypt 8 B | **absent** (header-only), same as all PS maps |
| Navigation graph (BotNav) | built at runtime from physics rays; **not saved anywhere** | only counts in logs (`NAV ready: 7633 nodes, 41618 edges`) |
| Map memory | `.mem` (WBMEM01): grid 96×63, cell 3 m, origin (1121.25, 1245.0), 7633 records | **real values, no positions** |
| Bot positions / state | `AI telemetry` every 2 s per verbose bot | **real** samples (2 s resolution) |
| Movement intent vs physics | `MOTION INTENT` (requested, actual, blocked) + `ABILITY PHYSICS` | real, per telemetry block |
| A* failure | `#N NAV fallback start goal` = `BotNav::FindPath` returned false | real event, **reason not logged** |
| A* success | not logged | **missing**: no failure *rate* can be computed |
| Selected goal | weapon id only when it times out/cancels; roam goal never; enemy id in telemetry | partial |
| Stuck reports | `chase abandoned`, `PROGRESS ESCAPE` | partial: the roam-blocked `ReportStuck` (BotBrain.cpp:418) is silent |
| Kills / deaths | `combat kill`, `death observed` | events real; position = last telemetry (≤ 2.5 s old), inferred |
| Revive | `revive requested … spawnId position` | real |
| Human players | hits/kills reference their ids; **no positions** | missing |
| Jumps | `JUMP TAKEOFF/LANDING` without actor id | not attributable (as Codex notes) |

Not used: `Steelmill.cry` (8 MB Sandbox editor file, not loaded by the game; format not
investigated) and `terraintexture.pak`.

## 2. Exportable without engine changes

* All mission entities (positions, classes, EntityIds) — `level_data.py`.
* Minimap image and bounds — `level_data.py`.
* Grid parameters and per-node counters of `.mem` — `wbmem.py`.
* Bot tracks/events from logs — `botlog.py`.
* Grid of the build is reconstructible: bounds = spawn bbox ± (40, 40, 15/35) m, which matches the
  `.mem` header exactly (min 1121.25/1245.0 = spawn min − 40).

## 3. Available only inside the running game

* Node positions/layers and edges (`BotNav::m_nodes/m_edges/m_columns`) → needs the export of
  [BOTNAV_EXPORT_CONTRACT.md](BOTNAV_EXPORT_CONTRACT.md).
* Physics geometry (`RayWorldIntersection` on static + terrain), water level, dynamic objects.
* Live positions of dropped items, humans, vehicles; current goals and paths of bots.
* FindPath success counts and failure reasons; Nearest() results.

## 4. CryMP native sources (no own mechanism needed)

| Need | Native source | Offline? |
|---|---|---|
| object positions | mission XML; runtime `IEntitySystem` / Lua `System.GetEntities*` | mission: yes |
| spawn points | mission `SpawnPoint`; runtime `CGameRules::GetSpawnLocation(i)` (BotNav uses it) | yes |
| item positions | mission `Layer="Weapons"`; runtime `IItemSystem` | placed items only |
| death events | `CGameRules::KillPlayer`/`OnKill` → GameplayRecorder `eGE_Kill` (listener possible) | logs only |
| navigation geometry | CryAI: **no data on Steelmill**; physics world: runtime only | no |
| points of interest | `SpectatorPoint`, `AreaBox/AreaSphere`, `ProximityTrigger` in mission | yes |
| map picture | `<Level>.xml` MiniMap + `.dds` (what the HUD uses) | yes |
