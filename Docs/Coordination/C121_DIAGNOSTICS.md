# C-121 diagnostics on d6a9a521

Exact green CI40498950981;28MSVC unit suites and artifact/datahashes verified. No local build or GUI.

## PS Shore, 6 x 300 s

Four deaths; QUEUE 4, WAVE 84, REVIVE result=true 4, false 0; fallback/refused/deferred all0. No crash, Lua or channel0RMI errors. Four same-actor returns alive with100HP; post-wave BRAIN SEARCHING_ENEMY. Wave is now confirmed independently of missing native demo revive events0. At60/120s there were4deaths, so no forced kill needed.

| Slot / actor | Death t | Same actor alive t | Delay s | Queue / wave / revive / delegated / fallback / refused / deferred |
| --- | ---: | ---: | ---: | --- |
|#2 / 65481|3.119|20.448|17.329|1/1/1/1/0/0/0|
|#5 / 65466|3.119|20.448|17.329|1/1/1/1/0/0/0|
|#2 / 130998|34.681|51.981|17.300|1/1/1/1/0/0/0|
|#1 / 130993|34.681|51.981|17.300|1/1/1/1/0/0/0|

The alive time is the first20Hz FRAME after death with sameEntityId/health>0. Lua logs have no own sidecar clock: exact engine line numbers and adjacent native timestamp brackets are preserved, not invented callback times. Per-death raw timeline: build/c121-ps0-d6a9a521/Shore/death-timelines.json; first40 lines below and ps-diagnostic-first40.log; full raw ps-diagnostic-raw-all.json.

