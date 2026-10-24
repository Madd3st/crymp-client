# Steelmill Triage

Offline, read-only tools for WarsBots navigation and dataset research. Python 3.9+, stdlib
only. Nothing here talks to a running game, and nothing writes next to its inputs.

```
python Tools/SteelmillTriage/steelmill_triage.py mem  "<My Games>/Crysis/WarsBots/Multiplayer_IA_Steelmill.mem" [--against before.mem]
python Tools/SteelmillTriage/steelmill_triage.py analyze <run.bots.log>... --mem <level.mem> --out lab-out
python Tools/SteelmillTriage/steelmill_triage.py graph <level.graph.json> --mem <level.mem> --replay <run.bots.log>
python Tools/SteelmillTriage/steelmill_triage.py summary <run.bots.log> --graph <level.nav.json> --mem <level.mem> --out summary.json
python -m unittest discover -s Tools/SteelmillTriage/tests
```

`analyze` writes `triage_report.json` and `steelmill_triage.html` (self-contained map viewer:
layers, floor filter, hover details, click a trap row to zoom its floor). Copy logs out of
`My Games/Crysis` first if a QA run might still be writing them.

| Tool | Works today | Needs |
| --- | --- | --- |
| `.mem` parser, fingerprint, growth diff, PickHotspot audit | yes, on real memory | — |
| Log parser (runs, slot→EntityId across resets, telemetry-block join) | yes, on real logs | — |
| Stall / pinned / trap detection with blocked-vs-moving split | yes | 2 s telemetry limits resolution |
| NAV fallback profile (distance, floor change, hot goals/starts) | yes | failure reason not logged |
| Dataset quality checks (action vs state, unattributed lines) | yes | — |
| Movement pace by state, sprint/suit/energy, bot vs human through the same LOS channel (`movement`) | yes | human trace (`MOVE TRACE`) for a real baseline |
| Level facts: 36 spawns, 78 map-placed items, areas, minimap PNG + bounds from `level.pak`/`<Level>.xml`/`.dds` (`level`, ported from Steelmill Lab) | yes, on real Steelmill files; outputs contain game assets — keep them out of git | — |
| ML dataset readiness of Codex `episodes.jsonl` (`episodes`, ported from Steelmill Lab) | yes | — |
| Per-run verdict `summary.json` (`warsbots-run-summary/1`, see `Docs/Research/SteelmillTriage/SUMMARY_CONTRACT.md`) | yes | — |
| Map viewer: positions, deaths, revives, stalls, pins, fallbacks | yes | — |
| Graph reader (`warsbots/botnav-graph/1`): contract §4 validation, §5 fingerprint, `memory_compat` vs `.mem`, components, dead ends, vertical classes, offline A* replay of `NAV fallback` | yes, on the Lab fixture + synthetic tests; exit code 2 on an invalid export | a real `wb_nav_export` file (HANDOFF step 4) |

`graph` output: `valid`, `errors`, `warnings`, then the graph summary; with `--mem`, `memory_link: OK`
means `BotNav::Load` would accept that memory for this graph. Offline `Nearest`/`FindPath` follow
`BotNav.cpp` line by line (C++ `int()` truncation for the column, strict `<` ties, 6000-expansion cap,
success as soon as the goal has a parent). The legacy `wbnav/1` schema is rejected.

Limits: positions are telemetry samples, not the walkable surface; `blocked` is BotMovement's
refusal, not physics contact; offline A* omits the visibility ray in `BotNav::Nearest`, so an
offline "ok" for an in-game failure points at that ray or at memory penalties.

## navmesh (D-006)

`python steelmill_triage.py navmesh <geometry.obj> [--bounds x0,y0,x1,y1] [--points map.json | --spawns s.json --items i.json]
[--trace <MOVE TRACE logs>] [--png top.png] [--cs 0.3 --climb 0.5 --radius 0.4 --height 1.8 --max-drop 4 --slope 45]`

Recast-style walkability model built offline from exported collision geometry (C-019 `wb_geom_export`): span
heightfield, slope + clearance, climb links, one-way drops, radius erosion, SCCs. Prints the same questions the
BotNav graph is judged by — main-part share, sinks, spawns/items by class (main / reach-main-only / from-main-only /
island / off-mesh) and the owner route: on-mesh % and reproducible % of consecutive samples. PNG: green two-way with
main, orange drop-off only, red no way back, grey isolated. Evaluation model only; the game links real Recast/Detour.
~8 s for a 400×300 m map at 0.3 m cells.
