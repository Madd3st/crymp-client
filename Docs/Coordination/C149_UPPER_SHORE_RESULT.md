# C149 final gate-approved upper submarine -> sea -> dry shore: PASS

Tested exact claude-sprint **b8b444d6c858f27a1fdde0d93b25a318cafc6937**, green CI40709459511; complete independent code reviewDS093b3182ff90373ad00731e75ea7d44df82a15c15d2 explicitly resolves the old DS090b blocker. Paired CI client/server hashes verified. Latest review fetched/read **after CI**, then the harness checked the pinned review head before staging, after actorless preflight/before bot launch and every15s. **Seven audit records allMATCH**, and post-run fetch still matched b3182ff9. Thus this final run has a valid fresh review gate; C148's stale-approval error is not repeated.

**PASS: one final shared native run,84.78wallseconds, upper exit -> sea -> real dry shore near target654.** No isolated water game. One US#2, same temporary E223oneStrength data/navmanifest, nav1/jump_links1/PS, doors0/vehicles0; native initial upper spawn, no forced resampling/teleport/client/local game build. Actorless effective-recipe preflight passed before the single bot observation.

| Trace time | Native evidence |
| --- | --- |
| .28 | Target6541794.54,1690.64,101.53 selected |
| .92 ->5.48 | Strength LINK start ->done at2114.97,1885.80,105.91 |
| 14.285–28.035 | First sea swim, starts2054.111,1880.155,97.501 |
| 28.308–53.795 | Intermediate native non-swimming/wading bar; continued, not final shore acceptance |
| 54.058–75.295 | Second sea swim, ends1871.167,1689.236,98.064 |
| 75.542–82.548 | Final non-swimming ground segment,7.006s; endpoint1825.403,1685.867,100.957/grounded1/swim0/flying0/running6.75 |

Endpoint is **31.235m from shore target654**, feet1.467m above the native water plane (raw relativeWater2.90 includes a different native reference, not feet height). Actual committed-core postcheck: non-water polygon snapped101.034, same component/reachable654, remaining path3corners/0links/enddistance.001. Grounded native movement is physical shore evidence; mesh data independently supports classification. The first intermediate bar was not allowed to stop the run: final candidate also required within40m of the native shore target.

**Zero NAV fallbacks, zero start_unprojected/no_route/link failures, zero Lua/script/RMI/errors; health100 in all330samples**,142swim samples. Only reported goal654; capture is not claimed (stopped31m short). Local water-roam repair is separately covered by the real-core878paths oracle; this PS capture route does not force that fallback branch.

Archive `C:/Users/Magic8/WarsBotsBuild/crymp-multibot/build/c149-route-b8b444d6-v1`: args/effective recipe/fullcommand2011, logs/trace/summary,7reviewfreshness records/post-run check, fresh memory/nav before-after/restored receipts and actual demo. WBDM2 header exactb8/Shore,1638frames/onebot65481, record tail complete; SHA256f151a843c14843b2be63e62fec58281132d15385004a22229e461db19fde9ecc. **Current memory/nav filenames AND byte hashes exactly equal fresh pre-run snapshots**, temporary Shore files absent again; no game process retained.

[Machine-readable result](C149_UPPER_SHORE_RESULT.json). No default/data promotion, full capture/full PS match/full-map/lower12spawns/41historicalholes/heard-water goals/visual animation certification. DS094 audits this same archive without another game. This final b8 gate-approved result supersedes the older def8 physical-only result.
