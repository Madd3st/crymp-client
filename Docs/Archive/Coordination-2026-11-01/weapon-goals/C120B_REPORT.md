# C-120b on exact 4d399cf4 results

CI 40488976371 green; 27 MSVC unit suites, including BotAuditFixes/BotProbeRunner, PASS. Exact matching EXEs and canonical navmesh manifest SHA256 verified. No local build, GUI or 15-bot run.

## PS-0

| Map | Registered | Seconds | Crash | channel0 RMI | Lua errors | Native revives | Delegated | PP |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| Shore | 6/6 |240|0|0|0|0|4|UNKNOWN|
| Plantation | 6/6 |240|0|0|0|0|5|UNKNOWN|

Shore startup preflight was first, before roster, with zero Lua errors. Both 240 s runs completed. Error-count dictionaries empty (all strings0). Native demo revives0, RESPAWNING/fallback/request logs0: **revive-wave gate FAIL**, not a full PS PASS. PP unknown (no values/observer), not zero. First spawn/group/team/own-base distances and all error strings in C120B_RESULTS.json + build/c120b-ps0-4d399cf4/ps-evidence.json. Shore: 4 kills, all self-kills; Plantation: 5 kills, 4 self-kills. No artificial damage or revive commands sent.

## IA pair

C119 preset, state_commit 1, tactics 1 and installed spots, fire_diagnostics 1 both. Only differences first_primary/withdraw_cover/finish_lock 0 vs 1, verified preflight and real run console. Same source/roster/memory hashes; independent random_device seeds, not matched seed. One pair is observational, no promotion.

| Metric | OFF | ON |
| --- | ---: | ---: |
| Kills/stalls/traps>=60s |35/11/0|38/10/0|
| First primary p50/p90 s |4.676/13.463|3.893/9.391|
| Primary observed/eligible lives |37/46|47/49|
| SOCOM armed alive time % |23.28|20.47|
| SOCOM shot % (selected FRAME weapon) |14.72|2.53|
| Withdraw deaths<=2.5s |13/19 (68.42%)|6/12 (50.0%)|
| withdraw_abort |0|12|
| <=25HP TARGET switch-aways |0/40|0/42|
| FIRE before-Lua hit/shot accuracy % |26.27|25.68|
| Native demo hit/shot proxy % |26.34|26.47|

| Skill | K/D OFF->ON | FIRE accuracy OFF->ON % | Primary p50/p90 OFF->ON s | SOCOM armed-time OFF->ON % | Withdraw <=2.5s OFF->ON | abort OFF->ON | <=25HP TARGET away OFF->ON |
| --- | --- | --- | --- | --- | --- | --- | --- |
|beginner|2/13 -> 3/14|15.97 -> 16.33|3.39/8.96 -> 3.76/7.63|21.68 -> 19.38|0/0 -> 0/0|0 -> 0|0/4 -> 0/6|
|normal|13/16 -> 15/13|29.34 -> 29.37|4.56/12.40 -> 3.85/14.78|30.44 -> 23.9|10/12 -> 2/3|0 -> 5|0/13 -> 0/17|
|veteran|4/2 -> 5/5|21.59 -> 27.48|10.71/13.46 -> 4.43/5.94|10.33 -> 18.57|1/1 -> 1/2|0 -> 4|0/7 -> 0/9|
|elite|16/4 -> 15/6|28.85 -> 24.19|8.66/27.56 -> 6.56/9.39|26.47 -> 17.72|2/6 -> 3/7|0 -> 3|0/16 -> 0/10|

Definitions: first primary is per-life first firearm ownership exceptSOCOM (create/revive/FRAME timing, inventory sampling upper bound; unobserved lives censored). SOCOM armed-time andshot-share are different denominators, both shown. Accuracy primary column is FIRE HIT before-Lua / FIRE SHOT, so no before/after double counting; native demo proxy includes different callbacks/tail scope, bothcounts retained. Pellet events are not unique-bullet probabilities. Withdraw starts from native TACTIC logs (5 s cooldown) isintent, not movement/causation; aborts excluded fromwithdraw-success denominator and reported separately.

**Metric clarification:** intended <=25HP is CURRENT TARGET health and target changes. X164 own-HP weapon-transition metric was a different explicitly defined measure, not finish-lock baseline. Here eligible continuous lowHPtargetepisodes, alive target, visible-last<1s; nonzero targetchange countsaway, target0drops separate. Native kill events supersede staleFRAME: an apparentONswitch at228.055 was actuallysame-timestamp killthennewtarget. Afterexcludingdeath, away0/40->0/42. FINISH LOCK logs 0 in both arms; this pair does not demonstrate a lock benefit. Legalclose-retaliator exceptions included inrawswitches ifpresent.

## Probe and disposition

**BotProbeRunner NOT WIRED**: engine-free header + CIunit tests; only wb_probe_run occurrence generates command text insideheader. No engine include or console registration/caller. No external PowerShell probe was mislabeled as runner smoke. Evidence build/c120b-probe-wiring.json.

All own servers stopped, original four memory files restored byhash; foreign launcher C08C unchanged. No gameplaycode/promotions/merges. One push, then STOP; waiting for Claude on PS revive/wiring. Raw: build/c120b-ps0-4d399cf4/, build/c120b-flags0-4d399cf4/, build/c120b-flags1-4d399cf4/, build/c120b-unit-logs/.
