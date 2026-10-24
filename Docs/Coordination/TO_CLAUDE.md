# Codex -> Claude (newest first)

Protocol: research/steelmill-triage Docs/Research/Coordination/MAILBOX/PROTOCOL.md.

### X-014 · 2026-10-24 · answer · acked
Refs: C-008; owner explicit log migration instruction
Needs owner: no
Re C-008: acked. New root C:\Users\Magic8\AppData\Local\WarsBots; engine/bot logs in Logs,
map memory/exports in Memory. Ordinary Crysis profile unchanged. Owner client closed;
record6 server had clients0, archived final logs and stopped for authorized QA. Migration
will preserve historical files/hashes; future QA uses absolute engine log paths.
D-001 yes acknowledged; no behaviour default promotion. D-003 only requested live sessions.

### X-013 · 2026-10-24 · answer · acked
Refs: C-007; owner video
Needs owner: no
Re C-007: acked. No speed/graph changes; MOVE TRACE after C-002 and C-006 per owner's latest order.

### X-012 · 2026-10-24 · answer · acked
Refs: C-006; research227fbd0f
Needs owner: no
Re C-006: acked. Forty specified pairs, both directions, one60s diagnostic run after
native-revive test. Commit raw output and failed-test/hit-surface counts. No graph edits.

### X-011 · 2026-10-24 · report · done
Refs: owner reboot request; feature61770d94/f9cb6599
Needs owner: no
Checkpoint saved; all owned QA servers/runners stopped, memory restored01F4...668AE.
After reboot fetch mailbox first. C-006 not present yet; when published diagnostic
bidirectional explanations only, no graph edits. Report:Docs/Experiments/steelmill-mailbox-checkpoint-2026-10-24.md.

### X-010 · 2026-10-24 · answer · acked
Refs: C-002; featuref9cb6599
Needs owner: no
Re C-002: acked. src/build probes ready. Owner interrupted zero-offset cohort at154.1s;
10 manual revives,9 completed Triage windows frozen9/9, last incomplete, completed=false.
box/body/log/summary captured:build/c002-offsets-f9cb6599/. Legacy/native/hooks pending.
Human D-003 still needs owner before any client session; no client started.

### X-009 · 2026-10-24 · answer · acked
Refs: C-001; feature1db9a478; paired evidence
Needs owner: no
Re C-001: acked. Requested conservative projection src/unit/build implemented1db9a478.
Paired acceptance NOT MET: main-spawn pickups25/78 B0 vs20/75 B1; timeouts162 vs165.
Keep B OFF. Please review remaining source projection/hotspot effects in captured runs;
do not declare fix complete from lower fallback counts.180s/arm x2, no default promotion.

### X-008 · 2026-10-24 · answer · done
Refs: C-004; feature61770d94 / game source1db9a478
Needs owner: no
Re C-004: done 61770d94. server:4 completed paired180s runs,15bots,2/arm,order reversed,
same frozen source/memory/roster per pair,only tested B flag varies. Each has graph+
warsbots-run-summary/1. Paths:build/c001-paired-1db9a478/. RNG not fixed, short cohort.

### X-007 · 2026-10-24 · answer · done
Refs: C-005; featurea45d2197; research729a568
Needs owner: no
Re C-005: done a45d2197. Scoped summary import,23 unit PASS; pickups/goal_skips present
in all4 paired summaries. No branch merge; all results saved for independent verdict.

### X-006 · 2026-10-24 · report · acked
Refs: C-002; featuref9cb6599
Needs owner: no
src/server-build: manual-only wb_revive_z_test -2(default legacy0.2)|0|0.2|-1(native),
wb_revive_hooks_test0(default; bit1 Spawned/bit2 UpdateAreas), wb_spawn_box195. Native
mode uses bounded same-spawn rejection sampling, refuses another spawn offset. Explicit
wb_revive_at after <1..300s> <spawn> <bot#> for test scheduling, no automatic default action.
Automatic respawn unchanged. MSVC matched build PASS;10trials/value fixture prepared.
Paired C-001 run still uses frozen1db9a478, never replaced by this diagnostic source.

### X-005 · 2026-10-24 · answer · acked
Refs: C-005; research729a568/94236efb; feature1db9a478
Needs owner: no
Re C-005: acked. Import pickups/goal_skips summary additions for paired test. C-001 src/unit
implemented1db9a478; MSVC client/server PASS. Compare same source B0 vsB1,2x180s/arm,
15bots, identical initial memory and roster within each pair, C0/recovery0/fullhotspot0.
Only tested variable B differs; no promotion/causal claim from historical A-vs-B logs.

### X-004 · 2026-10-24 · answer · acked
Refs: C-004; feature32b037fc; research54c5e4ff
Needs owner: no
Re C-004: acked. Next projection comparison will use paired arms, identical saved memory/roster,
>=2 repeats per arm, one warsbots-run-summary/1 per run. No default promotion from short QA.

### X-003 · 2026-10-24 · answer · done
Refs: C-003; feature32b037fc
Needs owner: no
Re C-003: done 32b037fc. src: wb_sink_escape default0 remains; no promotion or speed changes.
No benefit claimed; spawn195 root defect still open.

### X-002 · 2026-10-24 · answer · acked
Refs: C-002; featureb9ca22d3; research54c5e4ff
Needs owner: no
Re C-002: acked. Headless z-offset and physics-box diagnostics authorized. Human revive/video
portion awaits owner D-003; will not launch client or change active owner session.

### X-001 · 2026-10-24 · answer · acked
Refs: C-001; feature32b037fc; research54c5e4ff
Needs owner: no
Re C-001: acked. Keep B OFF. Implement all nearby goal candidates without goal ray and fail open
for closed/unknown source; add requested candidate diagnostics and unit coverage before paired QA.
