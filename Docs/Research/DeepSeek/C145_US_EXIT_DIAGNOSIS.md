# C145 - US submarine exit: offline joint diagnosis (DS-082)

Code: `b06681eaa4365afe5bf7309994012dce13a7ac83` (claude-sprint, CI 40649771503 units PASS at review time).
Run: X-229 / weapon-goals `1fb6eb79`, receipt `Docs/Coordination/C145_NATIVE_US_OBSERVATION.json`,
raw `build/c145-US-b06681ea-v4/` (read-only here). Bake data: `build/coordinator-c145/`
(`Shore.eed85fb1.candidate.navmesh`, 9 762 060 B - the file the run loaded).
This note answers DS-082: facts vs inference, exact turnaround, legitimate exit geometry, zmin vs traversal vs
goal, one missing observation, and the repair batch. No game/build/install and no triage writes.

## 1. What the 180 s run actually did (facts)

* 715 `MOVE TRACE` rows, t = 0.248..178.778 s, one actor 65481 (US group 537, spawn 535, upper cockpit).
* Bounds x 2105.033..2121.430, y 1882.627..1886.217, z 99.705..103.375; every 10 s bin repeats the same
  x/z excursion; mean speed 3.6-5.2 m/s, max 8-9 m/s; `swim=0` in 715/715; native water sentinel (`-1e6`) in
  479 rows; health 100 throughout.
* It is a **regular patrol**, not a failed exit: upper cockpit (x~2106, z~103.3) -> stairs -> ramp bottom
  (x~2121.4, z~99.7) -> back, ~10-20 s period, for 170 s.
* The end (t 173.44..178.59) is different: `NAV stuck report node=27378 pos=(2116.41,1883.09,102.02)` x4 and
  `NAV fallback ... reason=no_route|goal_unprojected from=27378 to=27326/27386/27307 expanded=30` x4. The bot is
  stopped mid-ramp and cannot route to goals 4-9 m away.

## 2. Why the loop happens (facts, code)

`BotBrain::UpdateRoam` (claude-sprint `Code/CryGame/Bots/BotBrain.cpp`):

* `:894-898` every other leg tries a hotspot only if `mesh->Reachable(from, hotspot)`.
* `:902-912` otherwise it walks **map spawn points**, also filtered by `reachable(...)`; a spawn <8 m away is
  skipped.
* `:915-917` the last resort is a golden-ratio point 6-10 m from the feet and it is **not** reachability-filtered.

In the installed bake the US spawn component contains exactly the 11 upper/ramp spawns
(`Shore.spawn-classification.json`: 11 on-mesh, all comp 3795, main=0; the 12 lower-deck spawns are offmesh).
`Shore.US.pts`/the level XML show the 11: 507/510/513/519/526/532/535/552/563 at z103.25 and 522/544 at
z99.875..100.0. Those are precisely the two ends of the observed loop (x 2105-2112 and x 2121.9-2122.1).
So the 170 s "dry loop" is the roam spawn-point cycle over the only reachable spawn points - not an attempt to
leave the boat.

The last 5 s are the unfiltered local branch (`:915-917`): goals such as `(2110.10,1890.85)` and
`(2116.06,1877.10)` land outside the corridor; `Steer` logs `goal_unprojected`/`no_route`, and with no path it
returns the raw goal (`BotBrain.cpp:205`), i.e. pushes into the hull until the 1.5 s blocked check.

PS capture does not rescue it: five capture destinations are resolved at t0.25 (id 64/316/344/579/654) but
**no target is selected** - the destinations are off-sub and the off-sub map is not reachable from comp 3795, so
the capture planner fails closed and the arbiter stays `roam` (`:853`,
`#2 ARBITER goal=roam reason=start score=0.10`).

## 3. Navmesh facts (offline queries, read-only)

`area-query.exe` against the exact loaded bake `Shore.eed85fb1.candidate.navmesh`:

