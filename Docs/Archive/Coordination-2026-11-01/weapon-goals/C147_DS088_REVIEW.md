# C147 / DS088 offline integration review: BLOCK

Reviewed DeepSeek `6a352c5624f24837a3a5eea64a77454bf12e764d`, patch
`Docs/Research/DeepSeek/patches/ds088_water_query.patch`, SHA256
`f00f184365abc4dd87ce6976338cd2246c030000cc73a33ef21b8625c203267c`.
Applied cleanly only to a disposable archive of exact b06681ea in `build/ds088-check`.
Production branch and staged B1 were not changed. No game, CI, installation, or memory/nav changes.

## Blocking findings

1. **Supplied regression test fails.** MSVC x64 C++17/O2 compiles the patched core,
   Detour and `Tests/BotNavMeshWaterTests.cpp`. Execution exits **5** after:
   `water detour: corners=3 deviation=5.00` / `DS088 raw dry projection unexpectedly found`.
   The synthetic raw-dry RED assumption at line 54 is false on the supplied fixture.
   Correct the real fixture/oracle and explain the Detour projection behavior; do not simply
   delete the assertion or weaken coverage. Remaining assertions were not executed.

2. **RayClear safety explanation is incomplete.** `BotBrain.cpp:262-265` uses
   `RayClear(rawFeet,next)` to gate waypoint advancement when `CornerReached` reports
   a plane crossing outside the 0.35 m reach radius. A false ray breaks advancement and
   keeps the current corner target. This is not merely an optional corner fast-skip.
   The patched real-core oracle gives **0/1085** successful raw-feet rays to the first
   returned corner on valid paths. This does not itself prove a live stall, but requires
   a meaningful water corner-plane control and safe existing-policy handling before acceptance.

## Independent actual-core result

Compiled the patched core plus Detour with all 1136 original C147 swim rows, E223
`Shore.US-strength-candidate.navmesh`, single-Strength links, mask14, goal654
`1794.54,1690.64,101.53`. [Oracle source](C147_DS088_ORACLE.cpp).

| Measurement | Count |
| --- | ---: |
| Default raw-feet Project | 0/1136 |
| Patched bounded water Project | 1085/1136 |
| Patched water Reachable654 | 1085/1136 |
| Patched FindPathLinks654 | 1085/1136 |
| Path failure start_off_mesh | 51/1136 |
| Raw-feet RayClear to first returned corner, successful paths | 0/1085 |

This improves navigation offline, but does **not** match the previous surface-query
counterfactual (1095/1136, 41 misses). The additional ten misses need classification
using actual native feet/bounded-box behavior, without globally widening nearest.
First entry path has 14 corners and reaches goal654; it includes elevations 99.511,
101.711 and 101.811 before returning to water-height corners. Graph connectivity
does not establish native shore traversal or native water-to-dry handover.

Raw logs and reproducible compiler script:
`build/ds088-offline/{compile.ps1,check-run.log,check-compile.log,BotNavMeshWaterTests-run.log,BotNavMeshWaterTests-compile.log}`.
Run `powershell -NoProfile -ExecutionPolicy Bypass -File build/ds088-offline/compile.ps1`.
The script deliberately ends in failure on the supplied unit test. MSVC required execution
outside the restricted sandbox; outputs remain under the workspace. No local game binary built.

DS088 needs a revised patch plus water/dry/stacked/shore controls before integration.
Existing green/reviewed game source remains b06681ea; no unchanged runtime repeat.
