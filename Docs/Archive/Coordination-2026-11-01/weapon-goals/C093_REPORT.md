# C093 / C097 visible-combat validation

Source7179ec36; controlC0885b90bfb6, independent native RNG (not matched seed).

| Skill | K/D | Hits | Headshot share | Hit dist median/p90 m | Hits >40m | Shots | Median body-yaw/brain-aim error deg |
| --- | --- | --- | --- | --- | --- | --- | --- |
| beginner | 24/81 | 351 | 0.037 | 10.58/15.07 | 0 | 1404 | 3.452 |
| normal | 83/79 | 882 | 0.040 | 10.72/16.64 | 1 | 2876 | 2.246 |
| veteran | 99/67 | 1054 | 0.056 | 13.51/21.57 | 6 | 3354 | 1.335 |
| elite | 70/49 | 796 | 0.078 | 11.16/17.89 | 6 | 2401 | 1.597 |

Totals: kills 278 -> 276, stalls 18 -> 19, traps 0. All requested gates PASS.
Target switches old!=0: 459; from a target damaged previously in run: 398 (control 641).

hits/headshots/distances from before-Lua FIRE HIT, not duplicate after-Lua
aim median is recorded actor body yaw vs BRAIN aim point proxy, not muzzle 3D aim error
damaged-target switch means target damaged earlier in run; no unlogged memory-window inference

Evidence: build/c097-on15-7179ec36/ and build/c088-owner-defaults-5b90bfb6/. No client naturalness or causal skill superiority beyond this one run inferred.
