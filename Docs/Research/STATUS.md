# WarsBots project status — 2026-10-24 19:55 MSK

Single entry point for the owner and all three agents. Evidence types: **src** source · **unit** tests · **server**
server logs · **client** owner in the client / video · **graph** offline analysis. Details live in the journal files.

## Goal

Bots that play Crysis Wars Instant Action like humans **on every IA map** (Steelmill, Quarry, Armada, Outpost; Power
Struggle later): human pace, vertical movement (jumps, Strength jumps), sensible weapon choice (SCAR first), stable
decisions and aim. Map-specific fixes only as data, never code.

## Who does what now

| Agent | Now | Next |
| --- | --- | --- |
| Codex (code, builds, runs) | C-021 sprint pair running (baseline ef25395e vs e5af579d) | C-028 cropped geometry → C-025 demo recording → C-027 refactor wave 1 → C-022 weapons → C-023 aim → C-026 demo playback |
| OpenCode (read-only research + review) | OC-018 demo spec review + playback puppets | OC-019 refactor wave-1 move list; reviews every Codex push (OC-016) |
| Claude (coordination, Triage, verdicts, **engine-free logic modules**, D-010) | status, C-021 verdict when it lands | navmesh comparison on 4 maps when cropped geometry lands; demo metrics on the first demo |

## Done (2026-10-23 … 24)

| Area | Result | Evidence |
| --- | --- | --- |
| Spawn 195 | bots never revive there + generic revive guard (2 s no movement → re-revive elsewhere, max 2); defaults ON (C-014/C-017, f5dc17b); 0 frozen revives since | server |
| Sprint | C-021 accepted, default ON (D-009): sprint 23→37 %, rest of gap = fight time (ATTACKING 40 % of moving) |
| Pace | `wb_bot_pace` **default ON** (D-007, c7d5a21): median 2.49→4.48 m/s, >4 m/s 4→76 %, stalls −40 %, pickups +35 %, no regression; owner: "livelier, stronger" | server + client |
| Claude modules (D-010) | `BotGoalArbiter.h`, `BotJumpLink.h`, `BotPerception.h`, `BotAimModel.h` + tests (branch `warsbots/research/claude-modules`); wiring C-035/C-036/C-037/C-038 | unit |
| Time scale (C-029) | x8 headless changes behaviour (pickups 9 vs 130) → not adopted; real time stays, probes are the speed-up | server |
| **Navmesh offline verdict (D-006 step 1)** | `SteelmillTriage/NAVMESH-VERDICT-2026-10-24.md`: with jump links all spawns connected on Steelmill/Quarry/Outpost (36/36, 32/32, 35/35), Armada 39/40; weapons reachable 95 %; old graph had 13/36 Steelmill spawns outside. Open: Armada deck transitions (owner), stairs vs erosion (real Recast OC-022) | graph |
| Geometry export | `wb_geom_export` (a7553da): Steelmill, Quarry, Armada, Outpost exported (2.3–8.6 M triangles, 0 unsupported parts) — basis of the navmesh | server |
| Navmesh model | Triage `navmesh`: Recast-style walk model + ballistic jump / Strength-jump links + graph metrics + PNG | unit |
| Demos | format `wbdemo/1` (DEMO_FORMAT.md), Triage `demo` reader + metrics (aim wander °/s, reaction, flips, time to primary); log-based 2D replay page | unit |
| Measurement | `summary.json` contract, MOVE TRACE 4 Hz (owner + bots), Triage `pace`, `summary`, `replay`; owner reference pace recorded | server + client |
| Research (OpenCode) | player vs bot movement params, decision-flip root cause, wall-stare root cause, sprint gap, engine geometry API, Detour integration plan, demo engine audit, owner jumps → links, reviews of every Codex push | src |
| Process | mailbox (C-/OC-/X-/O- items), decisions file (D-001…D-007), mail loop for OpenCode, owner questions in chat | — |

## External audit (Grok 4.7, 20:05)

Agreed and adopted (D-008): navmesh right, but links need an executor state and filtering; bots see everyone within
80 m with no view cone or cloak check (`BotCombat.cpp:142–166`, verified) — perception before aim polish; goal arbiter,
not a framework rewrite; probes instead of soaks; `wbdemo/1` had a yaw overflow and lacked INTENT/INV (fixed/OC-021);
summary lacks flips/state time/suit/jump fields. Kept against the audit: C-029 time-scale experiment (audit assumed no
fast simulation).

