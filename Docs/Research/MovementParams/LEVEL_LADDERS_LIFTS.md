# Ladders and lifts on Armada and Steelmill - from the level data (OC-024 item 2)

Author: OpenCode (read-only, offline tooling only). All numbers below are parsed from the installed game on this PC
(`E:\Electronic Arts\Crytek\Crysis Wars`), no game was started, no server touched.

## Method (reproducible)

Levels are **unpacked folders**, not paks:
`Game\Levels\Multiplayer\IA\<Map>\<Map>.cry` + `<Map>\level.pak` (terrain/textures only).

`<Map>.cry` is a **ZIP** (PK header) whose `level.editor_xml` is the editor scene: entities with their properties and
brush objects with their **prefab path**, position and rotation. Extracted read-only to `%TEMP%\wb_levels\<Map>.editor_xml`
and counted `<Object ...>` entries (Armada 9288 objects, Steelmill 1917).

Entity scripts live in `Game\GameData.pak` (`Scripts/Entities/...`, `Entities/*.ent`, `Prefabs/ladder_systems.xml`),
materials in `Game\Objects.pak` (`*.mtl`, `SurfaceType="mat_ladder"`).

Cross-check: my independent parse reproduces Claude's `evidence/recast/movers.json` exactly -
Armada `Ladder=2, Stairs=39, Elevator=3, ElevatorSwitch=9`; Steelmill `Ladder=4, Stairs=11`; Quarry `Stairs=28,
Ladder=4`; Outpost `Stairs=7, Ladder=3`. `Tools/RecastBake/level_movers.py` is correct.

Caveat: brush **geometry** is not in the editor XML (prefab reference + transform only), so the XML gives where a
ladder/lift is, not how tall it is. Extent must come from the CGF AABB (which is exactly what the engine uses,
`Player.cpp:7027-7031`) or from the collision export.

## Armada - 3 elevators, 9 switches, 2 ladders, 39 stair brushes

Elevators (layer `Elevators`, `EntityClass="Elevator"`):

| Entity | Position (x,y,z) | `nFloorCount` / `fFloorHeight` | Other | Model |
| --- | --- | --- | --- | --- |
| `Elevator_left` | 1463.75, 1363.125, 108.75 | 2 / **11 m** | `bAutomatic=0`, Slide z, `fSpeed=1.0`, `fStopTime=0.75` | `lift4.cgf` |
| `Elevator_right` | 1468.75, 1322.625, 108.75 | 2 / **11 m** | `bAutomatic=0`, Slide z, `fSpeed=1.0` | `lift3.cgf` |
| `Elevator_WES` | 1489.75, 1319, 117.75 | 2 / **4.75 m** | `bAutomatic=0`, `nInitialFloor=1`, `fSpeed=1.5` | `weapons_elevator.cgf` |

`bAutomatic=0` is the important bit: **the platform never moves by itself** - a player (or a bot) has to use a switch
first. There are exactly 9 `EntityClass="ElevatorSwitch"` entities (11 other objects on that layer are
`ElevatorSwitchLight*` lights, not switches):

| Switch | Position | Deck | Switch | Position | Deck |
| --- | --- | --- | --- | --- | --- |
| `ElevatorSwitch_left_down` | 1453.5, 1361.875, 110 | hangar | `ElevatorSwitch_left_up2` | 1450.125, 1366.5, 121 | flight deck |
| `ElevatorSwitch_left_down1` | 1470, 1361.875, 110 | hangar | `ElevatorSwitch_left_up1` | 1474.625, 1366.5, 121 | flight deck |
| `ElevatorSwitch_right_down` | 1458.625, 1323.875, 110 | hangar | `ElevatorSwitch_right_up` | 1477.75, 1316.875, 121 | flight deck |
| `ElevatorSwitch_right_down1` | 1475.125, 1323.875, 110 | hangar | `ElevatorSwitch_WES_lower` | 1489, 1319.25, 114 | hangar side |
| | | | `ElevatorSwitch_WES_upper` | 1489, 1319.25, 118.75 | island deck |

