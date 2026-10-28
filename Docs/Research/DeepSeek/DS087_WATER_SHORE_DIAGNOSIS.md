# DS-087 - C147 upper PASS / water shore FAIL: joint diagnosis and minimal water-query patch

Source: exact `b06681eaa4365afe5bf7309994012dce13a7ac83`, CI 40649771503 GREEN, DS081 OK, DS086 harness OK
`45d78996`, E2235993 one-Strength data. Run: `build/c147-upper-shore-b06681ea-v1/` (1199 trace rows, 301.68 s).
No game/build by me; B1 staged, speed patch not needed and held.

## 1. Run facts

* Upper route PASS with the unmodified executor: `LINK start 0.93`, native jump 4.07, `LINK done 5.55` at
  `2114.53,1885.63,105.92`; no speed patch. Sea entry 14.027 at `2054.337,1880.289,97.566`, swim1/waterZ 99.45.
* Whole shore FAIL at 300 s: 1136/1199 swim samples, health 100, no dry land; last `2115.825,1615.310,98.101`
  swim1; loop near `2115..2134 / 1595..1620`; 142 NAV fallbacks = **127 start_unprojected + 14 no_route + 1 other**;
  the later goals are fallback roam/NK, not capture 654 (`1794.5,1690.6,101.5`).

## 2. Root cause (facts + one hypothesis)

The logged `start_unprojected` is the **legacy BotNav** fallback, not the mesh reason: `Steer` tries
`mesh->FindPathLinks` first, then `nav.FindPath`; the legacy trace overwrites the mesh reason, so the log hides
the mesh failure. The mesh failure is consistent with the water surface offset:

* `BotNavMeshCore::Nearest` is a box search: `up` above the actor, `down` below. `FindPathLinks`/`Reachable` use
  `up 1.0` for the start (`BotNavMeshCore.cpp`), but the native swimmer's feet are **z~98.0-98.1** while the baked
  water surface is **99.45-99.53** (1.4-1.5 m above; earlier 1.5-1.8). The start box `[feet-3, feet+1]` misses the
  water surface; it may instead select a submerged/wrong component or fail `start_off_mesh`.
* Consequences are coherent:
  * `PSCaptureSnapshot` -> `mesh->Reachable(from,goal)` false -> every capture point gets
    `hasDestination=false` (`BotPSCapture.cpp:215-220`) -> the PS goal 654 is lost and the arbiter falls back to
    roam/NK goals.
  * `UpdateRoam` -> `mesh->Project(from,snap)` fails -> `mesh=nullptr` -> the `reachable` lambda accepts all goals.
  * `Steer` -> `mesh->FindPathLinks` start fails -> legacy `BotNav` fails `start_unprojected` (127) ->
    `m_path` empty -> the bot steers straight/loops.
* The `actuator`/swim state is not the blocker: `BotBlackboard` admits water, `BotMovement`/`BotLocomotion` swim,
  health 100. The blocker is the **query start box** and the resulting PS goal continuity, not the native swimmer.

Hypothesis to prove with the real core: the exact mesh reason is `start_off_mesh` (or a submerged wrong part) on
the captured feet; the water-aware start box selects the baked water surface and the path to 654 exists.

## 3. Minimal patch (own branch, applies on b06681ea; no new flag)

`Docs/Research/DeepSeek/patches/ds087_water_query.patch` (7 files + test + CI):

1. `BotNavMeshCore.h` - `NavQueryBounds {side,up,down}`; `Reachable(..., goalDown, const NavQueryBounds* start)`
   and `FindPathLinks(..., const NavQueryBounds* start)`; the default `nullptr` keeps the dry `up 1/down 3`.
2. `BotNavMeshCore.cpp` - `IsWaterPoly(ref)` checks `dtPoly::getArea() == kWaterArea` (4). The water start box is
   accepted only on the baked water polygon; otherwise the core falls back to the dry box. The goal keeps the dry
   `goalDown`.
3. `BotWaterQueryPolicy.h` - engine-free bounds: dry `1.5/1.0/3.0`, water `1.5/**2.0**/**0.2**` (covers the
   1.5-1.8 m feet-to-surface offset, avoids the submerged floor); `QueryBoundsFor(waterMovement, waterSurfaceValid)`.
4. `BotBrain.cpp` - the water box is passed only when `m_blackboard.waterMovement`:
   `Steer` `FindPathLinks`; `UpdateRoam` `Project`/`Reachable` (and the heard block); `UpdatePSCapture`
   `PSCaptureSnapshot(..., &water)`.
5. `BotPSCapture.cpp/.h` - `PSCaptureSnapshot(..., const NavQueryBounds* startBounds = nullptr)`; the captures
   `Reachable` with the water start box, so the PS goal 654 stays a destination.
6. `Tests/BotWaterQueryPolicyTests.cpp` + CI `BotWaterQueryPolicy`: dry never widened, water bounded, native/invalid
   combinations, the `up 2.0/down 0.2` band.

Scope: the **query start box + PS goal continuity**. The shore transition is the existing `FindPathLinks`
water->land path (`IsWaterPoly` only gates the start), the `RayClear`/`WallClearance`/`SteerCorners` and the
existing water policy; no global `up` widening, no dry-floor change, no actor position change, no teleport, no
forced link, no boat, no new flag.

## 4. Real-core red/green oracle recipe (actual captured positions, no game)

Inputs: candidate `build/coordinator-c145/Shore.US-strength-candidate.navmesh` + `US-only-strength.links.txt`
(one Strength row) and the actual trace feet, e.g. `2054.337,1880.289,97.566`; `2115.825,1615.310,98.101`; the
127 `start_unprojected` rows from `trace.json`; goal 654 `1794.5,1690.6,101.5`.

| Query | Current b066 | Patch |
| --- | --- | --- |
| `Project(feet, 1.5/1.0/3.0)` | fails / wrong submerged part | - |
| `Project(feet, 1.5/2.0/0.2)` + `IsWaterPoly` | - | z ~99.45-99.53, water area 4 |
| `Reachable(feet, 654)` (dry start box) | false | - |
| `Reachable(feet, 654, 2.6, water)` | - | true (same part, 654) |
| `FindPathLinks(feet, 654)` | `start_off_mesh` (hidden by legacy `start_unprojected`) | - |
| `FindPathLinks(feet, 654, water)` | - | `ok`, path corners, last corners on land |

Codex can run the existing `NavMeshCore`/`BotNavMeshCoreTests` harness with these points (offline, the local
candidate; the core is engine-free). The actualCore counterfactual is the independent evidence; no game.

## 5. Remaining unknown shore topology

The patch restores the start/PS goal continuity; whether the water->land path, the `RayClear`/`WallClearance` at
the shoreline, the land target 654 and the `316`/`344` alternatives actually connect on the real bake is the next
offline query result (the oracle above) and then the one shared acceptance on a new exact green+reviewed SHA.
No unchanged repeat or separate water-only game.

## 6. Status

Upper route stays PASS/unmodified; speed patch `3ec91e44` remains held (not indicated); B1 `0b39bdb6` staged;
current memory/nav restored exactly. The patch is a proposal for Codex compile/offline oracle/review, not an
integration.
