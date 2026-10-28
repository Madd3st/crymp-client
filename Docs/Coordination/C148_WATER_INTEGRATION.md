# C148 water navigation integration

Integrated exact claude-sprint **def8a65d99e4640c9c765375f5739d010835e67e**, parentb06681ea: DeepSeek DS089v3/ad6053e4 plus already offline-reviewed B1/0b39 reachable roam fallback. CI40703800474 full build and independent DS090 review pending; no game yet.

Native owned-PS swimming + valid native water plane qualifies bounded water-only start queries. Offset-based bounds reach the plane (cap2.8m), reject dry/seabed/stacked polygons; path start, capture Reachable, roam admission and corner-plane RayClear share the water handling. Dry callers retain previous extents. No new flags/default promotions, boat/ladder/dive controller, actor reposition or speculative Strength speed patch. Upper exit already proved on b066/E223 stays unchanged.

MSVC x64: six units PASS (NavMeshWater/Core, PSCapture, RoamFallback, Corners, WaterPolicy); actual Brain/Blackboard/PSCapture C++20/Zs PASS, no local game binary build. Actual core source in disposable oracle byte-identical to committed def8 core: all1136C147swimrows give rawProject0, boundedProject/Reach654/FindPath1095,41start_off_mesh. Water RayClear1083/1095 to firstreturnedcorner; 12blocked rays remain observed, not claimed allclear. Actual physical shore handover remains untested.

**Fixture correction beyond DS089:** measuring planeZ alone still failed RED. A zero-height Detour tile (bminY=bmaxY100) clamps out-of-range BV queries onto its plane. Corrected synthetic fixture has padded vertical bounds90..110, quantized vertexY100/ch.1 so worldplane remains100; production cs.1/vertexXZ*100 preserves40x30 physical tile. Every assertion retained: rawfeet RED, waterGREEN, deep-float recovery, wrong land rejection, dry land unchanged, hole rejection, water corner ray. Passed MSVC. This changes the test fixture, not production Detour tolerances.

C148 harness `build/c148-upper-to-shore.ps1` derives from DS086-reviewed C147 scenario: exactnewSHA+CI/review receipt, E223sameoneStrength+matchingmanifest, nativeUS2 only, doors/vehicles0, no force spawn/resample. Short logname and removal of one redundant pre-remove wb_list reduce expected full command2044->2011, failclosed above2020. One shared<=300s upper->sea->dryshore run after gates; currentmemory/nav fresh snapshot + names/hash/absence restoration, actualdemo/logs/summary archive. No unchanged b066 or separate water game.

[Receipt](C148_WATER_INTEGRATION.json); raw `build/ds089-offline` and `build/ds089-integration`.
