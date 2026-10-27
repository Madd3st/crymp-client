# Claude coordinator state (27.10 21:20 MSK)

**D-031 ACTIVE since 27.10 21:05** (Claude weekly limit ~3 %). Codex = interim coordinator + integrator
(SUCCESSOR.md prompt A), DeepSeek = reviewer of every Codex code push (prompt C). Claude's hourly cloud task
(trig_01ABKXcowTTiHUd7wwLt9Lxz, minute 19) must be OFF; if it still fires, it reads this file and exits.

Last processed by Claude: X-191, DS-054. Last processed by Codex coordinator: X-192 (C139 verdict FAIL), DS-054 patch handoff.

claude-sprint head **ba4d5f4e** (C-139: PS buy also alive at spawn, first 20 s, every 2 s; buy zone checked by stock Lua)
on 96f7f624 (DS-052 doors/vehicles/events, all default OFF) on 815bcb7e (C-136 PS team spawn + owner buy rule)
on 1a7ec84e (C-134) on 493f2cab (C-131 DS-050 fundament) on 4f3a1d72 (CI green).

Results since 20:00:
- C-137 (X-186, 815bcb7e): spawns 12/12 own base PASS; buys 0/6 FAIL — buying ran only while dead, nobody died in 300 s → fixed in ba4d5f4e.
- C-126 (X-188, 493f2cab): sniper probes DSG1 0/3, Gauss 0/3 FAIL (missed landings, no energy) → snipers PARKED until Shore navmesh/IA cleanup.
- C-138 (X-190, 96f7f624): FAIL — doors go through stock `Door:OnUsed` → client RMI `SvRequestOpen` ("RMI via client… not a client"); traps 1→2, stalls 59→73; vehicles/PS EVENT not exercised.
- DS-053 door hunk WRONG (same OnUsed path) → do not integrate. DS-054 (6a74b8e5, base ba4d5f4e) = fix: server `Door:Open(user, 1)`;
  two patches `ds054_doors.patch` + `ds053_probe_setup.patch` (probe `setup` array, `wb_probe_run <id> [seconds]`), both apply clean, units pass, Lua not run.

Open:
- Codex: **C-139 STARTED 21:09** (Shore 6x300 on ba4d5f4e, gates: ≥1 buy per bot incl. laser, base spawns 100 %, 0 Lua/channel/crash, traps ≤ 1). C-126 rest blocked (needs ds053_probe_setup), C-129 soak, C-123/C-127/C-128/C-130 bookkeeping.
- DeepSeek: DS-051 (delete BotNav fallback) waits for the Shore navmesh. DS-054 done, not integrated.
- Owner: D-032 (`wb_bot_first_primary` default ON) open.

Next for the interim coordinator (Codex), in order:
1. C-139 verdict.
2. Integrate DS-054 into claude-sprint (`ds054_doors.patch`, then `ds053_probe_setup.patch`; NOT ds053.patch door hunk), CI green, DeepSeek review, Shore 6x300 with `wb_ps_doors 1`: 0 RMI/Lua errors, ≥1 `PS DOOR open`, NK bots leave base ≤ 20 s.
3. C-126 remaining steps with the native probe runner (setup array now parsed).
4. Shore navmesh bake (Tools/RecastBake, release warsbots-ps-data-a943e070) → DS-051.
5. If the owner says yes to D-032: flip only `wb_bot_first_primary` default 1.

Handback: Claude reads this file + the coordinator summary below + `git log warsbots/feature/claude-sprint ba4d5f4e..`.

## Coordinator log (Codex appends below, newest first)

2026-10-27 22:38 MSK: D031 takeover active by direct owner request. Weekly remaining45% (guard20% not reached). C139 DONE FAIL ba4d5f4e/X192: actualprimary6/6 per spawnset, reportedok0/LAM0; no repeat. Current codehead ba4d5f4e CI40555474395 green. C-140 STARTED DS054 door/probe integration, then CI+independent exactSHAreview before run. DS-055 queued buy-result/laser analysis + promptC duty. Waiting on: offline checks/new CI/review; not owner. D032 remains open, no promotion. Local old sprint branch19989a6d diverged, preserved; detached worktrees use remote exact code/docs heads; main QA dirty files preserved.
