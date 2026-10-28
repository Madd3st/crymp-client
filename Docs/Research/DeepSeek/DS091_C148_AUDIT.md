# DS-091: C148 result audit (`def8a65d`, one shared upper -> sea -> dry shore run)

Archive `build/c148-route-def8a65d-v1`; report `C148_UPPER_SHORE_RESULT.md|json` (weapon-goals `8638b041`). Offline
audit of trace/log/summary/demo/restoration receipts; no game/build.

## Verdict: **`DS091 C148 def8a65d: PASS`** - upper route + one Strength exit + sea + dry shore near target 654,
inside the stated limits (no capture, no full map, no lower-deck route, no data promotion, no visual claim).

| check | independent result |
| --- | --- |
| source / harness | `def8a65d`, harness sha `9f14e9a7...`, command 2011 B, data `E2235993`, one US2, `doors/vehicles 0` |
| one run only | actorless preflight has 0 `MOVE TRACE`; one run log/one demo (`fd409447...`, 1642 frames, one actor); no second bot run |
| phases | LINK start 0.27 -> done 3.10; sea 11.52 (2054.84,1880.62,97.86); intermediate ground 25.5-54.8 (grounded 120/120 rows, z 97.97-101.83: sandbank wading); sea 55.0-76.3; final ground 76.5-82.55 (25/25 grounded, z 97.98 -> 100.97) |
| endpoint | (1822.856,1685.845,100.967), `swim=0`, `relativeWater +2.92`; `area-query16384`: on-mesh, snapped `101.081`, land component; 654 same component; 28.7 m away, 3 corners/0 links remaining |
| errors / health | 0 `[Error]`/Lua/Script/RMI lines in both logs; health 100 in all 330 samples |
| handover fallback | exactly one `NAV fallback` at 25.55 `start=(2013.92,1851.05,98.25)` goal 654 `reason=no_route from=27243 to=23683 expanded=1243`; route continued, `start_unprojected=0` |
| restoration | current memory+nav exact names/hashes restored, temporary Shore files absent, no active game |

**The early-stop guard mattered:** the first dry-looking sample is 29.275 s at (2006.28,1843.32,100.555) - dry,
grounded, >100 m from the sub, but **259 m** from 654. The pre-DS090 heuristic would have stopped there; the
"within 40 m of the shoreline target" condition correctly rejected it and the run continued to the real shore.

## Limits (must accompany the PASS)

* One US2 spawn, one audited `strength` link on the temporary E223 mesh; **not** the lower 12 US spawns, not the 41
  historical water holes, not a full-map route.
* Capture **not** completed: the harness stopped 28.7 m short (3 corners / 0 links left); no capture event, no PP,
  no "Playable".
* One `no_route` repath at the water->dry handover - the plan was not valid on every tick; the run self-recovered.
* No visual/video confirmation; no default flip, no data promotion (candidate restored/absent after the run).
* The DS-090b B1 water-roam defect stays latent (never exercised here) and is still worth the one-line fix before a
  run that depends on the local water fallback.