```text
L201 t~[3.12, 3.12] [WarsBots] PS QUEUE add WarsBot_02 [Normal/Assault] group=userdata: 0000000000000219 team=2
L206 t~[3.12, 3.12] [WarsBots] PS QUEUE add WarsBot_05 [Normal/Defender] group=userdata: 0000000000000078 team=1
L219 t~[8.09, 8.09] [WarsBots] #2 PS revive delegated to the wave (5 s self-revive skipped, fallback 30 s) actor=65481.
L220 t~[8.09, 8.09] [WarsBots] #5 PS revive delegated to the wave (5 s self-revive skipped, fallback 30 s) actor=65466.
L231 t~[20.24, 30.12] [WarsBots] PS WAVE WarsBot_04 [Normal/Flanker] active=false canRevive=true group=userdata: 0000000000000219 dead=false spec=0 overdue=nil
L232 t~[20.24, 30.12] [WarsBots] PS WAVE WarsBot_01 [Normal/Universal] active=false canRevive=true group=userdata: 0000000000000078 dead=false spec=0 overdue=nil
L233 t~[20.24, 30.12] [WarsBots] PS WAVE WarsBot_05 [Normal/Defender] active=true canRevive=true group=userdata: 0000000000000078 dead=true spec=3 overdue=nil
L234 t~[20.24, 30.12] [WarsBots] PS WAVE WarsBot_02 [Normal/Assault] active=true canRevive=true group=userdata: 0000000000000219 dead=true spec=3 overdue=nil
L235 t~[20.24, 30.12] [WarsBots] PS WAVE WarsBot_06 [Normal/Hunter] active=false canRevive=true group=userdata: 0000000000000219 dead=false spec=0 overdue=nil
L236 t~[20.24, 30.12] [WarsBots] PS WAVE WarsBot_03 [Normal/Marksman] active=false canRevive=true group=userdata: 0000000000000078 dead=false spec=0 overdue=nil
L237 t~[20.24, 30.12] [WarsBots] PS REVIVE WarsBot_05 [Normal/Defender] result=true
L238 t~[20.24, 30.12] [WarsBots] PS REVIVE WarsBot_02 [Normal/Assault] result=true
L300 t~[34.68, 34.68] [WarsBots] PS QUEUE add WarsBot_02 [Normal/Assault] group=userdata: 0000000000000219 team=2
L305 t~[34.68, 34.68] [WarsBots] PS QUEUE add WarsBot_01 [Normal/Universal] group=userdata: 0000000000000078 team=1
L318 t~[39.65, 39.65] [WarsBots] #1 PS revive delegated to the wave (5 s self-revive skipped, fallback 30 s) actor=130993.
L319 t~[39.65, 39.65] [WarsBots] #2 PS revive delegated to the wave (5 s self-revive skipped, fallback 30 s) actor=130998.
L328 t~[50.31, 60.17] [WarsBots] PS WAVE WarsBot_06 [Normal/Hunter] active=false canRevive=true group=userdata: 0000000000000219 dead=false spec=0 overdue=nil
L329 t~[50.31, 60.17] [WarsBots] PS WAVE WarsBot_05 [Normal/Defender] active=false canRevive=true group=userdata: 0000000000000078 dead=false spec=0 overdue=nil
L330 t~[50.31, 60.17] [WarsBots] PS WAVE WarsBot_01 [Normal/Universal] active=true canRevive=true group=userdata: 0000000000000078 dead=true spec=3 overdue=nil
L331 t~[50.31, 60.17] [WarsBots] PS WAVE WarsBot_04 [Normal/Flanker] active=false canRevive=true group=userdata: 0000000000000219 dead=false spec=0 overdue=nil
L332 t~[50.31, 60.17] [WarsBots] PS WAVE WarsBot_03 [Normal/Marksman] active=false canRevive=true group=userdata: 0000000000000078 dead=false spec=0 overdue=nil
L333 t~[50.31, 60.17] [WarsBots] PS WAVE WarsBot_02 [Normal/Assault] active=true canRevive=true group=userdata: 0000000000000219 dead=true spec=3 overdue=nil
L334 t~[50.31, 60.17] [WarsBots] PS REVIVE WarsBot_01 [Normal/Universal] result=true
L335 t~[50.31, 60.17] [WarsBots] PS REVIVE WarsBot_02 [Normal/Assault] result=true
L344 t~[70.36, 80.2] [WarsBots] PS WAVE WarsBot_06 [Normal/Hunter] active=false canRevive=true group=userdata: 0000000000000219 dead=false spec=0 overdue=nil
L345 t~[70.36, 80.2] [WarsBots] PS WAVE WarsBot_05 [Normal/Defender] active=false canRevive=true group=userdata: 0000000000000078 dead=false spec=0 overdue=nil
L346 t~[70.36, 80.2] [WarsBots] PS WAVE WarsBot_01 [Normal/Universal] active=false canRevive=true group=userdata: 0000000000000078 dead=false spec=0 overdue=nil
L347 t~[70.36, 80.2] [WarsBots] PS WAVE WarsBot_04 [Normal/Flanker] active=false canRevive=true group=userdata: 0000000000000219 dead=false spec=0 overdue=nil
L348 t~[70.36, 80.2] [WarsBots] PS WAVE WarsBot_03 [Normal/Marksman] active=false canRevive=true group=userdata: 0000000000000078 dead=false spec=0 overdue=nil
L349 t~[70.36, 80.2] [WarsBots] PS WAVE WarsBot_02 [Normal/Assault] active=false canRevive=true group=userdata: 0000000000000219 dead=false spec=0 overdue=nil
L358 t~[90.39, 100.23] [WarsBots] PS WAVE WarsBot_06 [Normal/Hunter] active=false canRevive=true group=userdata: 0000000000000219 dead=false spec=0 overdue=nil
L359 t~[90.39, 100.23] [WarsBots] PS WAVE WarsBot_05 [Normal/Defender] active=false canRevive=true group=userdata: 0000000000000078 dead=false spec=0 overdue=nil
L360 t~[90.39, 100.23] [WarsBots] PS WAVE WarsBot_01 [Normal/Universal] active=false canRevive=true group=userdata: 0000000000000078 dead=false spec=0 overdue=nil
L361 t~[90.39, 100.23] [WarsBots] PS WAVE WarsBot_04 [Normal/Flanker] active=false canRevive=true group=userdata: 0000000000000219 dead=false spec=0 overdue=nil
L362 t~[90.39, 100.23] [WarsBots] PS WAVE WarsBot_03 [Normal/Marksman] active=false canRevive=true group=userdata: 0000000000000078 dead=false spec=0 overdue=nil
L363 t~[90.39, 100.23] [WarsBots] PS WAVE WarsBot_02 [Normal/Assault] active=false canRevive=true group=userdata: 0000000000000219 dead=false spec=0 overdue=nil
L372 t~[110.41, 120.24] [WarsBots] PS WAVE WarsBot_06 [Normal/Hunter] active=false canRevive=true group=userdata: 0000000000000219 dead=false spec=0 overdue=nil
L373 t~[110.41, 120.24] [WarsBots] PS WAVE WarsBot_05 [Normal/Defender] active=false canRevive=true group=userdata: 0000000000000078 dead=false spec=0 overdue=nil
L374 t~[110.41, 120.24] [WarsBots] PS WAVE WarsBot_01 [Normal/Universal] active=false canRevive=true group=userdata: 0000000000000078 dead=false spec=0 overdue=nil
L375 t~[110.41, 120.24] [WarsBots] PS WAVE WarsBot_04 [Normal/Flanker] active=false canRevive=true group=userdata: 0000000000000219 dead=false spec=0 overdue=nil
```

