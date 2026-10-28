# C147 real-core swimmer query oracle

Offline diagnostic only, no production code change or game run. MSVC C++17/O2 compiles actual b06681ea BotNavMeshCore.cpp + Detour sources, with E2235993 mesh and singleStrength links/mask14. Source [probe](C147_WATER_QUERY_ORACLE.cpp); raw compile script, samples and result in `C:/Users/Magic8/WarsBotsBuild/crymp-multibot/build/c147-water-oracle`. Samples are all1136native swim rows from C147 trace.json; target6541794.54,1690.64,101.53.

| Query input | Projected | Reachable target654 |
| --- | ---: | ---: |
| Native actorfeet (unchanged worldposition) | 0/1136 | 0/1136 |
| SameXY, nativewaterZ as queryZ only | 1095/1136 | 1095/1136 |

Example t14.027 feet97.566/nativewater99.45: rawproject0/reach0, waterqueryproject1/reach1/snapped99.511. Same result at63.790,115.803,165.789,215.788,265.799. Thus lost navigation from swimmerfeet is independently reproduced in the real core, not merely inferred from fallback logs.

**Not a complete fix:** query-only counterfactual does not validate water area, obstruction or shore traversal.41surfacequeryfailures remain: isolated77.537(2079.187,1756.874),99.534..99.792(2109.089..2109.502,1710.448..1709.622), then many at x2134.2/y1596..1597 during the shoreline loop. Full positions in result.log. Inspect data boundary/collision and runtime path handling; do not silently broaden queries or teleport actors. Dry/stacked-room guards and water-to-land transitions need separate offline controls before any patch. Reachable is graph connectivity, not full physical path proof.

DS087 receives this evidence for the coherent capture-goal/path-query/shore batch. B1staged preserved, speculative Strength speed patch held because real link succeeded. No unchanged game repeat.
