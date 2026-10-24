# Local IA reproduction commands

Manual only; requires experimental build and existing loopback LAN IA gate:
sv_bind127.0.0.1, sv_lanonly1, wb_probe_local_test1. No automatic relocation/revive.

- `wb_revive_at 195 1`: native kill (if alive) then native revive of owned bot#1
  at registered spawn EntityId195, using the same +0.2m offset as automatic revive.
- `wb_revive_at 195`: same, for selected bot (`wb_select 1`).
- `wb_revive_at 195 me`: listen-server local player; on a dedicated server only
  the sole real player (refuses none/ambiguous). Refuses spectators/vehicles/frozen actors.
- `wb_teleport 1 1247.5 1319.75 136.51`: relocate owned live bot#1, preserve AI on/off.
  Native ClMoveTo broadcasts rather than addressing nonexistent channel0.
- `wb_physics_diagnostics 1` before revive enables existing now/+1/+5 bot samples.

Bot numbers come from wb_list, spawnId is an EntityId registered with current GameRules,
not an ordinal. Invalid IDs/coordinates/closed gate refuse before mutation. The commands
are diagnostic: native forced death affects match statistics, so exclude these sessions
from performance/combat comparisons. Teleport is not an automatic recovery fix.
Human native revive is explicit test-only; ordinary player lifecycle unchanged.

Build: MSVC x64 Release experimental client/server PASS (initial ZERO constructor compile
error corrected; final log build/debug-commands-build-r2.log). No graphical test yet.

## Automatic QA summary

Test-WarsBotsTactics.ps1 invokes Invoke-WarsBotsTriage.ps1 in final cleanup, after
log capture and memory restoration. Captured runs (including recorded failed runs)
produce summary.json beside logs plus native triage_report.json/steelmill_triage.html.
No game/client is started by offline Triage. Python can be supplied through
-PythonExecutable; -TriageScript supports an explicitly selected future tool version.
Missing Python/tool or Triage failure fails a normally completed QA batch, and warns
without replacing the original exception during failed/interrupted QA. No runs captured
means no summary (no invented data). Previous summary is removed before analyzer rerun.

Tools/SteelmillTriage updated by scoped import to31e9394b, no branch merge.23 unit
tests PASS; real saved6/15-bot log+wbnav/1 replays PASS. The adapter invokes summary
with label/memory and the graph exported for that exact run, never another run graph.
Compact schema is warsbots-run-summary/1; analyze remains detailed steelmill-triage/1.
[Published contract](../Research/SteelmillTriage/SUMMARY_CONTRACT.md).
Single-run batch writes summary.json; multi-run batches write <tag>-summary.json
(one object per run, all beside captured logs). Labels include map,bots,requested
duration,flags and roster. Different scenarios are not assigned a comparative verdict.
Native absent graph is null; baseline without export never borrows candidate graph.
Contract watchdog.episodes counts level1; sink escape can start at level2, so use
reached_level2/self_kills as well, not episodes alone for C. No tool semantics changed.
End-to-end six-bot60s runner PASS: build/triage-runner-b9ca22d3/summary.json
created automatically, with both wbnav/1 exports accepted. No engine errors.
This is a pipeline smoke, not a10-minute behavioral soak.

## Native command smoke, source b9ca22d3

60s headless,2 stopped bots, no engine error markers. Forced revive195 preserves
actor65501 withhealth100; at+5 it reproduces F1 posez136.512/invalid groundHeight.
Teleport bot2 logs requested(1240,1320,136.5), stopped AI remains stopped.
Closed-gate commands,NaN coordinate,unknown bot/unknown spawn and headless me without
human all refuse. Native match start resets actors around31.51s; roster restores
normally. Same-actor revive assertion concerns the manual operation before that reset.
Raw evidence/checks:build/debug-commands-qa-b9ca22d3/. Matched artifacts:
build/play-b9ca22d3/. Original map memory restored. Human me, ambiguous multi-client,
FP/client teleport replication and running-AI teleport remain live/unexercised cases.
