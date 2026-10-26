# C-098 sniper route and +20 stalls (DS-023)

Source: owner PC, read-only. Candidate `build/c098-on15-9ac55001/` (`bots.log`, triage JSON, demo) vs control
`build/c097-on15-7179ec36/`. C-098 recipe: `path_progress=1`, `threat_scan=1`, look/vantage/perception=1; C-097:
same C-093 set without path/threat. Triage maps weapon item `98` to **DSG1**, `263` to **GaussRifle**;
`mission_mission0.xml` places DSG1 at (1307.23,1330.63,z=159.11) and Gauss at (1225.44,1331.80,z=149.26).

## 12 `weapon 98|263 unreachable` lines

All 12 are **98 (DSG1)**; there is no 263/Gauss drop.

| # | Bot/profile | t | Last position at drop | z vs DSG1 |
| ---: | --- | ---: | --- | --- |
| 15 | marksman/veteran | 61.94 | 1300.16,1352.93,142.56 | -16.5 m |
| 3 | marksman/veteran | 81.66 | 1360.60,1348.42,142.56 | -16.5 m |
| 9 | marksman/beginner | 144.04 | 1312.25,1303.40,142.80 | -16.3 m |
| 15 | marksman/veteran | 224.06 | 1360.60,1348.42,142.56 | -16.5 m |
| 3 | marksman/veteran | 293.55 | 1360.86,1348.75,142.56 | -16.5 m |
| 10 | flanker/normal | 380.88 | 1317.54,1336.76,151.62 | -7.5 m |
| 14 | assault/normal | 405.16 | 1333.75,1341.98,142.56 | -16.5 m |
| 9 | marksman/beginner | 441.14 | 1360.23,1348.42,142.77 | -16.3 m |
| 3 | marksman/veteran | 459.06 | 1301.23,1352.93,142.56 | -16.5 m |
| 15 | marksman/veteran | 478.20 | 1318.96,1313.20,142.77 | -16.3 m |
| 6 | hunter/normal | 516.29 | 1300.93,1352.93,142.56 | -16.5 m |
| 9 | marksman/beginner | 559.19 | 1314.05,1303.73,142.99 | -16.1 m |

Link context: 10/12 had **no LINK start/done/failed within 30 s**; #15 had one `LINK done` 22.2 s before its 224 s
drop; #14 had one `LINK failed` 13.3 s before its 405 s drop. No bot reached the DSG1 roof (`z=159.1`): 11/12 were
at z=142.6-143.0, one (#10) at z=151.6. So the failure is the **last vertical leg/roof access**, not a missing jump link.

The log does not contain `m_upgradeBest`/`m_upgradeSince`/remaining-path values, so it cannot prove whether the
8 s best timer or the 60 s cap dropped each target. The drop is 14-43 s after the last `MOVING_TO_WEAPON` state
entry, but state entry is not selection time. Add one `UPGRADE progress bot=... target=... d=... best=... age=...
mesh=... link=...` line before the next run.

## Selection / flips

Each of the 12 drops implies the bot selected DSG1 once: #15 x3, #3 x3, #9 x3, #10/#14/#6 x1. There is no
"upgrade target selected" log, only the final unreachable line, so the number of selection flips cannot be measured.
No bot selected Gauss (`263`); DSG1 is the only sniper target the upgrade path ever chose.

## Stalls: 39 vs 19

Candidate 39 stalls / 32 `MOVING_TO_WEAPON`; control 19 / 15. The extra stalls are expanded/moved
`MOVING_TO_WEAPON` clusters around the DSG1 approach, not the Gauss base:

| Candidate cluster (2 m) | C-098 | C-097 | Verdict |
| --- | --- | --- | --- |
| 1335,1341,143 | 7 / 58.5 s | 3 / 16.1 s | expanded |
| 1362,1350,143 | 5 / 30.3 s | 3 / 16.1 s | expanded |
| 1341,1350,152 | 5 / 36.3 s | 1 / 16.2 s | expanded |
| 1362,1347,143 | 4 / 34.2 s | 1 / 10.1 s | expanded |
| 1302,1353,143 | 4 / 20.2 s | 1 / 4.0 s | expanded |
| 1332,1344,143 | 3 / 16.2 s | — | new |
| 1317,1338,152 | 1 / 12.1 s | — | new, upper floor |
| 1215,1311,140 | 1 / 4.1 s | — | new, lower route |
| 1266,1311,144 | 1 / 4.0 s | — | new |
| 1299,1353,143 | 1 / 4.0 s | 1 / 4.0 s | same |

The five big clusters lie in the 1300-1360 x 1338-1353 band, on the lower/upper approach to the DSG1 roof; the
Gauss base (1225,1331,z149) has no matching cluster. `look_glances=0` (threat scan) cannot explain the extra
stalls; the likely driver is `path_progress=1` keeping the unreachable DSG1 goal alive and the bot re-walking the
lower approach. This is a correlation from the C-097/C-098 pair, not a proved single-variable causation.

## Recommendation

1. Gate the sniper goal on reaching the **item's floor**, not just a path to its x/y; log `UPGRADE progress` before
   calling it unreachable.
2. Do not enable `path_progress` for a goal whose last leg is a roof/edge not present in the mesh; add a
   `Reachable(goal)`/last-link check at selection time.
3. Keep `threat_scan` off until `GLANCE` count > 0; C-098 had 0.
4. Re-run one C-098 pair with only `path_progress=1` and one with only `threat_scan=1` if the +20 stalls must be
   attributed; the current pair changed both.