## Sniper probes

Pinned updated source fixtures; explicit C117vantage1/rank1/arbiter0/state1/pathprogress1 plus sniper_reach1. Roof fixture grades DSG1 pickup<=60s, no longer the old impossible+4.3m strength link.

| Probe / trial | Sniper pickup / time | Unreachable | LINK failure reasons | too_high | GOAL SKIP lines |
| --- | --- | ---: | --- | ---: | ---: |
|roof_strength / 1|DSG1: none in completed trial|1|{'missed_landing': 2, 'no_energy': 1}|0|7|
|roof_strength / 2|DSG1: none in completed trial|1|{'missed_landing': 1, 'no_energy': 1}|0|7|
|roof_strength / 3|DSG1: none in completed trial|1|{'no_takeoff': 2, 'no_energy': 1}|0|7|
|sniper_reach_dsg1 / 1|DSG1: none in completed trial|1|{'no_takeoff': 2}|0|0|
|sniper_reach_dsg1 / 2|DSG1: none in completed trial|1|{'no_takeoff': 1, 'missed_landing': 1}|0|6|
|sniper_reach_dsg1 / 3|DSG1: none in completed trial|1|{'missed_landing': 1}|0|8|
|sniper_reach_gauss / 1|GaussRifle: none in completed trial|0|{'no_energy': 1, 'no_takeoff': 1, 'missed_landing': 1}|0|374|
|sniper_reach_gauss / 2|GaussRifle: none in completed trial|0|{'no_energy': 1, 'no_takeoff': 2}|0|376|
|sniper_reach_gauss / 3|GaussRifle: none in completed trial|0|{'no_energy': 1, 'no_takeoff': 1, 'missed_landing': 1}|0|373|

All9 complete with no runner error; sniper pickups0/9 and fullassertions0/9. Too_high0: this newguard was not exercised, not independently validated by these fixtures. Raw per-trial GOALSKIP/unreachable/LINK/select lines, metrics, assertions, demos andSHA are in build/c121-{roof_strength,sniper_reach_dsg1,sniper_reach_gauss}-d6a9a521/ and C121_DIAGNOSTICS.json.

One earlier PS attempt stopped at60s due QA numeric-key hashtable serialization; preserved as invalid, not gameplay failure or valid300s result. Fixed serialization of values array; only invalid PS test repeated. Current memory restored. Next: one15x660 IA on same exactSHA after X166; no other runs.