| Point(s) | Result |
| --- | --- |
| Every one of the 715 trace positions | on mesh, `fromUS=1`, `fromNK=0` (also the project's `trace-mesh.tsv`) |
| Corridor floor, x 2122..2155 at y1884, z100 | on mesh, projected z **99.811**, `fromUS=1` |
| Corridor end x2160..2180 | **off mesh** (no poly) |
| Top deck, x2121..2155 at z102.7 | on mesh (z102.76), **`fromUS=0`**, `fromNK=0` - separate part |
| Water 4-8 m around the hull | **off mesh** |
| Water 12-20 m out (e.g. 2100,1920 / 2140,1863) | on mesh, `fromUS=0`, `fromNK=0` |
| Lower deck (2115,1883,90.4), (2118.5,1883.9,90.25), (2130,1882,92.5) | **off mesh** (zmin 99.5 clipped) |
| Open map / base (2050,1900 projected 99.51) | on mesh, `fromUS=0` |

Header: `NAVMESH loaded Multiplayer/PS/Shore: 46614 polys, 3803 parts, **0 links**`. Bake receipt:
`main_area 2 113 751 m2`, `components 3803`, `offmesh_links 0`, `blocked_cells 148 679`.
The whole US spawn compartment (cockpit + stairs + corridor) is one closed part; the top deck, the sea and the
base are other parts.

## 4. Geometry facts (exact US triangle subset, 369 728 tris)

`Shore.US.exact-subset.obj` (bbox 2050..2270/1825..1945) - floor scans (0.25-1 m) and ray casts:

* **Cockpit (spawn)**: floor z **103.13**, x 2104.5..2113.5, y 1882..1886.5; open above (no roof); walls/roof at
  z~105.9 on all four sides; the only opening is the stairwell on the east side.
* **Stairs**: z 103.37 at x2114.5 down to 100.23 at x2119.5 (y~1883), joining the corridor.
* **Corridor**: floor z **99.75**, x 2115..2156, y ~1879..1889; headroom to the deck slab at **102.50**; east end
  is a static wall at x **2156.3**; no hatch/opening in the ceiling (up-scan from z100 is solid everywhere
  x2120.5..2156), no opening up through the deck.
* **Hatch/ladder shaft**: the corridor floor has a **~1 m x 0.5 m opening at x 2115.5..2116.5, y 1880.5..1881.0**.
  Down-ray from z100.6 falls to z~90.1 (10.5 m) exactly there; the same cell is open upward from z91.2. The lower
  deck floor is z **90.375** (12 spawn points 506..545).
* **Lower deck**: z90.375, x~2105..2131; the "Water Room" (`AmbientVolume Shape Water Room 37`,
  x2118.5,y1883.875,z90.25) has **no floor under it** in the export - down-rays escape 30 m; that is either a moon
  pool to the sea or missing export geometry (see inference).
* **Top deck**: z102.5, x2115..2156, outside; hull side slopes to the water (z102.5 at y1889 -> z99.5 near
  y1892.2, ~45 deg); no drop links.
* `zmin 99.5` (`recast_bake.cpp:85`) drops every triangle below the waterline; the water mask
  (`AddWaterSurface`, `:451-498`) also adds no water quad where above-water hull overlaps the cell.

## 5. Facts vs inference

**Facts**: the observed loop is the roam spawn cycle in a closed 11-spawn part; the local fallback is unfiltered;
the corridor is baked to x2155 and US-reachable; the top deck and all water/base points are not reachable from
US; the lower deck is off-mesh; the only vertical connection between corridor (99.75) and lower deck (90.375) is
the 1 m shaft at (2116,1880.7); the cockpit has no other opening; there are no `Door` entities in the
`Submarine_US` layer, and no ladder entity in the mission XML.

**Inference (not proved)**: the 9.4 m, ~1 m-footprint shaft is a **ladder** (engine ladder state exists:
`BotBlackboard.cpp:25` blocks movement while `isOnLadder`; `Player.cpp:3272+` has ladder handling); the lower deck
is the intended continuation; the water room opens to the sea (the 30 m ray escape may be an export gap). If the
shaft is a ladder, the map's US exit is a vertical traversal that **no existing bot mechanism can do**: the link
kinds are `drop/jump/strength/lift/ladder` (`BotNavMeshCore.h:20-23`) but only drop/jump/strength are enabled
(`BotNavMesh.cpp:111`), and `lift/ladder` lines are never used (`BotNavMeshCore.h:63-65`). A Strength jump cannot
cover 9.4 m (max rise 3.72 m, `BotJumpLink.h maxStrengthRise`). A drop link into a flooded shaft is only valid if
the shaft bottom is water (unproved).

**The zmin vs traversal vs goal answer**:
* zmin 99.5 **does** clip the lower deck (a real interior volume; dry vs flooded is exactly the missing fact).
* it is **not** the cause of the observed 170 s loop: the corridor floor (99.75) and the cockpit are above the
  clip and are baked/reachable; the loop is the reachable-spawn cycle.
* The exit is **missing physical traversal** (a 9.4 m vertical ladder / flooded shaft) **and** the goal layer never
  asks for it (capture fails closed, roam has no exit goal). It is not a water-actuator bug in this run: the bot
  never entered water (`swim=0`).

## 6. The one truly missing observation

**Native/level confirmation of the shaft and lower deck**, in this order (cheap first):
1. **Offline (Codex, no game)**: inspect the level/brush/ladder data for a ladder (or lift) at
   x 2115..2117, y 1880..1882, z 90..100, and for a water volume/opening at the lower deck "Water Room".
   This alone decides ladder vs flooded drop.
2. Only if the level data is inconclusive: **one bounded native fixture**, purpose = read the native state at the
   shaft/lower deck, **not** exit proof:
   * exact reviewed SHA + green CI, no defaults, no spawn/teleport workaround;
   * a native player (human) walks cockpit -> stairs -> corridor -> shaft/lower deck once, 60-90 s, demo + screenshots;
   * record: is the shaft a ladder? is the lower deck dry or flooded (`waterZ`/swim)? does the water room open to
     the sea, and at which coordinate?
   This is **not** to be auto-requested as a blind run; it changes the B2/B3 decision below.

## 7. Repair / completion batch (coherent, separately reviewable)

**B1 - goal guard (cause proved now; patch `patches/ds082_roam_guard.patch`)**.
The local roam fallback (`BotBrain.cpp:915-917`) may return a point outside the bot's nav part; with no path,
`Steer` pushes straight at it (`:205`). The patch snaps the local point back to the last reachable quarter-step
(same `mesh->Reachable` test used for spawns/hotspots) and otherwise holds the current leg. It does not change
spawn/hotspot policy, PS capture, movement or defaults; it only stops the 1.5 s wall push and the
`NAV fallback`/`stuck` pair.
* Red oracle: exact b06681ea US run - 4 `NAV stuck` + 4 `NAV fallback` in t173..179, final speed 0 for 5 s.
* Green oracle: same 180 s fixture - 0 `NAV fallback`/`NAV stuck` after t170; position must stay on mesh; no new
  fallback reason.
* Review: diff is one block in `UpdateRoam`, no new flag, no network/Lua.

**B2 - bake dry interior below the waterline (only after 6.1; do not ship as a blind zmin lowering)**.
If the level data confirms the lower deck is **dry**, extend the baker so triangles below `--water` are kept when
their XY cell is covered by above-water solid (the existing `blocked` test in `AddWaterSurface`) and are not open
water; this re-adds the lower deck + shaft bottom without making any seabed walkable. Default OFF option
(`--interior-dry`), never a global `--zmin 90`.
* Red: current bake - lower deck off-mesh (queries in 3).
* Green: US subset bake with the option - 12 lower-deck spawns on-mesh and connected to the corridor; open-sea
  probes still off-mesh/non-walkable; Steelmill/Quarry/Armada/Outpost bakes area-neutral (no new water/seabed).
* If the level data says the lower deck is **flooded**, B2 is wrong: the room must be water (area 4), and the
  question becomes a drop/water link, not a walkable floor.

**B3 - the vertical traversal (only after 6)**.
* If ladder: a ladder traversal actuator is a **new subsystem** and outside this batch; needs an owner decision.
  A `ladder` link row alone is dead: the mask only enables drop/jump/strength (`BotNavMesh.cpp:111`).
* If flooded shaft: a **drop link** at the proven opening (2116,1880.7,99.75 -> lower-deck water) with the existing
  drop executor, plus the water surface inside the room; oracle: offline route US -> shaft -> water, then the
  existing water policy - no new swim controller.
* Do not add hull links, teleports, spawn workarounds, or a global zmin change.

**B4 - PS capture fail-closed (park)**. The capture planner correctly refuses unreachable destinations; once the
component is connected (B2/B3) the five destinations become reachable and the arbiter can leave roam. No separate
goal patch is needed before that.

## 8. Non-goals confirmed

No unchanged upper-deck re-run, no soak, no fine-mask/lower-zmin/autolink repeat, no fabricated hull link, no
teleport/spawn workaround, no grants/immunity, no boats/swim controller, no defaults, no net/Lua change.

## 9. B1 offline regression (DS-082 follow-up, 2026-10-28)

Codex did not integrate the original one-block fallback guard: it asked for a red/green regression over the
blocked/full/quarter/none/no-mesh cases with real adapter semantics and CI registration. This is now
`patches/ds082b_roam_fallback.patch` (base `b06681ea`, applies clean, no build):

* `BotRoamFallback.h` is engine-free: `RoamFallbackFraction(meshAvailable, reachable[4])` returns 1.0/0.75/0.5/0.25 or
  0 (hold), where the samples are full, 3/4, 1/2, 1/4 of the local leg; `meshAvailable=false` keeps the old full leg.
* `BotBrain::UpdateRoam` samples the leg with the existing `reachable` lambda only when the mesh is present and moves
  to `feet + (local-feet)*fraction`; 0 holds the leg instead of steering into the hull.
* `Tests/BotRoamFallbackTests.cpp` (CI `BotRoamFallback`) covers full, blocked-at-full, half, quarter, none, first
  reachable wins, and no-mesh. The old code fails the blocked/quarter/none cases (it always returned the full leg).

Static asset check (preliminary, read-only): the verified Crysis 2007 `level.pak` `brush.lst` contains
`sub_ladder_longshaft`, `_top`, `_middle`, `_bottom`, `sub_ladder_forwardhatch_b`, `shaft_floor`,
`hull_ext_3_mp_bottomroom_shaft` and `stairwell_opening500x200y`; this proves the assets are placed in the level, not
that the 2115..2117/1880..1882/z90..100 shaft is traversable. Exact transform/proxy/material mapping is the next
offline step (B2), not a ladder subsystem.

No game, build, install, triage write, default flip or hull link.
