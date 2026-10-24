# Run summary contract — `warsbots-run-summary/1`

One small JSON per QA run or owner playtest, so "better / same / worse" is visible right after a run
and comparable across builds. Produced by Triage; Codex's harness only calls it.

```
python Tools/SteelmillTriage/steelmill_triage.py summary <run.bots.log> \
    [--graph <Level>.nav.json] [--mem <Level>.mem] [--label <text>] [--out summary.json]
```

* Input: one `*.bots.log` (the longest run inside is used), optionally the graph export (`wbnav/1` from
  `wb_nav_export` or `warsbots/botnav-graph/1`, both accepted) and the `.mem` it was built with.
* Output: stdout and `--out`. Stdlib only, read-only, ~2–5 s for a 15-bot 11-minute log.
* Exit code 0 on success; a broken graph export raises `GraphError` (non-zero), the log alone never fails.
* Use the Triage version from the newest commit of `warsbots/research/steelmill-triage`. Fields may be
  **added** within `/1`; renaming or changing meaning bumps the schema.

## Fields

| Field | Meaning | Direction |
| --- | --- | --- |
| `schema`, `label`, `log`, `log_sha256`, `build`, `bots`, `seconds`, `live_bot_minutes` | identity of the run | — |
| `spawn195.revives` / `.frozen` | revives at spawn entity 195 / of those, bot moved < 0.3 m within ~12 s | frozen → 0 |
| `revives.total`, `.frozen`, `.frozen_spawns` | same test for every spawn; map spawnId → counts (only spawns with a freeze) | frozen → 0 |
| `watchdog.episodes`, `.at_spawn195`, `.reached_level{1..4}`, `.self_kills` | from `TRAP RECOVERY` lines; an episode = a level-1 event | episodes, self_kills ↓ |
| `pinned_live_percent` | % of live bot time "accepted but not moving" (Triage F1 detector) | ↓ |
| `traps_ge60s` | stuck episodes ≥ 60 s | → 0 |
| `stalls` | short stalls (≥ 4 s, < 1 m) | ↓ |
| `nav_fallback.total`, `.per_bot_minute`, `.reasons{}`, `.no_route_percent` | `NAV fallback` lines and native `reason=` | ↓ |
| `unreachable_weapon_skips` | `weapon N unreachable; skipping it` lines | ↓ |
| `pickups.total`, `.lives`, `.lives_with_pickup`, `.lives_with_pickup_percent`, `.by_spawn_kind` (with `--graph`: main / sink / isolated / other / no_entry) | weapon pickups per life, split by the component of the revive spawn | ↑ (B must not lower it) |
| `goal_skips{weapon,hotspot}` | `GOAL SKIP unreachable` lines (wb_reachable_goals) | — |
| `movement.over_4ms_percent`, `.native_sprint_percent`, `.median_speed_by_state_max`, `.moving_samples` | pace while moving (telemetry block) | target 35–55 % over 4 m/s |
| `graph` (only with `--graph`) | `fingerprint`, `nodes`, `edges`, `sink_sccs`, `nodes_in_sinks_percent`, `main0_main1_edges`, `spawns_outside_main`, `revives_on_non_main_spawns_percent`, `no_route_goal_outside_main_percent`, `memory_link` | sinks ↓, main0↔main1 edges > 0 |

Comparisons are valid only between runs of the same scenario (map, bot count, duration, flags).
Reference values (d4b0854c, recovery 1): see `PLAYTEST-2026-10-24-0458.md` (Recovery6, r3 soak).