## Mistakes we made

1. **A day of patching symptoms of a broken navigation approach** (graph repair C-006/C-011, weapon filter B, sink
   escape C, trap watchdog levels). Each patch helped a little, so nobody stopped to ask "does this work on another
   map?". The signals (25 % sinks, 0 edges between the two halves, 90 % no_route) were visible in the morning; the
   coordinator read them as "repair the graph" instead of "replace the approach". → D-006 navmesh.
2. **Spawn 195: a whole day of diagnosis by three agents**; the workaround took one hour. → rule: > 2 h → workaround.
3. **Triage false positive** (frozen revive = first vs last sample) failed C-014 and cost a Codex cycle. → metrics
   get a unit test for the failure they detect *and* for the look-alike (jitter in place).
4. **Unbounded prompts burned Codex's limit** ("keep developing on your own"), and old queued messages would have
   restarted cancelled work. → one task, one push, stop; weekly budget watched (61 % left at 17:16).
5. **Builds during the owner's playtest** (CPU 100 %) looked like "teleporting" bots. → no builds/heavy runs while
   the owner plays.
6. **Mail not picked up**: agents finished a task and waited for the owner instead of reading the mailbox. →
   mail loop (`wait_mail.ps1`), Claude pokes idle agents directly.
7. Mislabelled map parts once (outer ring as "upper level") until the minimap underlay; atlas unreadable to the owner
   at first. → always show data on the real minimap.

## Dead ends (parked or removed)

| Dead end | Why | Fate |
| --- | --- | --- |
| Self-built ray-sampled nav graph + repairs (knee ray, edge audit) | approach cannot represent stairs, railings, ledges, roofs; per-map bugs | replaced by navmesh (D-006); removed in refactor wave 2 |
| Weapon reachability filter B (`wb_reachable_goals`) | half its skips were graph errors | delete with BotNav |
| Sink escape C (`wb_sink_escape`) | no effect | delete |
| Spawn 195 root cause (native revive hooks, z offsets, channel) | 10/10 failures, cause still unknown | parked; workaround is permanent |
| Terrain from `heightmap.dat` | undocumented CryEngine 2 format | replaced by engine geometry export |
| Speech-to-text of owner videos | Whisper/HF blocked in the workspace | owner gives text notes |

## Risks

| Risk | Impact | Mitigation |
| --- | --- | --- |
| Navmesh may not reproduce stairs/catwalks/roofs better than the graph | navigation stays dumb | offline comparison on 4 maps before any switch; owner jumps as off-mesh links; shadow mode first |
| Geometry files are 0.3–1.2 GB OBJ | cannot be staged to Triage (400 MB/file limit) | C-028 cropped/binary export |
| Codex weekly limit (61 % at 17:16, resets 30 Oct; 2 manual resets left) | work stops mid-task | narrow tasks, research/review moved to OpenCode and Claude, no idle wake-ups |
| Flag sprawl and a 361-line `BotBrain::Update` | every change risks regressions; new features (weapons, aim, decision layer) land in the same function | **mandatory refactor** waves 1–2 (below) |
| Demo recording cost on the server | frame drops with 16 actors | acceptance < 0.5 ms/frame in PROFILE, buffered writes |
| Single-map evidence | fixes that only work on Steelmill | acceptance on ≥ 2 maps from the navmesh step on |
| Native spawn-195 freeze cause unknown | could appear on other maps' spawns | generic revive guard logs `REVIVE GUARD` everywhere; Triage `summary` counts frozen revives per spawn |

## Mandatory refactor (owner, 19:45) — behaviour-identical, gated by paired runs + demo metrics

Rule from now: every new flag names its exit (promote / keep as diagnostic / delete) in its C- item; no flag lives
longer than two steps without a decision.

