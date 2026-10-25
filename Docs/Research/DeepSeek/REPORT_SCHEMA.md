# Batch report schema (`wbrun/1`) — DS-011 #2

One JSON per batch run, computed only from `*.bots.log` (+ optional `wbdemo/2` fields marked `demo`). Every X- report
must carry this object so runs are comparable. No hand-written counts.

## Identity (required for comparability)

`source_sha`, `server_sha256`, `client_sha256`, `map`, `rules` (`InstantAction`/`PowerStruggle`), `bots`, `seconds`,
`wall_seconds`, `time_scale` (must be 1), `flags` (all `wb_*` that were on), `navmesh_manifest` (sha256 or none),
`seed` (null until `wb_seed` exists), `log_sha256`, `host`.

## Fixed metrics (log source → rule)

| field | computation |
| --- | --- |
| `kills.native` / `kills.demo` | `AI SCORE ... kills=` at end (native) or `combat kill shooter=` events (demo); count unique `(t,shooter,target)` |
| `deaths` | `death observed EntityId=`; `AI SCORE ... deaths=` as cross-check |
| `revives.total` / `.frozen` | `revive result ... alive=1`; frozen = same actor moved <0.3 m within 12 s (`MOVE TRACE`/telemetry) |
| `pickups.total` | `REAL PICKUP ... acquired=1`; `pickups.per_bot_min` = total / (bots × seconds/60) |
| `stalls` | `MOVE TRACE`/`AI telemetry`: speed <1 m/s for ≥4 s while intent.move; count episodes per bot |
| `traps.by_level` | `TRAP RECOVERY level=N`; `.episodes` = level-1 count; `.self_kills` = level-4 |
| `trap_clusters` | see below |
| `nav_fallback` | `NAV fallback` lines; `.per_bot_min`; `reasons{}` from `reason=`; `no_route` share |
| `links.start/done/failed/banned` | `LINK start`, `LINK done`, `LINK failed ... reason=`, `-> fail noted/banned`; done% = done/start |
| `vantage.travel/relocate` | `VANTAGE travel`, `VANTAGE relocate`; claims are demo `BRAIN`/`INTENT` if available (log lacks a claim line) |
| `hold_angle` | `HOLD angle from` starts, `LOOT item=` events |
| `jumps.strength_takeoff/landing` | `JUMP TAKEOFF: strength=1`, `JUMP LANDING: strength=1`; landing rise/displacementZ |
| `decision_flips` | `ARBITER goal=` sequence per actor: A→B→A within 3 s; divide by bot-minutes |
| `state_time` | `AI telemetry ... state=` samples per state / total |
| `pace` | telemetry speed: median, `over_4mps_percent`, `moving_samples`, per state |
| `combat` | `TARGET CHOICE` (acquisitions), `FIRE HIT`/`combat kill` (hits/kills), first `TARGET CHOICE` latency after revive |
| `cpu` | `PROFILE bots= avgSum= worstFrame= rays= targetScans=` last report; per-bot `wb_status` avg/max |
| `net` (client) | `NET POSE REPLICA physicsReads/motionReads/snapMoves/snapMax` per 2 s window; max/mean |

`aim wander`, `reaction`, `time-to-primary`, weapon share come from `demo` (`steelman_triage.py demo`) and are marked
`source:"demo"`; do not mix them into log-only comparisons.

## Trap clusters (DS-009 method, fixed)

Parse every `TRAP RECOVERY level=N actor=A age=... pos=(x,y,z)` with a position. Union-find points with
`|Δ| ≤ 2.0 m` (3D). Keep components with `count ≥ 3`. Report, sorted by count desc:
`center` = mean x,y,z (1 decimal), `count`, `levels{}`, `actors` (unique), `first_t`, `last_t`.
Compare two runs by matching centers within 2 m; list unmatched clusters from both sides. Do **not** merge clusters
by name or by nearest if the distance is >2 m; a new cluster is a finding, not noise.

## JSON shape

```json
{ "schema":"wbrun/1",
  "run": { "source_sha":"...", "map":"multiplayer/ia/steelmill", "bots":15, "seconds":662,
           "flags":{"wb_bot_corners":0,"wb_bot_navmesh":1}, "seed":null },
  "metrics": { "kills":{"native":267,"demo":266}, "stalls":37, "traps":{"by_level":{"1":16,"2":9,"3":3,"4":4}},
               "pickups":{"total":195}, "nav_fallback":{"per_bot_min":0.4}, "links":{"done_percent":0.49} },
  "trap_clusters": [ {"center":[1306.1,1353.9,144.5],"count":23,"levels":{"1":15,"2":7,"3":1},"actors":6} ],
  "comparison": { "baseline":"c070-on-91a01bbc", "matched":[{"center":[...],"counts":[16,23]}],
                  "unmatched_baseline":[...], "unmatched_candidate":[...] } }
```

## Acceptance (per feature, not global)

Report the gate result explicitly, e.g. X-065: `stalls 135 > 48 FAIL`, `pickups 195 < 247*0.9 FAIL`, `kills 241 >= 258
PASS`, `traps/level4 0 PASS`. A run is comparable only at the same `bots`, `seconds`, `map`, `rules`, `flags` and
data manifest; otherwise say “not comparable” and do not compute deltas.

