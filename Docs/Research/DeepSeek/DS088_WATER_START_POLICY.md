# DS-088 - revised water-start query policy (supersedes DS087 cfd7b9b9)

Patch: `patches/ds088_water_query.patch` (based `b06681ea`, applies clean; no game/build). Source code unchanged
except the query policy. B1 staged, upper Strength PASS unchanged, speed patch held.

## 1. ActualCore evidence (X238 a6bce068 / f5debb09)

All 1136 C147 swim rows, actual b066 core + E223/mask14:
* raw native feet -> Project **0/1136**, Reachable654 **0/1136**;
* same XY with native waterZ -> Project **1095/1136**, Reachable654 **1095/1136**;
* the 41 surface misses are isolated `(2079.187,1756.874,98.0)`, `(2109.09..2109.50,1710.45..1709.62)`, and the
  `x2134.2 / y1596..1597` shore loop. They are water-surface mesh/shore gaps, not a query-normalisation result.

## 2. Chosen policy (consistent, safe, native-qualified)

* The water start box is `1.5 / 2.0 / 0.2` (`WaterQueryBounds`, core header).
* **Water start is water-only**: if the bounded `Nearest` returns a ref, accept it only when `IsWaterPoly(ref)`
  (`kWaterArea = 4`). If no ref (surface hole) or a non-water ref (submerged seabed / dry / stacked floor), the
  query **fails closed** (`start_off_mesh` / `start_not_water`). No fallback to the dry box from the water query:
  the old asymmetric `if(a && startBounds && !IsWaterPoly(a)) dry` is removed.
* **Dry start** (native `waterMovement == false`, no bounds) keeps the existing `1.5/1.0/3.0` box; stacked
  submarine floors, IA and dry maps are untouched. The shore transition is the native water end, when the caller
  switches to the dry query; the water query never substitutes a dry/seabed floor.
* `Project` gains `waterOnly` for the path/roam start; `PSCaptureSnapshot` passes the same water box to
  `Reachable`, keeping the PS goal continuity only on real water polygons.
* No global up-widening, no actor position change/teleport, no forced link, no boat, no new flag.

## 3. Tests (real synthetic Detour, not a constant mirror)

The dead `BotWaterQueryPolicy` helper/`QueryBoundsFor` boolean-mirror unit and its CI entry are removed. The
existing `Tests/BotNavMeshWaterTests.cpp` (already CI-built with Core+Detour) now covers on its synthetic mesh:
* raw swimmer feet 1.5 m below the water surface: dry `Project`/`Reachable` RED, bounded water `Project` +
  `Reachable` to the land GREEN (z ~100 water surface);
* water query on the land/wrong area: **fails** (no non-water fallback); the dry caller still reaches;
* no water ref (outside the mesh): **fails closed**, no land/dry substitution;
* the stacked/through-deck dry control is the existing dry default; the actualCore run supplies the real
  dry/stacked/shore cases.

## 4. RayClear / WallClearance assessment

`RayClear` is called in `SteerCorners` with the **raw feet** and the next path corner; its internal
`Nearest(a, 0.5/1.0/2.0)` cannot see the water surface from the swimmer's feet, so `RayClear` returns false and
the **corner-skip optimization** simply does not run. The path corners themselves are still followed; this is not
the shore blocker and no `RayClear` change is required. `WallClearance` is not called by the current `BotBrain`
(only declared/legacy callers), so it has no runtime shore impact. Leave both unchanged: no broad refactor.

## 5. Remaining unknown / next

The 41 water-surface misses are data/shore topology holes on the **observed old-fallback route** (the isolated
points and the x2134/y1596 shore loop); the planned shortest path to 654 may not visit the shore loop, so the next
actualCore query should also project the planned path corners, not only the observed rows. The patch fails closed
on the holes instead of admitting a submerged/stacked floor. Next: actualCore dry/stacked/shore controls (Codex,
offline), then a water-mask/shore data repair if the holes are persistent; no global `Nearest` widening, no
teleport/boat. After a new exact green+reviewed SHA, one shared upper->sea->shore acceptance; no unchanged repeat
or water-only game.
