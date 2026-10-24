# Mailbox / reboot checkpoint, 2026-10-24

Owner requested immediate stop for reboot. All own servers/runners stopped; no client
started. Memory restored01F4C7F4B894AE2AC492C381487FAC5B539824F83EC76C6F902F428A165668AE.
[Metrics/summaries](steelmill-mailbox-checkpoint-2026-10-24.json).

## Completed C-001 / C-004 evidence

Source1db9a478 changes goal projection to ANY nearby candidate (+/-2 columns,|dz|<=2.5),
without goal LOS ray. Unknown/closed source fails open; hotspot closed source also kept.
Requested skip diagnostics added. Focused BotGoalProjectionTests PASS; MSVC x64 Release
client/server PASS. Frozen build/play-1db9a478/ used for BOTH arms; only B0/B1 differs.
C0/recovery0/fullhotspot0/physicsdiag0 in both arms, defaults unchanged. Triage729a568
adds per-life/component pickups and goal_skips;23 tests PASS. Baseline flags recorded
as -1(default0), treatment explicit; roster/memory/source equal within each pair.

Four180s requested runs, observed178.22-179.14s, completed;2 repeats per arm, reverse
order in second pair. All15 bots, same saved memory; no fixed engine RNG seed.
Raw logs/graphs/per-run summary files/report/episodes:build/c001-paired-1db9a478/.

| Arm/repeat | Main-spawn lives with pickup | Weapon timeouts | median bot Update sum ms/frame |
|---|---:|---:|---:|
| B0/r1 |15/35 (42.9%)|82|0.245|
| B1/r1 |9/32 (28.1%)|72|0.2425|
| B1/r2 |11/43 (25.6%)|93|0.2235|
| B0/r2 |10/43 (23.3%)|80|0.222|

Aggregated main pickup25/78=32.1% OFF vs20/75=26.7% ON; timeouts162 vs165.
**C-001 acceptance FAIL/unproven, keep B OFF.** Source projection change is not enough
for promotion. Do not compare historical11min A directly with this3min cohort. Ask
Claude to inspect remaining weapon/source projection vs hotspot effects from these
logs before another isolated change. C remains OFF; graph/speed/player/PS unchanged.

## C-002 implemented, test interrupted

Sourcef9cb6599 compiled client/server PASS, frozen build/play-f9cb6599/.
Manual-only wb_revive_z_test:-2(default legacy0.2),0,0.2,-1(native). Native mode samples
GetSpawnLocation up to128 times until requested spawn matches; refuses otherwise,
never borrows another spawn offset. wb_revive_hooks_test0 default, bits1Spawned /
2UpdateAreas (only explicit BOT revive). Automatic respawn unchanged. wb_spawn_box195
uses GetEntitiesInBox +/-2m, logs entity names/classes/proxy masks when available,
physics type/foreign id/pose, capped128. Explicit scheduling:
wb_revive_at after <1..300s> <spawnId> <botNumber>; no delayed human target.

Tools/Test-WarsBotsRevive.ps1 schedules10 manual revives,16s apart (full~12s freeze
windows),170s per mode, captures graph/body diagnostics and contract summary.
First zero-offset run stopped for owner reboot at154.1s:10 native revive commands,
9 complete Triage freeze windows, **9/9 frozen**.10th window incomplete; completed=false,
NOT PASS. Zero offset alone did not resolve observed freezes. Legacy/native modes
and hook mask3 still untested. Raw evidence:build/c002-offsets-f9cb6599/.

## Resume after reboot

Fetch research branch and read Docs/Research/Coordination/MAILBOX/TO_CODEX.md first;
reply with X-NNN in Docs/Coordination/TO_CLAUDE.md and fetch again after every push.
C-001 remains open on behavioral acceptance. C-002: preserve interrupted folder;
run remaining legacy/native with a fresh output folder, then hooks only if useful.
Do not rerun already captured9 complete zero-offset windows as a PASS claim; one
additional controlled window or a full explicit confirmation cohort can be needed.

```powershell
./Tools/Test-WarsBotsRevive.ps1 -BuildDirectory build/play-f9cb6599 -ExpectedSource f9cb6599 -Modes legacy,native -OutputDirectory build/c002-offsets-f9cb6599-resume
```

C-006 was not yet in mailbox at final read; when published, batch wb_nav_explain in
both directions. **No graph changes yet:** no obstacle-height relaxation/jump edges.
Human me/video D-003 needs owner approval; no live session started. Default changes,
self-kill policy, merges and log relocation decisions remain pending owner. No main/
force push or installed EXE modifications. C++ default-OFF build not rerun.