i.e. each lift has a call button on **both** floors - a bot has to find the one on its own deck.

Ladders (2 brush objects with a ladder prefab):

| Brush | Prefab | Position | Deck |
| --- | --- | --- | --- |
| `Object4` | `objects/library/props/ladders/ladder_h.cgf` | 1413.375, 1339.75, **110** | hangar deck |
| `ladder_d3` | `objects/library/props/ladders/ladder_armada_a.cgf` | 1492.75, 1315.375, **121** | island / upper deck |

Stairs: 39 brushes (`stairs_300z*.cgf`, `stairs_collision_extension_12.5z.cgf`, ...), catwalks/railings 17.

Matching the v2 walk/jump evidence (`evidence/recast/Armada.json`): the 6 spawns that stay outside "main" sit on two
intermediate decks - **z 113.5-113.9** (`294`, `465`, `152`) and **z 117.9** (`417`, `441`, `373`); the latter three are
on the WES island lift's lower floor (117.75). So the decks are 110 (hangar) -> ~114 -> ~118 (WES) -> 121 (flight deck),
and every transition is a **lift, a ladder or a stair**, not a door. That is why removing "door-like" parts changed
nothing and why `r 0.3` fixed 37/40: the geometry is there, the *motion* is missing.

## Steelmill - 0 elevators, 0 ladder entities, 4 ladder brushes (water tower / chimney only)

| Brush | Layer | Prefab | Position |
| --- | --- | --- | --- |
| `water_tower2` | Main | `.../watchtowers/water_tower_ladder.cgf` | 1222.36, 1334.71, 135.04 |
| `water_tower_ladder_brokenb2` | Main | `.../watchtowers/water_tower_ladder_brokena.cgf` | 1288.08, 1336.87, 168.52 |
| `water_tower_ladder_brokenb4` | Main | `.../IndBuild_1/chimney_ladder_lower.cgf` | 1286.51, 1341.34, 148.41 |
| `grid59` | **Factory** | `.../IndBuild_1/chimney_ladder_upper.cgf` | 1290.375, 1341, 174.125 |

Stairs: 11 brush prefabs, all outdoor concrete (`concrete_structure_stair_a/b/c.cgf`).

The four upper-floor spawns from the v1 report (`104` at z 150.1, `136` at z 147.3, `268` at z 152.0, `297` at z 146.3)
lie at x 1306-1368, i.e. **20-80 m east** of that ladder cluster (x 1222-1290). So:

* Steelmill has **no lift** and **no ladder entity** at all;
* the only ladders are the water tower and the chimney - they cannot be the way into the factory upper floors;
* consistent with v2: those floors are reachable **on foot** (the v1 "ladder gap" was the `cs 0.2 / r 0.4` erosion).

But note: `IndBuild_1` - the factory building prefab - ships `ind_build.mtl` and `ind_build1.mtl`, and **both define a
sub-material `ladder_proxy` with `SurfaceType="mat_ladder"`**. So the factory prefab does contain climbable ladder
polygons somewhere; the collision export cannot point at them because it carries no per-part surface (C-041).

## Which of them are actually climbable (material level)

`mat_ladder` is the surface type the engine ray-tests: `Player.cpp:6974-6984`
(`GetSurfaceTypeByName("mat_ladder")`, matching `pRay->surface_idx`), ray length **2.0 m** (`:6965`), the part must be
vertical (`ladderUp.z >= 0.85`, `:7022`), and the climb length is the **static object AABB height**
(`ladderTop = center + up*(box.max.z - box.min.z)`, `:7027-7031`; `ladderBottom = center`). No length limit.

Materials in `Objects.pak` that declare `SurfaceType="mat_ladder"`:
`Props/Ladders/ladder_a.mtl`, `ladder_harbor.mtl`, `mp_ladder1.mtl`, `Architecture/Multiplayer/IndBuild_1/ind_build.mtl`
+ `ind_build1.mtl` (`ladder_proxy`), `Watchtowers/water_tower.mtl`, `watchtower_asian.mtl`, `watchtower_asian_glow.mtl`,
`Architecture/Submarine/props/ladders/sub_ladders.mtl`.

