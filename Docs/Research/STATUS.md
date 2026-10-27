# WarsBots project status — 2026-10-27 21:20 MSK

Single entry point for the owner and all three agents. Evidence types: **src** source · **unit** tests · **server**
server logs · **client** owner in the client / video · **graph** offline analysis. Details live in the journal files.

## Goal

Bots that play **Crysis (2007) multiplayer via CryMP** Instant Action like humans **on every IA map** (Steelmill, Quarry, Armada, Outpost; Power
Struggle later): human pace, vertical movement (jumps, Strength jumps), sensible weapon choice (SCAR first), stable
decisions and aim. Map-specific fixes only as data, never code.

## Who does what now

**D-031 ACTIVE since 27.10 21:05** (Claude weekly limit ~3 %): Codex = interim coordinator + integrator, DeepSeek
reviews every Codex code push. Prompts and current plan: `Docs/Research/Coordination/SUCCESSOR.md`; live state:
`MAILBOX/CLAUDE_STATE.md`.

| Agent | Now | Next |
| --- | --- | --- |
| Codex (interim coordinator + QA) | C145 Shore candidate baked, rejected pending water safety and routes; revised tool eed85fb1 ten-case PASS. Game2e0a6641 reviewed CI GREEN | DS071 tool review; coherent native swim/door/search package; US subdeck routes before game QA |
| DeepSeek (analysis, patches, reviewer) | DS068 BLOCK fixed, DS071 new tool review; DS067d door candidate syntaxPASS | DS070 gated swim/native oxygen; DS069 US subdeck and actual capture Area routes |
| Claude | out of limits; handback via CLAUDE_STATE.md | — |

## 27.10 afternoon → evening (key results)

| Area | Result | Evidence |
| --- | --- | --- |
| 15-bot with 4 new flags (d6a9a521) | 287 kills / 32 stalls / 0 traps PASS; first primary p50 5.7 s, SOCOM shot share 6.6 %; withdraw deaths 69 % (no gain); DSG1/Gauss kills 0 | server (X-167) |
| PS revive | **works**: wave revives confirmed (same actors back at 100 HP after ~17 s); "no revive" was a demo recorder gap | server (X-166) |
| Fundament (D-033) | DS-050 integrated behaviour-identical (IBotGameMode, flag delete, PS summary, native send guard); Steelmill pair kills 39→44, stalls 5→5 | server (X-184) |
| Owner Shore playtest (4f3a1d72) | stable; bots spawned on neutral bunkers, bought nothing (Lua error powerstrugglebuying:984), never left base (doors), no vehicles | client + server (X-181) |
| PS spawn | C-134/C-136: initial and revive spawn in the team group → **12/12 own base PASS** | server (X-186) |
| PS buy | C-136 buy = shotgun/SMG + LAM + ammo via DoBuyAmmo; 0/6 buys because buying ran only while dead → ba4d5f4e buys alive at spawn, C-139 running | server (X-186) |
| PS doors | DS-052 used stock `Door:OnUsed` = client RMI → "RMI via client… not a client", traps 1→2 (C-138 FAIL); DS-053 same mistake; **DS-054 fix: server `Door:Open(user,1)`**, not integrated yet | server (X-190) + src |
| Snipers | probes DSG1 0/3, Gauss 0/3 (missed landings, no energy) → parked until Shore navmesh/IA cleanup | server (X-188) |
| Probe runner | native ParseScenario ignored the JSON `setup` array; fix in `ds053_probe_setup.patch` (DS-054), not integrated | src |
| Process | Codex budget rules (archive mailboxes, X- ≤ 6 lines, one instance); D-031 activated 21:05 | — |

## 26.10 day → 27.10 (key results)

