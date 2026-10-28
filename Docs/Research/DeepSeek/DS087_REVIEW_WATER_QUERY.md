# DS-087: independent review of `ds087_water_query.patch` + offline evidence + real-core oracle

Inputs: `C147_UPPER_SHORE_RESULT.md|json` (weapon-goals `90c7588f`), local
`build/c147-upper-shore-b06681ea-v1/{summary,trace}.json` + `.bots.log`, source `b06681ea` (review `dd68f2ce`),
and the patch already pushed on this branch (`Docs/Research/DeepSeek/patches/ds087_water_query.patch`, `cfd7b9b9`).
Offline only: no game, build or install. **My own draft `ds087_swim_query.patch` is withdrawn** (a z-tolerance
variant of the same start lift); the area-check design below is the one to integrate.

## 1. Verdict

**OK to compile/integrate** after the notes in section 4. It fixes the measured cause, keeps the dry/stacked-floor
behaviour, and touches no flag, default or actor position.

Verified on a pristine `b06681ea` tree: `git apply --check` passes for all 8 files; `kWaterArea` already exists
(`BotNavMeshCore.cpp:23`) so `IsWaterPoly` compiles; the new parameters are defaulted, so every existing caller
(`BotBrain.cpp:875/897/966/1038/1058`, `BotWeapons.cpp:135`, `BotManager.cpp:870`, tests) still builds unchanged;
the CI insertion keeps `BotDifficulty` and adds `BotWaterQueryPolicy`.

## 2. Mechanism (why this is the right layer)

A native swimmer floats with the head at the surface: C147 feet `97.57-98.15` vs `waterZ 99.45-99.53`
(1.4-2.0 m below). `NavMeshCore::Nearest` searches only `1.0 m` up (`BotNavMeshCore.h:45`,
`BotNavMeshCore.cpp:172-183`), and the start uses exactly that box (`FindPathLinks` `:322`, `Reachable` `:238`),
so from water the mesh query fails, `PSCaptureSnapshot` marks every point `hasDestination=false`
(`BotPSCapture.cpp:216-219`, planner needs it at `BotPSCapture.h:50`), capture 654 is dropped at ~14.8 s, and
`Steer` falls through to legacy `BotNav` (`start_unprojected`, `BotNav.cpp:283`) then to a straight line
(`BotBrain.cpp:213`) - the 285 s swim loop. The patch lifts only the **query start**, only for
`waterMovement`, and accepts the lifted box only on the **baked water polygon** (`IsWaterPoly`, area 4), falling
back to the dry box otherwise. It also fixes the same failure inside `UpdateRoam`'s projection and its
"reachable" filter, which was silently accepting every goal when the projection failed.

## 3. Offline evidence of the size of the gap (candidate E2235993, `area-query16384`)

`area-query`'s `Project` is `side 0.2 / up 2.0 / down 3.0` (`area-query.cpp`), i.e. more permissive than the runtime:

* sea entry feet `2054.337,1880.289,97.566` -> projects to `99.511` (water poly) **only because up=2.0**; the
  runtime `up=1.0` cannot reach a plane 1.885 m above the feet. Exactly the arithmetic of the bug.
* loop point `2115.825,1615.310,98.101` -> `on=0` even at up=2.0 (side 0.2; runtime side 1.5 may still resolve it -
  the oracle decides).
* capture goals are all on the mesh: `654 1794.54,1690.64 -> 101.531`, `64 -> 110.311`, `316 -> 124.556`.
* shore connectivity is patchy: transect `x=2120`, water `99.511` is `fromNK=0` for `y=1600..1632` and `fromNK=1`
  at `y>=1636`; a land snap exists at `y~1616` (`101.111`, `fromNK=1`). `fromNK=0` can also be the 256-poly path
  buffer (`BotNavMeshCore.cpp:329-344`), not `other_component` - the oracle must print the reason.

## 4. Review notes (non-blocking)

1. `QueryBoundsFor(waterMovement, waterSurfaceValid)` is only used by the test; the runtime calls
   `WaterQueryBounds()` directly, so the `waterSurfaceValid` gate is dead. Either use
   `QueryBoundsFor(m_blackboard.waterMovement, m_blackboard.waterValid)` at the three call sites, or drop the
   function. Safety does not depend on it (`IsWaterPoly` already rejects a non-water snap), but a test that asserts
   an unused function is not evidence.
2. The water box is `up 2.0 / down 0.2`: it covers the observed 1.4-2.0 m float offset and deeper swims fail closed
   (`start_off_mesh`, old behaviour). Keep that bound explicit in the header comment.
3. `Reachable` calls for vantage/cover/weapon scans (`BotBrain.cpp:966/1038/1058`, `BotWeapons.cpp:135`) stay on the
   dry box; harmless in water (no items/vantage spots there) but should be stated, not silently left.
4. QA logging: the logged `reason=` is the legacy `BotNav` reason (it overwrites the mesh trace at
   `BotBrain.cpp:201`); the mesh reason (`start_off_mesh`) is only visible before `BotNav` is ready. The next
   harness should log both reasons, otherwise a future run again looks like "start_unprojected" without the mesh
   cause.

## 5. Real-core oracle (Codex compiles; with the patch applied)

Load `build/coordinator-c145/Shore.US-strength-candidate.navmesh` + the one-row links (`LoadLinkKinds`, mask 14),
print `FindPathLinks`/`Reachable` **with the reason**:

| case | from (actual captured) | goal | unmodified | patched |
| --- | --- | --- | --- | --- |
| A | `2054.337,1880.289,97.566` (sea-entry feet), dry box | `1794.54,1690.64,101.53` (capture 654) | `start_off_mesh` (RED) | `ok` (GREEN) |
| B | `2115.825,1615.310,98.101` (loop feet) | same | `start_off_mesh` | `ok` or a named reason |
| C | `2054.337,1880.289,97.566` with the water box | `2120,1636,99.6` (water area) | - | `ok` |
| D (negative) | deck `2114.82,1883.35,103.13` with the water box | any | - | must **not** snap to water (`IsWaterPoly` false) -> dry box |
| E (regression) | Steelmill mesh, dry queries from `BotNavMeshCoreTests` points | - | unchanged | unchanged |

If A/B come back `other_component`, the bake's water patch is disconnected from capture 654's shore: that is a
data item (shoreline connectivity, DS-062/DS-071 shoreline coverage), not more movement code. `search_exhausted`
means the 16k budget / 256-poly buffer, not topology. Then, and only then, a new exact SHA + independent review +
one shared upper->sea->shore run; no unchanged repeat.

## 6. Remaining unknowns

* Water-patch (component) connectivity to capture 654's shore - oracle A/B.
* The native swim->walk exit at the chosen shore point - runtime, after a green oracle.
* B1 `0b39bdb6` stays staged; `3ec91e44` is **not** indicated (the upper exit passed unmodified) and stays held.
