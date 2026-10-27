# Claude coordinator state (read and updated by every scheduled Claude run)

Last processed: X-185 (Codex, ACK C-137), X-184 (C-131 pair accepted: no regression, one pair), DS-052 (integrated 96f7f624 with gating fixes). DS-051 PARTIAL (99605013 — NOT integrated: removes BotNav runtime path, Shore has no navmesh yet).
claude-sprint head 96f7f624 (DS-052 doors/vehicles/events, all default OFF: wb_ps_doors, wb_ps_vehicles, wb_ps_events) on 815bcb7e (C-136 PS initial spawn + owner buy rule) on 1a7ec84e (C-131 DS-050) on 4f3a1d72 (CI GREEN 40526830694).
Open Codex: C-137 (STARTED 20:19, PS spawn+buy retest 815bcb7e, CI 40546394540 green), C-138 (DS-052 smoke on 96f7f624, after C-137), C-126 rest, C-127/C-128/C-129/C-123/C-130 bookkeeping.
Open DeepSeek: DS-053 (door toggle -> open-only; ParseScenario setup array / wb_probe_run args), DS-051.
Owner: D-032 (first_primary default) open; D-033 YES -> UPSTREAM_READINESS.md is the plan.
Waiting on: C-137 result (X-), DS-053. Closed this run: C-131, C-132, C-133 done; C-135 superseded by C-137.
Next for Claude: C-137 verdict; DS-053 integration; Shore navmesh bake before DS-051; if D-032 yes -> flip wb_bot_first_primary default 1 (only that flag).

Claude hourly cloud scheduled task: ACTIVE (trig_01ABKXcowTTiHUd7wwLt9Lxz, minute 19 every hour, owner OK 27.10 15:18).
D-031: successor plan approved but DORMANT — Codex/DeepSeek switch to SUCCESSOR.md prompts only when the owner pastes them.

Shore: no navmesh anywhere (only release warsbots-ps-data-a943e070 geometry). Claude bakes Shore with Tools/RecastBake/recast_bake.cpp (triage) before DS-051 goes in.
PS plan: Docs/Research/PS_PLAN.md (stages 0-5). Owner questions in §8 pending.
Codex limit ran out 19:56 (resets 20:09): mailbox files 137+177 KB re-read per step -> TO_CODEX/TO_DEEPSEEK archived, C-137 budget rules.
