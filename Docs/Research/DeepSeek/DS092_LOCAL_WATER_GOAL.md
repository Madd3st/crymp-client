# DS-092: local water roam goal - samples **and** committed goal

Base `def8a65d99e4640c9c765375f5739d010835e67e`. Patch `patches/ds092_local_water_goal.patch`
(sha256 `a0843fea...3db35`, 4 files, +41/-2, `git apply --check` OK on def8, `git diff --check` clean). Offline only.

## Why the DS090b one-liner was insufficient

`Steer` -> `NavMeshCore::FindPathLinks` projects the **goal** with the dry box `[goal.z-2.6, goal.z+1.0]`
(`BotNavMeshCore.cpp:264`). A local roam point built at the swimmer's feet z (C147: 97.2..98.6) is 1.4-2.3 m below
the baked water poly (99.511), so it fails `goal_off_mesh` even when the four samples are lifted. The committed
`m_patrolGoal` at `BotBrain.cpp:970` therefore had to be lifted too, at the same qualified z as the samples.

## Fix

* `BotRoamFallback.h` (engine-free): `RoamGoalZ(bool waterStart, feetZ, waterZ) { return waterStart ? waterZ : feetZ; }`
  - the gate is the existing `waterStart` fact from `WaterStartBounds` (`waterMovement && waterValid && lift > 0.05`).
* `BotBrain.cpp` local fallback (`:959-975`): `goalZ = RoamGoalZ(...)`; every sample `p.z = goalZ`; the committed
  positive-fraction goal `goal.z = goalZ`; `fraction == 0` still holds at `feet`; no mesh / dry keeps `feet.z`
  byte-identically (the local leg is horizontal, so `goal.z == feet.z` when `waterStart` is false).
* No actor position change, no teleport, no new flag/subsystem, no global `Nearest`/`Project` widening; spawn/hotspot
  land goals untouched; heard/sound goals untouched and noted as a separate unknown.

## Tests (real core, not a height constant)

`Tests/BotNavMeshWaterTests.cpp` (synthetic Detour, same fixture): the same XY local goal
* at the swimmer's feet z -> `FindPathLinks` and `Reachable` must **fail** (RED, the DS090b bug);
* at the native water plane -> both must **pass** (GREEN);
* off-mesh local goal -> still rejected (fail closed);
* dry land / water-only / hole / ray controls unchanged.

`Tests/BotRoamFallbackTests.cpp`: `RoamGoalZ` dry (feet z), native water (plane), and the C147 deep-float value.

## Status / gates

C148 (`def8a65d`) remains **physical evidence only**: the review gate was OK `80144e60` at 17:18:18, my BLOCK
`09e245e1` at 17:19:29, and the run at 17:24:29 - i.e. the run was not approved by the latest review. The DS091 audit
still stands for the trajectory itself (upper exit -> sea -> dry shore), with its limits. Any C149 needs a new exact
SHA with green CI and a latest-review fetch before the one shared whole-route run; this patch changes no gameplay
default and does not touch the successful upper jump, the capture-654 route or the held speed patch.
