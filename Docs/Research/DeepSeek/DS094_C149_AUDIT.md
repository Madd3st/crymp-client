# DS-094: C149 result audit - gate-approved `b8b444d6` whole-route run

Archive `build/c149-route-b8b444d6-v1`; report `C149_UPPER_SHORE_RESULT.md|json` (weapon-goals `cd08782c`). Offline
audit of trace/log/summary/demo/restore + review-gate receipts; no game/build.

## Verdict: **`DS094 C149 b8b444d6: PASS`** - one gate-approved shared upper -> sea -> dry shore run, with the
limits below (no capture, no full map, no lower-deck route, no visuals, no default/data promotion).

| check | independent result |
| --- | --- |
| review gate | 7 `c149-review-checks.jsonl` entries all `MATCH` my `b3182ff9` (15:02:37..15:04:05), plus `post-review-check.json` `MATCH_AFTER_RUN`; `review_gate_valid_at_launch=True` |
| source/CI/hashes | `b8b444d6`, CI 40709459511 green, paired artifacts `AE9EE3FA...` client / `8FD34B53...` server (match `build/ci-b8b444d6/sha256.txt`) |
| one run | actorless preflight (0 `MOVE TRACE`) + one run; one demo `f151a843...`, 1638 frames, one actor, `record_tail_complete=true` |
| route | LINK start 0.92 -> done 5.48; sea 14.285-28.035; wading bar (non-swim) 28.308-53.795 - not stopped; sea 54.058-75.295; final ground 75.542-82.548 |
| endpoint | (1825.403,1685.867,100.957), grounded, `swim=0`, feet **1.467 m above the native water**; 31.235 m from 654; 7.006 s of final ground motion |
| mesh proof | endpoint non-water poly `101.034`, 654 reachable, 3 corners/0 links, `end_distance 0.001` (my `area-query16384` agrees: land component, same component as 654) |
| robustness | 330 trace / 142 swim, health 100 throughout, **0** NAV fallback / `start_unprojected` / `no_route` / link-failure / error lines |
| restoration | current memory+nav exact names+hashes restored, temporary Shore absent, no active game |

The intermediate dry-looking sample (wading bar) is 250+ m from the target and correctly not treated as the shore;
the final 7 s of ground motion plus the 31 m remaining mesh path is what the 40 m guard classifies as the shore.

## Limits (must accompany the PASS)

* One US2 spawn, one audited `strength` link on the temporary E223 mesh: **not** the lower 12 US spawns, not the 41
  historical water holes, not a full-map route.
* Capture **not** completed (stopped 31.2 m short, 3 corners left); no capture event, no PP, no "Playable".
* Local water roam (878 committed paths) remains **offline actual-core coverage**, not forced at runtime; the heard
  water goal stays an open unknown.
* No visual/video confirmation; no default flip, no data promotion (candidate absent after the run).
* C148 (`def8a65d`) stays physical evidence only; C149 is the gate-approved run.
