# C-121 single15-bot regression on d6a9a521

Exact SHA d6a9a5219a04629b959fecd6285aab499baef2ed, green CI40498950981, matching MSVC client/server,28unit suites. One authorized15x660 at1x with20Hz demo and FIRE diagnostics; no GUI/localbuild.

## Gates and control

| Metric | Archived0fa9c1cb | d6a9a521 | Gate |
| --- | ---: | ---: | --- |
| Kills |293|287|>=264 PASS|
| Stalls |30|32|<=37 PASS|
| Traps>=60s |0|0|<=1 PASS|

Runtime and NAVMESH proof PASS; native demo287kills matches server count,13199frames readable. Watchdog13episodes reachedlevel1/2/3 counts13/4/1; no recovery selfkill. One observational run: many build/data changes vs archived baseline, no isolated causal claim or default promotion.

## Requested metrics

| Metric | Result |
| --- | --- |
| First primary p50/p90 |5.739/14.769s; observed 274/313 lives|
| SOCOM armed alive time / shot share |34.09% / 6.63%|
| Withdraw death<=2.5s |72/104 (69.23%); abort 85|
| FINISH LOCK |2 log entries|
| DSG1 / Gauss pickups |0 / 0|
| DSG1 / Gauss kills |0 / 0|
| FIRE accuracy |3072/10455 = 29.38%|
| Native FRAME window mean avg/p95/max |31.836/38.254/53.43ms|
| botAI |0.542ms/frame, mean of65steady windows|

| Skill | K/D | FIRE hit / shot | Accuracy | Primary p50/p90 s | SOCOM armed time % | Withdraw death<=2.5s |
| --- | --- | --- | --- | --- | --- | --- |
|beginner|33/92|412/1590|25.91%|5.218/15.853|35.56|0/0|
|normal|93/73|921/2836|32.48%|6.223/14.149|35.92|24/27|
|veteran|72/70|878/2862|30.68%|5.643/13.050|31.09|31/49|
|elite|89/52|861/3167|27.19%|6.521/11.961|33.8|17/28|

Accuracy uses FIRE HIT phase=before-Lua only over FIRE SHOT, no before/after doublecount; event ratio is not unique-bullet probability with pellet weapons. Kill weapon attribution uses same shooter/victim native FIRE HIT class within25ms of demo kill; all287 matched, no selected-weapon guess. First-primary sampling/censor definitions in JSON. Withdraw starts are intent, no causal survival claim. FRAMEp95 is mean of window p95s, not pooled per-framep95; full raw FRAME-lines.log retained.

LINK failure reasons: {'too_high': 3, 'missed_landing': 31, 'no_energy': 8, 'no_takeoff': 4}. Too_high was not hit in nine probe fixtures, but runtime15-bot run hit it3times. No sniper pickup/kill despite passing regression gates; sniper reach remains unproven/red as recorded in X166.

## PS interpretation and stop

X166 confirms PS queue -> wave -> Lua result=true -> sameActor alive100HP / BRAIN SEARCHING_ENEMY for all4deaths. No fallback/refused/deferred needed. Native demo revive events stay0: earlier C120b gate lacked observability, so that absence must not be used as proof the PS wave was mechanically broken. C121 PS changes are diagnostics, no claim of a new behavior fix. Full raw first40/per-death timelines in C121_DIAGNOSTICS.md/JSON.

Raw build/c121-on15-d6a9a521/; exact recipe/effective console/EXE hashes, raw server+bots logs, full20Hz demo, Triage summary/HTML, metrics and source analysis retained. All four original memory files restored byhash, foreignRecording C08C preserved, own processes stopped. No gameplay edits/merges/default promotions. C122/C123 read but not started per latest owner STOP after X167.
