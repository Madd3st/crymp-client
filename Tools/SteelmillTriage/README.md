# Steelmill Triage

Offline, read-only tools for WarsBots navigation and dataset research. Python 3.9+, stdlib
only. Nothing here talks to a running game, and nothing writes next to its inputs.

```
python Tools/SteelmillTriage/steelmill_triage.py mem  "<My Games>/Crysis/WarsBots/Multiplayer_IA_Steelmill.mem" [--against before.mem]
python Tools/SteelmillTriage/steelmill_triage.py analyze <run.bots.log>... --mem <level.mem> --out lab-out
python Tools/SteelmillTriage/steelmill_triage.py graph <level.nav.json> --mem <level.mem> --replay <run.bots.log>
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
| Map viewer: positions, deaths, revives, stalls, pins, fallbacks | yes | — |
| Graph layer, memory on map, components, one-way edges, offline A* replay | code + synthetic tests only | `wb_nav_export` ([contract](../../Docs/Research/SteelmillTriage/NAV_EXPORT_CONTRACT.md)) |

Limits: positions are telemetry samples, not the walkable surface; `blocked` is BotMovement's
refusal, not physics contact; offline A* omits the visibility ray in `BotNav::Nearest`, so an
offline "ok" for an in-game failure points at that ray or at memory penalties.
