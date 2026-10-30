---
name: warsbots-log-analysis
description: Analyze WarsBots session logs and archived receipts to explain goal selection, traversal failures and stuck behavior. Use for evidence-based match diagnostics and compact summaries; does not launch the game or create a new playtest.
---

# WarsBots log analysis

## Establish provenance

Identify the question, session directory, exact binary/source revision, preset/effective flags and relevant navmesh/links/spots version from receipts. Keep sessions/builds separate. Current task/status and AGENTS.md take precedence over old runtime restrictions or role assignments embedded in historical reports.

Local starting points: QA checkout `C:/Users/Magic8/WarsBotsBuild/crymp-multibot`; archives under `E:/WarsBots/Showcases`, `E:/WarsBots/Development`, `E:/WarsBots/Visual`. Consult the current START_HERE rather than recursively scanning drives. Use `rg --files` on the selected archive, then search relevant event names with bounded context.

## Read only the evidence needed

- Correlate by session, actor EntityId, slot, life/respawn and time. Slot number alone is not stable identity. State transitions from two lives are not one completed route.
- Main `.log` and `.bots.log` can contain the same events. Count from one complete stream, not both. Avoid including live-copy subfolders as additional sessions. Ignore a trailing incomplete row in a file still being written; distinguish a snapshot from final EOF.
- Verify an event's logging site at the session's source revision before interpreting its absence. An unimplemented `VANTAGE hold` message cannot establish zero holds. Repeated Travel lines need not mean new attempts; repeated HIGH GROUND rolls need not mean distinct roof routes.
- To demonstrate Travel/Heard overwrites, correlate ordered events with the same bot and time, then inspect the real caller's conditions and ordering. Report aggregate counts separately from per-bot counts. Coincident timestamps alone are not a general causal proof.
- Count failure reasons separately (`too_high`, `missed_landing`, `no_takeoff`, energy, timeout). A planner/executor mismatch does not establish that a specific roof route uses the rejected edge. Preserve direction, bidirectionality and type masks when inspecting link data.
- Separate goal chosen, route found, transition started, physical arrival, tactical hold and visual quality. A position above a Z threshold does not uniquely identify a roof or prove useful occupancy.

Prefer a small streaming/read-only Python aggregation for large logs, print a compact result and representative evidence with line numbers. Avoid putting raw multi-megabyte logs into model context. Do not create a generic parser when the existing analyzer fits; do not modify archived source logs, demos or snapshots.

## Reuse scenario-specific tools carefully

In the QA checkout, inspect `Tools/Diagnostics/Analyze-C154Route.py` and its CLI help when analyzing the controlled C154 route. It expects one registered lower545 setup and checks actor continuity/physical stages; it is not a generic Shore or interactive owner-session grader. Multiple owner revives can violate that recipe without invalidating each observed physical exit.

`Tools/Diagnostics/Test-WarsBotsDataManifest.py` can help with data provenance when its actual inputs fit. Inspect usage before execution; do not regenerate a receipt or repin quality merely to make an old gate pass.

Codex's logical `%LOCALAPPDATA%/WarsBots` may be MSIX-redirected to `C:/Users/Magic8/AppData/Local/Packages/OpenAI.Codex_2p2nqsd0c76g0/LocalCache/Local/WarsBots`. External agents may see another physical tree. Resolve and disclose which namespace a receipt and current-data comparison use. Never copy between them to hide a mismatch. Restore claims require matching file sets and hashes, not just matching counts or an exit code.

## Output

Provide a compact session/version summary, relevant counts with precise definitions, ordered evidence examples, conclusions labelled confirmed/inferred/unknown, and one discriminating next check. Preserve historical failures; later success does not erase them. State whether video was actually inspected or success was reported by the owner.

Default to reporting in chat. Save a derived summary only if requested, with provenance and outside archived evidence. This skill does not authorize runtime, changing flags/data, deletion or autonomous monitoring.