| Area | Result | Evidence |
| --- | --- | --- |
| Night batches 26→27 (15×660) | 2fee59c2 302 kills/27 stalls; fd12a5eb (+pro_fire, high_ground, threat_scan) 287/29 PASS; 0fa9c1cb (+no sprint near a visible enemy) 293/30 PASS, Elite no-fire 17.1 → 9.5 /min, first shot B/N/V/E 1.02/.61/.38/.23 s; bot AI 0.12 ms/frame | server (X-156) |
| Owner playtest 5 (6e2713f9) | skills finally distinct: K/D B 10/34, N 30/26, V 18/26, E 41/23 (V < N = per-bot noise, 2 bots/skill, DS-046); owner 13/3 | client + server (X-164) |
| state_commit 2 | FAIL: A→B→A 2.69 → 15.25 /bot-min, 160 STATE CONFLICT → stays 1 | server (X-162) |
| DS-044 flags (4d399cf4, 6×300 A/B) | first_primary + withdraw_cover + finish_lock: kills 35 → 38, SOCOM shot share 14.7 → 2.5 %, withdraw deaths 68 → 50 %, finish_lock never triggered (no case) | server (X-165) |
| Power Struggle PS-0 | crash at bot 2 (C-117) fixed; DS-045 guards every per-client RMI by bot identity → 0 RMI/Lua errors, 6/6 bots, no crash; **bots never revive** → diag in d6a9a521, DS-047 | server (X-161, X-165) |
| Snipers | never fetched DSG1/Gauss: pickup search box ±12 m, DSG1 at +20.3 m → never chosen; fix `wb_bot_sniper_reach` (DS-046), links above Strength rise 3.72 m banned | src + graph |
| DeepSeek audit (DS-042) | Bible 2 PASS / 18 PARTIAL / 15 FAIL; 7 exploit probes; probe runner header (not wired) | graph |
| Process | sync v4 (D-030): all tasks in the mailbox, wake loop fixed; runs any time except while the owner plays (D-029) | — |

## Today 25.10 evening → 26.10 night (key results)

