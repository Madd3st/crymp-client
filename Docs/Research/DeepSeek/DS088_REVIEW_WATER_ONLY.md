# DS-088: independent review of `ds088_water_query.patch` (water-only start, `9db1d39a`)

Base `b06681ea`. Offline only: no game, build or install. My earlier `ds088_water_query_v2.patch` (validated dry-exit
fallback) is **withdrawn**: the water-only policy below is the simpler answer to the reviewer's warning and needs no
extra blackboard state.

## 1. Verdict

**OK to compile/integrate.** Verified on a pristine `b06681ea` tree: `git apply --check` passes for all 6 files
(`BotBrain.cpp`, `BotNavMeshCore.cpp/.h`, `BotPSCapture.cpp`, `BotPSCaptureRuntime.h`, `BotNavMeshWaterTests.cpp`).
No new CVar, no default change, no actor move, no boat/teleport/forced link; the upper Strength exit and `3ec91e44`
are untouched.

What it does, exactly:
* `NavQueryBounds{1.5, 2.0, 0.2}` + `WaterQueryBounds()`; the water box is used **only** when
  `m_blackboard.waterMovement` is true (the existing gated `ShouldSwim + BotWaterAllowed` state).
* `Nearest` -> `IsWaterPoly` (area 4) decides; a missing ref (surface hole) or a non-water ref fails closed -
  `start_off_mesh` / `start_not_water` (`NavMeshCore::QueryStart` equivalent inline in `Reachable`/`FindPathLinks`).
  No dry/seabed/stacked fallback, so the asymmetric `!IsWaterPoly` case from cfd7b9b9 is gone in *both* directions.
* `Project(..., bool waterOnly=false)` keeps every existing caller unchanged (default), and the roam start uses
  `waterOnly=true`.
* Dry callers (`waterMovement == false`) keep `1.5/1.0/3.0`; the goal box is unchanged; `PSCaptureSnapshot` keeps the
  water start via a defaulted parameter, so `BotManager.cpp:870` still compiles.

## 2. Notes (non-blocking)

1. `NavQueryBounds` has no explicit `water` flag: the core treats **any** `startBounds` as water-only. Today only
   `WaterQueryBounds()` is ever passed, so this is fine - but a future dry custom box would silently become
   water-only. Either add `bool water` (my withdrawn v2) or state the invariant next to the struct.
2. `Reachable` checks `!a` twice (the early `if (!a || (startBounds && !IsWaterPoly(a)))` then `if (!a || !b)`);
   harmless, tidy when touching the file.
3. `Project(..., waterOnly=true)` still writes the dry snap into `out` when it returns false; callers use the bool
   (`UpdateRoam` does), so no behaviour issue.
4. Shore transition: at the last water polygon the water query fails `start_not_water` until `waterMovement` flips
   false on land - expected. A future trace should show a short `start_not_water` burst at the beach exit; that is
   the native hand-over, not a bug, and it is worth logging at level 2 once so acceptance can see it.

## 3. The 41 surface misses (characterised, independent probe)

Codex oracle (`build/c147-water-oracle/result.log`, 1136 native swim rows): raw feet project/reach 0/1136; at the
native water plane 1095/1136. The 41 misses are **not** a query-box artifact: `area-query16384` (`up 2.0`) also
returns `on=0` for all 41.

* **38 rows: shoreline void strip** `x 2134.10..2134.30, y 1596.18..1597.73`. Ring probe at `y 1596..1602`:
  `x=2130/2132` -> water `99.511`; `x=2134` -> **no polygon**; `x=2136/2138` -> land `101.1`. A ~2-4 m bake gap
  between water and land polygons - wider than `side 1.5`, so no query change should bridge it.
* **3 rows: two open-water voids** `(2079.187,1756.874)` and `(2109.089/2109.502,1709.622/1710.448)`; neighbours 4 m
  away are water `99.511`, the points themselves have no polygon (the 2622 unknown water cells class).
* All 41 lie east/north of the sea entry (`x >= 2079` vs goal `x 1794`): they belong to the **old fallback/roam**
  path along the east coast, not to the direct entry->654 swim, which the oracle already shows reachable
  (`surfaceReach654 = 1` on 1095 samples incl. the entry).

## 4. RayClear / WallClearance

* `WallClearance` has no callers (`BotNavMeshCore.cpp:201` only) - nothing to propagate.
* `RayClear` (`BotBrain.cpp:259`) receives the **raw swim feet** as `a`; its `Nearest(a, 0.5, 1.0, 2.0)` fails under
  water, so the corner fast-skip simply does not run and the corner is passed at the normal 0.35 m reach. That is a
  lost optimization, never a false "clear": leave it, as decided, and document it (the patch does).

## 5. Oracle recipe (with the patched core)

1. All 1136 rows: `FindPathLinks(feet, 654, bounds)` with `bounds = WaterQueryBounds()` -> expect >= 1095 `ok`,
   exactly the 41 named holes failing (`start_off_mesh`), and no `other_component` on a non-hole row.
2. Controls: dry sample with `bounds = nullptr` (unchanged); land sample with water bounds (`start_not_water`);
   off-mesh sample (fail closed); stacked/deck sample (dry default).
3. `Tests/BotNavMeshWater` (synthetic Detour, CI) covers raw-dry RED -> water GREEN, land/wrong-area rejection,
   no-ref fail-closed, dry land unchanged; `BotNavMeshCore` (Steelmill) proves no dry regression.
4. Only after green exact SHA + review: one shared upper->sea->shore run; no unchanged repeat.

## 6. Remaining

The `x~2134` void strip and the two open-water voids are **bake/shoreline-connectivity data items** if a route ever
needs them; the planned west route does not. B1 `0b39bdb6` stays staged; `3ec91e44` stays held (upper exit passed
unmodified). No game/build/install by me.