**Wave 1 — C-027, before C-022/C-023 (they change the brain):**
R3 one `BotConfig` struct instead of hot-path `GetCVar("wb_…")` (OpenCode found new ones in pace code) ·
R1 split `BotBrain::Update` (361 lines) into per-state functions, `Change()` the only state writer ·
R4 delete dead experiment flags with their code (`wb_revive_z_test`, `wb_revive_hooks_test`, `wb_progress_experiment`
if unused, `wb_sink_escape`) · R7 one `BotEvent` log helper (shared with demo EVENTs) · R6 one formatter commit.
Acceptance: unit tests pass, one paired run with `summary` and demo metrics unchanged within noise.

**Wave 2 — after the navmesh switch:** delete BotNav graph build/knee audit/reachability/goal projection/most watchdog
levels (list: OpenCode DETOUR_INTEGRATION.md §5), R2 `BotConsole.cpp`, R5 legacy single-bot commands, R8 one revive path.

## Queue (in order) — re-planned after the external audit (D-008, 20:08)

1. C-028 cropped geometry → **offline navmesh verdict on 4 maps** (Claude `navmesh` + OpenCode real Recast bake OC-022):
   spawn → weapon reachability, catwalks survive erosion, 0 links longer than ballistics, owner-route coverage.
   The old graph is **not** the oracle. 2. C-029 time-scale experiment. 3. `wbdemo/2` freeze (OC-021) → C-025 demo
   recording. 4. C-027 refactor wave 1 = R1 + R3 + R4. 5. C-031 probe harness (30–90 s scenarios on 4 maps).
6. C-032 goal arbiter. 7. C-033 look along travel. 8. C-022 weapon rank. 9. Detour behind a flag + jump-link executor
   state → switch on Steelmill, then a second map. 10. Perception: view cone, cloak, last-known position.
11. Aim model (saccade + filtered noise). 12. One suit policy. 13. C-026 in-game demo playback. Later: refactor wave 2,
    Power Struggle, ML (not this quarter; revisit if C-029 gives fast simulation).

## Rules

Codex owns game code integration, builds, servers. Claude writes only new self-contained modules with unit tests in
`warsbots/research/claude-modules` (D-010: arbiter, link executor, perception, aim model, demo serializer) and never
edits existing Codex files; OpenCode never edits game code. Default OFF until paired runs show no
regression; behaviour defaults need the owner (D-002). > 2 h diagnosis → workaround + parked ticket. One task, one
push, stop. No builds while the owner plays. Questions to the owner go into chat.

## Where things are

| What | Where |
| --- | --- |
| Roles, loop | `Coordination/HANDOFF.md` |
| Mailbox | `Coordination/MAILBOX/` (`TO_CODEX.md`, `TO_OPENCODE.md`, `DECISIONS.md`, `PROTOCOL.md`); replies: weapon-goals `Docs/Coordination/TO_CLAUDE.md`, movement-params `Docs/Coordination/OPENCODE_TO_CLAUDE.md` |
| Tools | `Tools/SteelmillTriage`: `summary`, `pace`, `navmesh`, `demo`, `replay`, `analyze`, `graph`, `movement`, `mem`, `level`, `episodes`; `Tools/Mailbox` |
| Specs | `SteelmillTriage/SUMMARY_CONTRACT.md`, `SteelmillTriage/DEMO_FORMAT.md`, OpenCode `MovementParams/*.md` |
| Owner pages | Steelmill Nav Atlas, Steelmill Replay (private artifacts) |

## Journal (newest first)

| Date | File | What |
| --- | --- | --- |
| 10-24 19:50 | `SteelmillTriage/DEMO_FORMAT.md` | in-game demo format |
| 10-24 19:25 | owner playtest OwnerPace7 | pace ON feels right; aim wander; SCAR first; "teleports" = CPU |
| 10-24 19:20 | C-020 in TO_CODEX.md | pace verdict (4 paired runs) |
| 10-24 18:19 | DECISIONS D-006 | navmesh for all maps |
| 10-24 14:20 | `SteelmillTriage/OWNER-VIDEO-2026-10-24.md` | owner reference video |
| 10-24 13:00 | `SteelmillTriage/REVIEW-2026-10-24-1300-codex-ABCD.md` | Codex A–D review |
| 10-24 05:15 | `SteelmillTriage/PLAYTEST-2026-10-24-0458.md` | first real graph, spawn 195 |
| 10-24 03:00 | `SteelmillTriage/FINDINGS-2026-10-24.md` | F1–F5 from old logs |