| Area | Result | Evidence |
| --- | --- | --- |
| **Owner playtest 2 (23:41, b521feeb, corners+sanity ON)** | owner: "more or less playable", fewer stucks, lasers used. Found: skills indistinguishable (speed 4.4–4.6, aim median 0.9–2.2°), no bot hit > 40 m (p90 22 m vs owner 31 m), 24 low-HP target switches, aim looks late/crooked in spectator 1st person (server error ~1° → replication suspect), position snaps rare (27/3345 > 0.5 m). Owner movement: roofs via Strength + wall jump chains, sniping from height, speed bunny hop 9–13 m/s. Report: Claude Doc «разбор плейтеста 2» | client + server |
| **Stuck bots fixed (candidate)** | `wb_bot_corners` v2.1 + `wb_bot_jump_sanity`: two independent 15×660 runs — kills 280/282 vs 238/243, pickups 260–291 vs 223/238, stalls 18/18 vs 71/47, Strength jumps 72 vs 217, stair 1306,1353 and spawn-176 traps 0. **D-016 (default ON) waits for the owner.** | server (X-072, X-075) |
| Why v1/v2 failed | v1 (C-071) cut turns; v2 (C-074/C-076): BotMovement stops 0.8 m short while corners waited for 0.35 m → bots stood still; level body rays read stair steps as walls. v2.1: corner mode owns arrival (0.2 m), probe clamped to the corner, rays follow the stair slope | server + graph |
| Owner playtest 19:48 | report tool: 364 Strength jumps (58 on links; 157 random travel hop, 149 obstacle rays on stairs); 0/8 bots ever held DSG1/Gauss (bot #3 tried, marked unreachable after 8 s); aim median 2.4°, p95 ~90° per 10 s window; 25–40 % time with SOCOM | client + server |
| Tools (D-015 pyramid) | `Tools/OfflineCheck` (unit tests + clang compile check, fail-closed), `walk_sim` (capsule + BotMovement model, reproduces 716's failure), `Tools/PlaytestReport` (HTML report with video timestamps) | graph |
| PS geometry | exporter: PowerStruggle gate (813c43c1), PS structure bodies (17211645), 64 m tiled physics query (faff5ac7, bunkers are on-demand static brushes), play-area box (a943e070, X-076 hit the 15 M cap) | server |
| Shore 3D | artifact v3: terrain + collision geometry + PS objects; bunkers missing until C-082 | graph |
| Process | D-015 verification pyramid; external architecture review prompt (`Coordination/EXTERNAL_REVIEW_PROMPT_2026-10-25.md`) | — |

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

11. **Integrated a patch without checking the call path (27.10)**: DS-052 doors went through stock `Door:OnUsed`,
    which is the client RMI path, and C-138 failed. → for every stock Lua call from a bot, check it is a server method
    (not `self.server:Sv…`) before integrating.
8. **Tasks sent only via chat (27.10)**: Claude gave the owner prompts to paste instead of mailbox items, so the wake
   loop had nothing to wake on and the owner had to nudge all three. → every task is a C-/DS- item (PROTOCOL v4).
9. **Wake loop never fired at night**: `wake_agents.ps1` required the word "open" in headers that never had it. →
   any newer item counts; a fail-closed check of a scheduler needs one real (not dry-run) wake.
10. **Promised work not done**: Claude said "integrating DS-044 now" and did not, Codex waited 1.5 h on a SHA. →
    state what was done, not what will be.

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
| Claude runs out of limits | coordination and code integration stop | `SUCCESSOR.md`: Codex interim coordinator, DeepSeek reviewer; D-031 |
| PS bots cannot leave base / no vehicles | Power Struggle unplayable | DS-054 doors via server Door:Open; vehicles (DS-052) untested; Shore navmesh |
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

## Queue (27.10 21:20, in order — run by Codex as interim coordinator)

1. C-139 verdict (PS buy at spawn, ba4d5f4e). 2. Integrate DS-054 (`ds054_doors.patch`, then `ds053_probe_setup.patch`;
not the ds053.patch door hunk), CI green, DeepSeek review, Shore 6x300 `wb_ps_doors 1`. 3. C-126 rest with the native
probe runner. 4. Shore navmesh bake (Tools/RecastBake, release warsbots-ps-data-a943e070) → DS-051 (delete BotNav
fallback). 5. PS plan stage 1–2 (doors, vehicles): `Docs/Research/PS_PLAN.md`. 6. Owner D-032 (first_primary default).
7. Soak C-129 when no PS work runs. Out of scope while Claude is out: refactors, boats, grenades, capture goals, new subsystems.

## Rules

Claude owns game code (`warsbots/feature/claude-sprint`) and integrates DeepSeek patches; Codex runs CI artifacts,
batches, probes, presets and archives (no local builds); DeepSeek writes analysis and `git apply`-able patches in its
own branch. Every behaviour change behind a default-OFF `wb_*` flag; defaults need the owner (D-002). Offline green
before push. Every task is a mailbox item (v4). Runs any time except while the owner plays (D-029). > 2 h diagnosis →
workaround + parked ticket. One task, one push, stop. Questions to the owner: D- rows + chat.

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
| 10-27 21:20 | `MAILBOX/CLAUDE_STATE.md` | handover refresh: D-031 active, Codex coordinates, plan in SUCCESSOR.md |
| 10-27 21:04 | DS-054 | doors via server Door:Open + probe setup patch (base ba4d5f4e) |
| 10-27 20:57 | C-139 | ba4d5f4e: PS buy alive at spawn |
| 10-27 20:45 | X-190 | C-138 FAIL: doors = client RMI |
| 10-27 20:28 | X-186 / X-188 | PS spawns 12/12 PASS, buys 0; snipers 0/3 parked |
| 10-27 20:00 | C-136 | 815bcb7e: PS team spawn + owner buy rule |
| 10-27 18:06 | C-131 | 493f2cab: DS-050 fundament (IBotGameMode, flags, PS summary) |
| 10-27 17:52 | D-033 | upstream readiness plan `UPSTREAM_READINESS.md` |
| 10-27 15:30 | `Coordination/SUCCESSOR.md` | handover prompts if Claude is out of limits (D-031) |
| 10-27 15:10 | PROTOCOL v4, C-122 | sync: mailbox-only tasks, wake loop fixed (deepseek, any new item) |
| 10-27 14:50 | C-121 / DS-047 | d6a9a521: DS-046 sniper reach + PS revive diag |
| 10-27 14:15 | X-165 | 4d399cf4: PS 0 RMI errors but no revives; 3-flag A/B |
| 10-27 12:57 | C-120 | 4d399cf4: DS-044 + DS-045 integrated |
| 10-27 11:38 | X-164 | playtest 5 archive, skills distinct |
| 10-27 10:44 | X-162 | state_commit 2 FAIL |
| 10-25 15:45 | C-066 | sniper spots: tactical_spots.cpp (cover/low/sniper/exposed per map), BotVantage + wb_bot_vantage; 3D view of Steelmill (Gauss tower 13 m, 4 Strength jumps; DSG1 22 m, stairs) |
| 10-25 15:25 | C-065 | owner playtest 717853ff: "kills without ammo" = spectator HUD (server ammo fine) → ammo synced; laser+zoom (wb_bot_weapon_handling), names with skill/style, raw client aim while firing, Hurricane lower |
| 10-25 14:35 | C-063 | 8214f16a: links 27 % → 49 % done, but nav fallbacks ×6 (weak parts) → SCC + directed reachability, run-up before link jumps (717853ff) |
| 10-25 13:45 | C-062 | **C-061 GREEN, 0470fe7e merged**: kills 219 → 281, SCAR kills 4 → 80, pickups 112 → 208, flips 16.6 → 9.2 |
| 10-25 13:10 | C-059 | 1e343d81 client lag fix #1 (O-025): client cvar `wb_bot_client_smooth` (default OFF) filters replicated direction/aim steps; added to the C-058 batch; roles table per D-013 |
| 10-25 11:45 | C-056 | new: BotSuitPolicy module, VisibleLookAhead (C-055 fix), meshes with jump/lift links (Steelmill items 38 -> 93), Triage still/off-travel metrics |
| 10-25 03:20 | C-055 | 15-bot pair all-7 OFF/ON: SOCOM kills 60 % -> 25 %, pickups +50 %, flips -52 %, kills equal; regression: wall stares x10 (look-ahead behind wall corners) + standing 3.4 -> 8.4 %; fix specified |
| 10-25 02:25 | C-054 / X-048 | turn cap fixed by Codex (52df15c1): view snaps >720 deg/s 10.5 % -> 0.9 %, p95 1872 -> 226 deg/s; next all-7 smoke + 15-bot pair |
| 10-25 01:45 | C-053 / X-046 / X-047 | time scale ×2/×4 not adopted (pickups 134/92/28); sprint batch builds after one macro fix, 6 module tests PASS, meshes installed; QA running |
| 10-25 01:15 | C-052 | C-027 refactor accepted (C-043 pair within noise: kills 240/236, hits 3529/3537); b902dfb4 = baseline |
| 10-25 01:10 | D-012 / C-051 | back to old roles: Codex owns game code; claude-sprint ae798245 handed over to Codex for review/build/integration |
| 10-25 01:15 | C-050 BATCH READY claude-sprint ae798245 | Claude wrote sprint code (D-011): 7 default-OFF gates — look, turn cap, weapon rank, arbiter, **Detour navmesh (P0)**, perception, aim model; awaiting Codex build + smoke |
| 10-25 00:40 | C-047 + `Coordination/SPRINT-2026-10-25.md` | owner's final implementation sprint handed to Codex (P0 movement, P1 arbiter/look/weapon, P2 perception/aim/jumps); C-043/C-044/C-046 folded in |
| 10-25 00:50 | claude-modules a040dffc | new modules: BotLookPolicy (+PathLookAhead), BotWeaponRank, BotMoverLink (lifts); 7 modules, all tests pass; C-045/C-046 (x2/x4 test) mailed |
| 10-25 00:15 | X-042 / C-043 | C-027 refactor done (R1+R3+R4, 60 s smoke OK); paired equivalence run requested before C-031 |
| 10-25 00:05 | `RECAST-BAKE-2026-10-24.md` v3 | lifts/ladders from level .cry: Armada 3 elevators + 2 ladders → 37/40 spawns (r 0.3), items 151/156; C-041 cancelled |
| 10-24 23:55 | C-042 in TO_CODEX.md | owner playtest 23:33 (demo + video): bots look > 90° off travel 32 % of moving time; wall stares = looking at the aim point through walls (SEARCHING_ENEMY); `look_walls.py` |
| 10-24 22:40 | `SteelmillTriage/RECAST-BAKE-2026-10-24.md` v2 | finer bake (cs 0.05, r 0.35): Steelmill 36/36 on foot, Quarry 32/32, Outpost 35/35, Armada 34/40 with jumps; v1 gaps were erosion, not ladders/doors; Detour step: cs 0.1 r 0.3 |
| 10-24 22:10 | `SteelmillTriage/RECAST-BAKE-2026-10-24.md` v1 | real Recast bake 4 maps: walk-only far better than walk model (Steelmill items 9 → 53); Strength links with real clearance: Steelmill 32/36, Quarry 32/32, Outpost 34/35, **Armada 6/40**; gaps = ladders → C-041 |
| 10-24 21:30 | C-040 in TO_CODEX.md | first wbdemo/2 read OK; baseline: SOCOM 61 % of kills, 14.9 decision flips/bot-min, reaction 0.8 s |
| 10-24 19:50 | `SteelmillTriage/DEMO_FORMAT.md` | in-game demo format |
| 10-24 19:25 | owner playtest OwnerPace7 | pace ON feels right; aim wander; SCAR first; "teleports" = CPU |
| 10-24 19:20 | C-020 in TO_CODEX.md | pace verdict (4 paired runs) |
| 10-24 18:19 | DECISIONS D-006 | navmesh for all maps |
| 10-24 14:20 | `SteelmillTriage/OWNER-VIDEO-2026-10-24.md` | owner reference video |
| 10-24 13:00 | `SteelmillTriage/REVIEW-2026-10-24-1300-codex-ABCD.md` | Codex A–D review |
| 10-24 05:15 | `SteelmillTriage/PLAYTEST-2026-10-24-0458.md` | first real graph, spawn 195 |
| 10-24 03:00 | `SteelmillTriage/FINDINGS-2026-10-24.md` | F1–F5 from old logs |