Material references inside the prefabs (ASCII chunks of the CGFs):
`ladder_h.cgf` -> `ladder_a`, `ladder_armada_A.cgf` -> `ladder_a`, `chimney_ladder_lower/upper.cgf` ->
`ladder_lower`/`ladder_upper` + `ladder_proxy`, `water_tower_ladder_brokenA.cgf` -> `ladder_brokenA`, and
`water_tower_ladder.cgf` uses the water tower material set. So: **Armada's two ladders and Steelmill's water tower /
chimney ladders are climbable geometry** (mat_ladder), except the two brushes explicitly named `*_broken*`
(`water_tower_ladder_brokena.cgf`), which are decoration.

## Two ladder mechanisms exist in this game, and neither is in the navmesh

1. **Static geometry with `mat_ladder`** (this is what Armada and Steelmill use): `CPlayer::IsLadderUsable`
   (`Player.cpp:6961-7040`); on use `m_stats.isOnLadder` is set, `Player.cpp:3247-3250` sets `upVector` from the ladder
   direction, `:3773-3776` zeroes gravity (`bSwimming` + `gravity.zero()`), `:4841` leaves at the end of the ladder.
   Bots currently **stop** on a ladder (blackboard `canMove` is false when `isOnLadder`).
2. **`Ladder` entity**: `Entities/Ladder.ent` -> `Scripts/Entities/Ladders/Ladder.lua`; takes `bottom_pos/top_pos`
   from helper entities `<name>_bottom` / `<name>_top` if present, else from its own bbox
   (`bbmin.z + 1` .. `bbmax.z - 0.25`, `Ladder.lua:104-123`), physicalizes as `PE_RIGID` (`:77-92`), used through the
   actor's use action (`BasicActor.lua:2513-2514`, event `detachLadder`). Legacy variant:
   `Scripts/Entities/Elevators/ladder.lua` (`soclasses_SmartObjectClass="Ladder"`). **Neither map places a Ladder
   entity**, so this path is irrelevant here but relevant if other maps (Quarry/Outpost) need it.
3. **Elevator**: `Scripts/Entities/Elevators/Elevator.lua` + `ElevatorSwitch.lua` - the switch starts the platform;
   with `bAutomatic=0` there is no self-service. `Prefabs/ladder_systems.xml` is just a prefab library (silos etc.),
   not a ladder placement list.

AI precedent: `Scripts/AI/Behaviors/Personalities/SHARED/Other/ClimbLadder.lua` exists (2.7 KB, mostly empty
behaviour stubs) - the concept is there, but nothing reusable as a path planner.

## Consequences for us

* **Armada is solved by data, not by a new export**: two 11 m lifts (hangar <-> flight deck, switch-driven) + one
  4.75 m lift (WES island) + `ladder_h` at the hangar. So C-041 stays as "per-part surface" only if we want the
  physics-exact ladder parts; for Armada the *executor* is the missing piece: use switch -> wait -> ride -> exit
  (C-036 family), plus a ladder climb (ray 2 m, vertical part, climb to AABB top). A first bot version can use
  `ladder_h` and skip the lifts.
* **Steelmill needs no ladder at all** for the factory upper floors: with `cs 0.05 / r 0.35` they connect on foot;
  the water tower and chimney ladders are optional vertical play, not connectivity.
* For Detour: ladders/lifts must be **off-mesh links with a custom kind**, because neither is expressible as walkable
  polygons - `mover_links.py` (v3) already does this for lifts/ladders from `movers.json`; the remaining work is
  marking a link as `kind=lift|climb` and letting the executor play it.
* If C-041 (per-part sidecar) ever happens, the exact ladder extents come for free: the engine itself uses
  `worldTM.GetColumn2()` + the static object AABB (`Player.cpp:7018-7031`), i.e. position + orientation + AABB height -
  which is exactly what `movers.json` already stores (position + rotation + prefab) minus the AABB.
