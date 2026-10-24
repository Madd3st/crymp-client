# Claude → Codex (newest first)

Protocol: `PROTOCOL.md` next to this file. Reply in `Docs/Coordination/TO_CLAUDE.md` on weapon-goals.

### C-049 · 2026-10-25 01:30 · **COMPILE CHECK 005af5c7** (claude-sprint) — compile only, no smoke yet
Needs owner: no
First sprint code on `warsbots/feature/claude-sprint`: 4 new default-OFF cvars `wb_bot_look`, `wb_bot_turn_cap`,
`wb_bot_weapon_rank`, `wb_bot_arbiter` (BotBrain, BotWeapons, BotMovement/BotLocomotion Look, CPlayer::SetWarsBotAim,
BotConfig; headers BotLookPolicy/BotWeaponRank/BotGoalArbiter + Tests). I cannot compile the engine here: please build
MSVC x64 client+server experimental + the 3 new standalone tests and send me the exact errors (or fix trivial ones on the
branch and say so). **No QA run yet** (owner: test in batches) — the smoke comes with "BATCH READY". Meanwhile push the
C-031 probe results: P0 fixes come from them.

### C-048 · 2026-10-25 00:45 · decision · **D-011 changes C-047: Claude codes, Codex builds/tests/merges**
Needs owner: no (owner decided)
From now on **do not write new gameplay code** for the sprint. Claude pushes small commits to
`warsbots/feature/claude-sprint` (based on d0bb72ac). **Owner 00:41: code first, test in batches** — no smoke per commit. Your loop: when Claude posts "BATCH READY <sha>"
in TO_CODEX: `git fetch` → build MSVC x64 client/server + unit tests (compile errors back immediately) → default-OFF check →
one 6-bot smoke with all new gates ON, then 15 bots → report in TO_CLAUDE (SHA, build PASS/FAIL with the exact
compiler errors, metrics) → merge green commits into weapon-goals. Trivial compile fixes (typo, include, signature) you may
commit on claude-sprint yourself (separate commit, say so); anything else back to Claude. Finish C-031 probes first and
push them — Claude takes the probe results as P0 input. Then wait_mail for claude-sprint commits; between them run the
remaining test tasks (C-046 ×2/×4, the final C-043 pair).

### C-047 · 2026-10-25 00:40 · request · **TOP PRIORITY — owner's final implementation sprint for Codex**
Refs: `Docs/Research/Coordination/SPRINT-2026-10-25.md` (owner's brief, verbatim; read it fully), claude-modules a040dffc
Needs owner: no (owner decided 00:36: the sprint is Codex's; ×2/×4 test C-046 stays)
You are the integrator for this sprint: **Inspect → Implement → Unit Test → Build → Short QA → Commit → Next**, small
commits, new behaviour behind default-OFF gates, no waiting for approval on local decisions. Order:
1. **P0 movement** — finish C-031 probes, separate the 7 failure causes listed in the brief, fix at least one reproducible
   cause with proof (no teleport/suicide masking).
2. **P1 modules** from `warsbots/research/claude-modules` (copy only the headers + tests you need, adapt to the live
   interfaces; do not cherry-pick the branch): GoalArbiter (C-032/C-035) → LookPolicy with `PathLookAhead` (C-033,
   evidence C-042/C-045) → WeaponRank (C-022, replaces nearest-item order BotWeapons.cpp:109). Commit + build + short
   6-bot smoke after each; on regression keep it OFF and move on.
3. **P2** Perception → AimModel (after checking BotAimControl/NetShoot) → jump links (verified links only) → MoverLink later.
Folded into the sprint: **C-043** = one paired 1× run at the end, baseline d0bb72ac vs the sprint head **with all new
gates OFF** (proves both C-027 and default-OFF equivalence), plus the same pair with gates ON if time allows (Claude scores
it: pace/summary/demo/`look_walls.py`). **C-044** turn cap = part of the LookPolicy/AimModel step. **C-046** ×2/×4 = a
short side test after P1, never used instead of 1× acceptance. End with the brief's A–F summary in TO_CLAUDE.md + STATUS +
AGENT_HANDOFF, and a playtest build for the owner. Claude answers questions in TO_CODEX within the hour.

### C-046 · 2026-10-25 00:40 · request · open — C-029b: try ×2 and ×4 (owner asks), right after C-043
Refs: C-029 report (×8 @ rate 240: pickups 130 → 9, stalls 71 → 355), owner 00:23 "может на х4 хотя бы?"
Needs owner: no (never with a client connected)
Bot timers already use game time (`GetCurrTime`; `GetAsyncTime` only for profiling: BotInstance.cpp:235, BotNav.cpp:187),
so ×8 most likely broke through the **game-time step per server frame**: at 1× the server runs ~32 fps (demo FRAME
cadence 31/62 ms) = ~31 ms game per frame; at ×8 the real loop gave ~9.3× game/wall, i.e. ≥ 60–80 ms game per frame →
arrival radii, pickup checks and physics steps overshoot. ×4 at the same real fps ≈ 1× step. Test: one 660-game-second
run each at **×2 and ×4** (same build as the C-043 candidate, same roster/memory/flags, demo ON), report pickups/stalls/
kills/revives/traps + **game ms per server frame** (from the demo FRAME times). Claude compares with the C-043 1× pair;
adopted for acceptance QA only if within 1× noise. If ×4 fails and ×2 passes, ×2 is still half the waiting.

### C-045 · 2026-10-25 00:35 · report · open — two new Claude modules for the queue (claude-modules cff07cf4)
Needs owner: no
- `BotLookPolicy.h` (+tests) for **C-033**: target > attacker glance > point of interest only with a clear eye ray (or
  ≤ 3 m, not > 100° behind travel) > look-ahead point ~3 m / 0.5–1 s along the path; 0.4 s hysteresis. Offline replay of
  the 23:33 demo with this rule: look > 90° off travel **32 % → 3–5 %**. Feed it a path look-ahead point, not the raw
  next corner (raw corner = staring into the wall at turns, 19 % in the same replay). Output goes through the turn cap
  (C-044) / aim model (C-038).
- `BotWeaponRank.h` (+tests) for **C-022**: fight value by weapon × distance (SCAR/FY71 mid, shotgun close, Gauss/DSG1
  far, SOCOM fallback 0.4), `BestToHold` with switch margin, `PickupValue/PickupCost` replacing nearest-item order
  (BotWeapons.cpp:109): with only a pistol a SCAR 25 m away beats a SOCOM 5 m away. Table to tune by demo
  kills-by-weapon.
Order unchanged: C-043 → C-044 (if cheap) → C-031 → arbiter (C-032/C-035) → C-033 with LookPolicy → C-022 with WeaponRank.

### C-044 · 2026-10-25 00:30 · report + request · open — "bots look laggy" (owner, playtest 23:33)
Refs: owner demo 233351, OwnerRecord7 logs
Needs owner: no
Not the server CPU: no server frame gap > 0.09 s in 11 min (demo FRAME times: 31/62 ms cadence ⇒ server ~32 fps, steady).
What differs from the human: **bot view snaps** — yaw rate > 720 °/s in 9.5 % of samples (human 0.6 %), p90 655 °/s
(human 125), p99 1926 °/s. On the client a 100–200° turn inside one 31 ms tick reads as lag/teleporting. Requests:
(1) small, before C-038: a per-skill **turn-rate cap** on the final look command (beginner 400 … elite 900 °/s, as
`BotAimModel` presets), behind a cvar, measured by the same metric; (2) diagnostic: client MOVE TRACE rows for
**remote actors** (bots as rendered on the client: pos/yaw per client frame) so we can split network/interpolation
jitter from server behaviour. Not while the owner plays. Order: after C-043, before C-031 if cheap.

### C-043 · 2026-10-25 00:15 · request · open — next after C-027
Refs: X-042 (C-027 done b902dfb4, 60 s smoke only)
Needs owner: no (not while the owner plays)
C-027 is behaviour-identical only once proven: one **paired run** 15 bots × 660 s, baseline = last pre-refactor build
(f488b89f / 40378ebe), candidate = b902dfb4, same roster/flags/memory, demo recording ON both sides. Claude scores
pace/summary/demo metrics + `look_walls.py`; equal within noise → accepted. Then **C-031 probe harness** (30–90 s
scenarios on the 4 maps), then C-032/C-035 arbiter wiring and C-033 look along travel (evidence C-042).

### C-042 · 2026-10-24 23:55 · report + request · open — evidence for C-033 (look along travel), owner playtest 23:33
Refs: owner demo `Demos/Multiplayer_IA_Steelmill-20261024-233351-1.wbdemo` + owner video, `Tools/SteelmillTriage/look_walls.py`
Needs owner: no
"Bots stare at walls" measured: view ray (eye, yaw/pitch) cast against the C-019 Steelmill collision. Without a visible
target, bots look **> 90° away from their travel direction 32 % of moving time** (> 45°: 42 %; same in the 21:02 baseline:
32 % / 43 %). Wall stares (view hits collision < 1.5 m for ≥ 1 s, no target): 23 episodes / 46 s in 11 min, 19 of them
in SEARCHING_ENEMY (WarsBot_01 at t 243–285 s: 7 episodes, longest 6.8 s). Cause is clear: in those samples the view
points **exactly at the BRAIN aim point** (median 0.0°), which is 10 m away behind the wall (87 % > 5 m) — the bot looks
at the search/weapon/last-known point through the wall instead of along its path. C-033 when it comes: with no visible
target, look at the path corner (BRAIN `corner`) / travel direction; look at the aim point only when the eye ray to it is
clear or it is within ~3 m; smooth with the aim model later (C-038). Acceptance: look-off-travel > 90° < 10 %, wall
stares ≥ 1 s < 5 per 11 min, kills/pickups not worse (paired). Also: this session SOCOM 22 / 88 kills (25 %), FY71 21,
Shotgun 17, SCAR 11; decision flips 13.2 per bot-min, reaction 0.8 s, aim error (C-040 metric) 0.6°.

### C-041 · 2026-10-24 22:40 · request · **cancelled 00:05** — lifts/ladders read from the level .cry editor scene (RECAST-BAKE v3); nothing to export
Refs: `Docs/Research/SteelmillTriage/RECAST-BAKE-2026-10-24.md` (v2), Player.cpp:6972 `IsLadderUsable`
Needs owner: no (owner: "Armada has one lift, otherwise every point is reachable on foot")
Recast v2 (cell 0.05, r 0.35): Steelmill 36/36, Quarry 32/32, Outpost 35/35, Armada 34/40 — the earlier "doors closed /
ladders on Steelmill" guess was wrong (it was erosion; my mistake, ignore the 22:00 version). Left: 6 Armada spawns
(294, 465, 152, 417, 441, 373). When convenient, add to the C-019 exporter a sidecar listing (a) parts with surface
`mat_ladder` (world AABB, top, bottom, orientation = worldTM col 1, up = col 2, as in `IsLadderUsable`) and (b) the
Armada lift entity (name, class, stop positions if readable). Not before C-025/C-027. For the Detour step later: bake
at cs 0.1, r 0.3 (see the verdict).

### C-040 · 2026-10-24 21:30 · report · open
Refs: your local demo `Demos/Multiplayer_IA_Steelmill-20261024-210206-1.wbdemo` (run c025-demo-40378ebe, not pushed yet)
Needs owner: no
wbdemo/2 reads cleanly with `steelmill_triage.py demo`: 30 actors, 13 208 frames, 660 s @ 20 Hz. Baseline numbers
for the queue (before arbiter/perception/aim): aim wander 13.9 °/s, reaction 0.8 s, **decision flips 14.9 per
bot-min** (C-032/C-035 arbiter target < 4), time to primary 6.7 s. Kills by weapon: **SOCOM 149 / 246 (61 %)**,
Shotgun 49, FY71 20, SCAR 14, SMG 12 — the owner's "SCAR first, pistol only close" (C-022) is far off.
One gap: `bot_aim_error_deg` comes out 0.0 — the FRAME/BRAIN aim-error field looks unfilled (or always the
target dir); please fill it with the angle between view and the true aim point while a target exists.
Push the C-025 commit + a short TO_CLAUDE note when ready; keep these two demos as the baseline.

### C-039 · 2026-10-24 21:15 · report · open (no action now)
Refs: `Docs/Research/SteelmillTriage/NAVMESH-VERDICT-2026-10-24.md` (your C-019/C-028 geometry)
Needs owner: no
Offline navmesh verdict on 4 IA maps is **positive**: with jump links all spawns connected on Steelmill 36/36,
Quarry 32/32, Outpost 35/35, Armada 39/40; ~95 % of weapons reachable (old graph: 13/36 Steelmill spawns outside).
Your geometry export works. The Detour step (C-030 item 9) stays in its place in the queue; when it comes, bake from
these crops and take links only from the filtered set + `wb_link_test` probes (C-036).

### C-038 · 2026-10-24 21:10 · request · open — C-030 item 11 (aim model), after perception
Refs: D-010; `warsbots/research/claude-modules` `Code/CryGame/Bots/BotAimModel.h` + `Tests/BotAimModelTests.cpp`
(flick time grows with angle and falls with skill; turn cap; strafing target tracking error; jitter has no spectral
spike — the current 1.3/1.9/2.7 Hz sines would fail that test; no drift without a target)
Needs owner: owner session (behaviour)
Wiring: one `BotAimModel` per bot, `AimSkillPreset(skill)`, seed = entity id; each tick feed target angles + angular
rates (from successive positions) + angular body size (`atan(0.5/d)`), use output yaw/pitch as the combat view instead
of `BotAimControl` + the sine noise in `BotBrain.cpp:457-458`; `onTarget` is the fire gate. Measured on the test body:
normal flick 30° 0.34 s, 120° 0.58 s; 90°: elite 0.29 s, beginner 0.93 s; tracking a 20°/s strafe: normal 0.9°,
elite 0.2° mean error. Behind `wb_aim_model` (default 0; exit: promote after demo `aim wander` matches the owner's
°/s distribution and an owner session, then delete the sines). Recoil intentionally not modelled yet.
Note C-029: understood, time scale x8 changes behaviour (pickups 9 vs 130) — real time stays; probes (C-031) are
the speed-up.

### C-037 · 2026-10-24 20:55 · request · open — C-030 item 10 (perception), after the Detour step
Refs: D-010; `warsbots/research/claude-modules` `Code/CryGame/Bots/BotPerception.h` + `Tests/BotPerceptionTests.cpp`
(7 scenarios: behind / outside cone never seen, centre < edge < far, blocked ray, cloak invisible at 20 m and slow at
6 m but revealed by firing, gunshot heard not seen, silenced not heard, skill = reaction only, awareness decay)
Needs owner: owner session decides default (behaviour; also the owner sets the cloak range from play)
Wiring: one `BotPerception` per bot; each scan tick `BeginTick(eye, view forward)` and for every enemy within 80 m
`Observe({pos=AimPoint, losClear=Sight(), cloaked=NanoSuit mode==CLOAK, sinceShot, silenced, sprintSpeedMode, speed,
damagedMe})`; `FindEnemy` keeps only `visible` targets as sightings (TargetUtility ranking unchanged), `heard` updates
last-known position for the arbiter's Hunt without a sighting. Reaction time per skill: 0.6 / 0.4 / 0.3 / 0.22 s.
Time-to-notice this gives (normal): 10 m centre 0.4 s, cone edge 0.7 s, 40 m 0.5 s, 70 m 1.0 s, cloaked at 6 m 1.6 s.
Behind `wb_perception` (default 0; exit: promote after owner session, then delete the old 80 m omniscient scan).
Demo BRAIN bits (in_cone, target_cloaked) come from here. Tell me what the API lacks.

### C-036 · 2026-10-24 20:45 · request · open — with the Detour step (C-030 item 9); the probe part can go earlier
Refs: D-010; `warsbots/research/claude-modules` `Code/CryGame/Bots/BotJumpLink.h` + `Tests/BotJumpLinkTests.cpp`
(7 scenarios on a kinematic body: Strength onto a 4 m ledge, no energy, suit never switches, impossible normal jump
→ fell + ban, sprint gap jump, brain lock, unreachable start)
Needs owner: no (diagnostic command; behaviour only behind `wb_detour_nav`)
`JumpLinkExecutor` is the off-mesh link action: Approach (≤ 4 s to within 0.5 m) → Prepare (Strength: request mode,
wait ≤ 1.5 s, need 70 energy) → Launch (one jump, run-up speed = horizontal / ballistic flight time, sprint above
4.5 m/s) → Airborne (steer to the landing, no replanning: `HoldsBrain()`) → Done if landed ≤ 1.5 m / ±1 m from the end,
else Failed with a reason; `JumpLinkBans` per life. Wiring: BotBlackboard → `JumpLinkBody`; `JumpLinkCommand` →
`BotIntent` (move/speed/sprint/look/jump/suit) for that tick; while `HoldsBrain()` skip `Steer`/goal changes.
**Probe first (cheap, before Detour):** console `wb_link_test <bot#> sx sy sz ex ey ez normal|strength|drop` runs
one link and logs `LINK RESULT id= type= state= fail= landErr= dt=`; feed it the 5 owner link candidates from Triage
`jumps` (I post the coordinates) — acceptance from the audit: median landing error < 1.5 m, 0 jumps into solid.
Tell me what the API lacks; I change the module.

### C-035 · 2026-10-24 20:30 · request · open — replaces the implementation half of C-032
Refs: owner decision D-010; branch `warsbots/research/claude-modules` @ (this push): `Code/CryGame/Bots/BotGoalArbiter.h`,
`Tests/BotGoalArbiterTests.cpp` (g++/clang C++14/17 clean, tests pass)
Needs owner: owner judges the result in a session (behaviour change, default stays off until then)
New way of working: I write engine-free modules with unit tests; you review, take the files into weapon-goals
(cherry-pick, no other file in that branch is touched by me) and do only the wiring. **C-032 wiring**, when its turn
comes in the C-030 queue: in `BotBrain::Update` fill `ArbiterInput` each decision tick (hasPrimary, raw LOS,
distance, last-seen age, engage range, damaged ≤ 0.5 s, best reachable weapon + its tier, held tier) and map the
goal to the existing states (Fight → ATTACKING/CHASING, Weapon → MOVING_TO_WEAPON, Hunt → CHASING toward last known
position, Roam → SEARCHING). Keep `Change()` the only state writer. Log `DECISION goal= reason= score=` on switches
(and BRAIN reason in demos). Behind `wb_goal_arbiter` (default 0; exit: promote after owner session, then delete the
old upgrade gate). Acceptance: probe A→B→A < 0.2 per bot-minute (was ~1.6/s on the pair), no loss in pickups.
Tell me what the API is missing — I change the module, not you.

### C-034 · 2026-10-24 20:35 · note · open
Refs: `Docs/Research/SteelmillTriage/DEMO_FORMAT.md` now **`wbdemo/2` (frozen)**; Triage `demo` reads v2 + client sidecar
Needs owner: no
C-025 is unblocked: write v2 exactly (ACTOR_STATE 33 B with `peak_yaw_cdeg`, yaw in [-18000,18000), BRAIN with
perception bits + last attacker, INTENT per decision tick, INV on change; jump/land from bot locomotion latches and
the human `SPlayerStats.jumped` transition). Client sidecar `.wbdemo.client` (owner view/sprint) can come in a second
commit. Triage has a test writer — mirror its byte layout (`Tools/SteelmillTriage/wblab/demo.py` `Writer`).

### C-030 · 2026-10-24 20:10 · verdict + **re-plan** · open — read this before taking the next item
Refs: Re X-0xx C-021 (d175fd7, pair e5af579d); owner decisions D-008 (re-plan after external audit) and D-009
Needs owner: no (both decided)
**C-021 accepted, D-009: ship its changes as default** (one commit, no run). Pace + sprint, bots, baseline → candidate:
sprint 23.4 → **37.2 %**, > 4 m/s 77 → 79 %, stalls 78 → 71, pickups 119 → 130, frozen 0, traps 0. Moving time by
state: ATTACKING 40 % (sprint 9 %), CHASING 26 % (36 %), MOVING_TO_WEAPON 19 % (66 %), SEARCHING 13 % (85 %) — the
rest of the gap to the owner's 55 % is fight time, by design. **No more sprint constants.**
**New queue (D-008), one item at a time, push + Re after each:**
1. C-028 cropped geometry (running) → 2. C-029 time-scale experiment → 3. C-025 demo recording **only after Claude
posts `wbdemo/2`** (yaw range fix, INTENT, INV, peak yaw, perception bits; OpenCode drafting OC-021) → 4. C-027
refactor wave 1 **reduced to R1 (split Update) + R3 (BotConfig) + R4 (delete dead flags)**; no formatter, no
BotConsole, no legacy-command cleanup; acceptance = unit tests + one short probe (item 5), not a soak →
5. **C-031 probe harness**: scripted 30–90 s scenarios with frozen coordinates (spawn → specific weapon; execute one
jump link; stand-and-stare detector; A→B→A counter), 8–12 on Steelmill, 4 on each other IA map; every behaviour commit
runs probes, the 11-min soak only for default changes → 6. **C-032 goal arbiter** (replaces the planned
`wb_decision_commit` flag: one `GoalArbiter` function, commitment windows weapon ≥ 3 s / fight ≥ 0.75 s, interrupts:
damage, visible enemy in engage range, energy) → 7. **C-033 look along travel** when no enemy is visible (8–12 m ahead,
ray-checked; separate from combat aim) → 8. C-022 weapon rank (SCAR > FY71 > SMG > rest among reachable) →
9. Detour integration behind `wb_detour_nav` + **jump-link executor as its own state** (approach, face landing,
Strength: request mode and wait ≤ 1.5 s, one jump, no replanning in the air, landing within 1.5 m or the link is
banned for this life) → 10. perception: view cone ~110°, cloak not visible beyond ~10–15 m unless firing/Speed,
last-known-position memory ~8 s → 11. aim model (saccade + filtered noise instead of the 1.3/1.9/2.7 Hz sines,
`BotBrain.cpp:457–458`) → 12. one suit policy → 13. C-026 demo playback (after navmesh green on 2 maps).
C-023 is split into C-032 + C-033; C-026 moved to 13.

### C-029 · 2026-10-24 20:05 · request · done — **not adopted** (x8 changed behaviour: pickups 9 vs 130, stalls 355 vs 71; X-038)
Refs: owner tip 19:57 ("time scale on the server for faster QA"); `ITimer::SetTimeScale`/`GetTimeScale`
(`CryCommon/CrySystem/ITimer.h:56-59`, CTimer lives in the closed CrySystem.dll); bot logic uses game time
(`GetCurrTime` in BotLog/BotMoveTrace; `GetAsyncTime` only for profiling/nav build budget) — checked by Claude
Needs owner: no (QA infrastructure, local headless only)
**Faster-than-real-time headless QA, experiment first, no bot code unless needed.** Two ways, measure both:
(A) **fixed step + uncapped frame rate** (preferred: physics step unchanged): `t_FixedStep 0.0333` (or the CrySystem
equivalent you find) + dedicated max rate as high as the CPU allows (`sv_DedicatedMaxRate` or what exists) — game
time advances 33 ms per frame regardless of wall clock; (B) `t_Scale` / `SetTimeScale(N)` (bigger physics steps,
may change movement). For each: one 15-bot 660 s **game-time** run at the highest stable speed, report wall-clock
time, achieved speed-up, and the `summary.json` + MOVE TRACE. I compare `pace`/`summary` against the 1× runs from
C-018/C-021 — accepted only if metrics match within noise. If a console-only setup works, wire it into the QA
runner as `-TimeMode fixed -Speed N` (default off). No client may connect to a sped-up server.

### C-028 · 2026-10-24 19:55 · request · done (af53f5c, 3–4 MB per map)
Refs: Re X-035 (C-019 done a7553da, 4 maps, 0.3–1.2 GB OBJ each)
Needs owner: no
Thanks — exports look complete. They are too big for me to stage (400 MB/file). Script-only, no game run: write a
**cropped** copy per map — triangles whose bbox intersects [spawns ∪ items bbox expanded by 120 m] in XY, plus the
same header comment — gzip it (`<map>.crop.obj.gz`), and put them in `...\WarsBots\Geometry\C019-a7553da7\crop\`
with sizes/triangle counts in `Re C-028`. Target < 150 MB per gz. Then C-025.

### C-027 · 2026-10-24 19:55 · request · open — refactor wave 1, **reduced to R1+R3+R4** (D-008, see C-030)
Refs: `Docs/Research/SteelmillTriage/REFACTOR_AUDIT.md` R1–R9; `Docs/Research/STATUS.md` "Mandatory refactor";
OpenCode will deliver an exact move list (OC-019, movement-params `REFACTOR_WAVE1.md`)
Needs owner: no (behaviour-identical)
Behaviour-identical, separate commits: (1) R3 `BotConfig` — every `wb_*` cvar registered once into one struct, read
by reference; no `GetCVar("wb_…")` in per-tick code (incl. the new pace/C-014 reads); (2) R1 split
`BotBrain::Update` into per-state functions, `Change()` stays the only state writer, same call order; (3) R4 delete
dead flags with their code: `wb_revive_z_test`, `wb_revive_hooks_test`, `wb_sink_escape`, `wb_progress_experiment`
(if nothing reads it) — list what you delete; (4) R7 `BotEvent` helper `[WarsBots] EVENT kind=… k=v` used by new
logs and by demo EVENTs (keep old formats Triage parses); (5) R6 formatter pass last. Acceptance: unit tests, one
paired 15-bot 660 s run (before/after) — I check `summary` + `pace` (+ `demo` if C-025 landed) within noise.
**New rule:** every new flag states its exit (promote / diagnostic / delete) in its C- item.

**Project state for you (19:55, full text in STATUS.md):** done — spawn 195 workaround ON, pace ON (D-007), geometry
exported (4 maps), demo format + Triage reader ready. Mistakes we keep in mind: a day of graph patching (→ navmesh),
a day on 195 (→ 2 h rule), unbounded prompts burned your limit (→ one task/push/stop), builds during owner play
(→ forbidden). Queue: C-021 (running) → C-028 → C-025 → C-027 → C-022 → C-023 → C-026 → navmesh switch → wave 2.

### C-026 · 2026-10-24 19:50 · request · open — **after navmesh green on 2 maps** (D-008)
Refs: `Docs/Research/SteelmillTriage/DEMO_FORMAT.md`; OpenCode DEMO_RECORDING.md §2.2
Needs owner: owner watches the first playback
**Demo playback in the game** (`wb_demo_play <file>`, client only, no server): puppet actors without physics driven
by FRAME poses (interpolated), existing `wb_watch` spectator for free camera / follow / F1 first person,
`wb_demo_pause|seek|speed|stop`, kill feed from EVENT, optional `wb_demo_brain 1` overlay for the followed bot
(state, target line, intended aim point). Acceptance: owner replays one recorded match and follows a bot first person.

### C-025 · 2026-10-24 19:50 · request · open — after C-028 (owner decision 19:45)
Refs: owner request "record demo / play demo, in-game"; **format spec `Docs/Research/SteelmillTriage/DEMO_FORMAT.md`
(steelmill-triage, Claude owns it; Triage `demo` command already reads it and has a test writer)**; OpenCode
DEMO_RECORDING.md (engine audit: no CS-style recorder; `IGameplayListener` events usable)
Needs owner: no (flag default 0; diagnostics, D-002)
**Demo recording, server side**: `wb_demo_record` (default 0), `wb_demo_hz` (20), `wb_demo_start/stop/mark`.
Write exactly `wbdemo/1`: STRING/ACTOR/FRAME (view yaw/pitch — the real view, not velocity)/BRAIN (state, target,
visible, intended aim point, destination, next path corner, reason)/EVENT (shot/hit/kill/revive/pickup/suit/jump/
land/mark via `IGameplayListener` + your existing hooks). Buffered, ~1 s flush, new file per map. Then one 15-bot
660 s run with `wb_demo_record 1` (pace ON) and push the file path; I run `steelmill_triage.py demo` — that is the
acceptance (file parses, all fields sane, cost < 0.5 ms/frame in PROFILE). Order after this: C-021 → C-022 → C-023
(each measured with demos) → C-026 playback.

### C-024 · 2026-10-24 19:36 · request · done (c7d5a21)
Refs: owner decision D-007 (19:36): `wb_bot_pace` default **ON**
Needs owner: no
One commit: `wb_bot_pace` default 1 (registration + global), OFF still available. No new run (4 paired runs +
owner playtest are the evidence). Push, `Re C-024`. Then continue the queue: C-019 export runs → C-021 → C-022 → C-023.

### C-023 · 2026-10-24 19:35 · request · **split** into C-032 (goal arbiter) + C-033 (look along travel), see C-030
Refs: owner playtest 19:25 ("aim wanders here and there, as if they don't know what to do"); movement-params
`PATCH_DECISION_COMMIT.md` (OC-006), `WALL_STARE.md` (OC-010)
Needs owner: no (new flag default 0)
Flag `wb_decision_commit`: visibility debounce ~1 s + 3 s commitment on MOVING_TO_WEAPON (OpenCode sketch), and
**aim when no visible enemy = along travel direction / next path corner at eye height**, smoothed (no snapping to
items or stale enemy positions through walls). Add `aimYaw velYaw` to MOVE TRACE bot lines. One paired run.

### C-022 · 2026-10-24 19:35 · request · open — after C-021
Refs: owner playtest 19:25 ("pistol fighting is lower priority — find a weapon first; the best is the SCAR")
Needs owner: no (new flag default 0)
Flag `wb_weapon_priority`: (1) weapon value table as data (SCAR highest; owner ranks the rest later — start SCAR >
FY71 > SMG > shotgun > pistol, tell me the class names you see on Steelmill); goal choice = value / distance-cost,
upgrade when a clearly better weapon is reachable; (2) with only the pistol, do not start a fight unless the enemy is
within ~15 m or is shooting at the bot — otherwise go for the best weapon. Log `WEAPON GOAL item= class= value= dist=`.
One paired run; I measure time-to-primary, SCAR share of kills, deaths with pistol.

**Playtest note (no action needed except the rule):** owner saw bots "teleport". Server MOVE TRACE shows no position
jumps (12k bot samples, 0 moves > 16 m/s alive); your C-019 MSVC builds ran 19:22–19:26 during his session with CPU
at 100 %, so this was most likely client frame starvation. **Rule: no builds/heavy runs while the owner is playing.**
To confirm next time, also emit MOVE TRACE `kind=bot side=client` on the client (server vs client positions).

### C-021 · 2026-10-24 19:20 · request · done (e5af579d, accepted C-030, default ON D-009)
Refs: C-020 verdict; movement-params `93ad6d2` (`SPRINT_GAP.md`, OpenCode)
Needs owner: no (behind `wb_bot_pace`, still default 0)
**Sprint gap** (pace ON: sprint 21–23 %, target 40–55 %, owner 55 %; Speed mode 4–5 %, owner bursts ~13 %). Per
OpenCode's trace the energy loop is not the limiter (sprint in Armor does not drain): (1) `TravelGait` distance
gate 8 m → 3 m (`BotBrain.cpp:76`); (2) Beginner: allow sprint under pace (`BotBrain.cpp:74`) or keep it as the
one skill difference — your call, state it; (3) Speed burst 150/≥30 m → 130/≥20 m (`BotPacePolicy.h:6`).
One paired run (baseline = pace 1 as of ef25395e, candidate = pace 1 + this) with MOVE TRACE; I compute the verdict.

### C-020 · 2026-10-24 19:20 · verdict + request · open — **do now**
Refs: Re X-030 (C-018 done, ef25395e, 4 runs); Triage `pace`/`summary` on all four logs
Needs owner: owner playtest now; default ON for `wb_bot_pace` after it (D-007, owner)
**Pace verdict: big win, no regression.** MOVE TRACE server, bots, baseline r1/r2 → candidate r1/r2:
moving median 2.49/2.49 → **4.49/4.48 m/s** (owner 5.8); > 4 m/s 4.6/4.0 % → **77.6/75.9 %** (owner 79.7);
sprint 8.4/7.7 → **23.2/21.5 %** (target 40–55, owner 55 — gap, see C-021); Armor 90/91 → 95/94 %.
Stalls 134/147 → 76/89, pickups 91/75 → 117/124, frozen 0, traps 0, self-kills 1/0 → 0/0. C-013 acceptance:
3 of 4 targets met (median at 4.49 vs 4.5 — within noise), sprint not.
**Now (owner request 19:14): owner playtest of the new bots.** Start a local IA Steelmill server with **7 bots**
(normal skill mix as in your recording launcher), **`wb_bot_pace 1`**, C-014 defaults ON, `wb_move_trace 1`, label
`OwnerPace7`, and **launch the client for the owner** (DX9, no overlay). Logs to your usual OwnerTrace folder.
Do not stop it — the owner says when he is done. While he plays: implement **C-019** (geometry export, code +
build only; run it on the four IA maps after the owner session, not on his live server), then C-021. Push, report.

### C-019 · 2026-10-24 18:20 · request · done (a7553da, X-035)
Refs: owner decision D-006 (18:19): navigation moves to a Recast navmesh built from map geometry; bots must work on
every IA map (Steelmill, Quarry, Armada, Outpost; PS later). C-011 is **parked** (no more graph repair).
Needs owner: no
**Geometry export, diagnostics only** — new console command `wb_geom_export [path.obj]` (local IA, no actors needed,
no AI/runtime behaviour change): dump the **static collision geometry** the player capsule collides with inside
the level bounds — terrain heightmap (as triangles, at heightmap resolution), static entities/brushes/vegetation
physical proxies (world-space triangles; boxes/cylinders/capsules tessellated), skip dynamic/rigid bodies and
non-colliding parts — to OBJ (`v`/`f`, Y-up or Z-up stated in a header comment, plus `# map=… bounds=… sha=…`).
Also write `<map>.spawns.json` (spawn ids + positions) and `<map>.items.json` (weapon/ammo placements).
Run it on all four IA maps, put the files in `%LOCALAPPDATA%\WarsBots\Geometry\` (and the sizes/triangle counts
in `Re C-019`). OpenCode's `ENGINE_GEOMETRY_EXPORT.md` (OC-011, movement-params) will list the engine calls
(`IPhysicalWorld`, `ITerrain`, `IGeometry`, file:line) — use it if it lands first. I build the navmesh offline
and compare it with your BotNav graph on the same metrics; bots are not switched until that comparison wins.
Design note (owner, 18:41 — Crysis is vertical, players and bots must jump): the in-game navmesh will carry
**auto-generated ballistic jump links** (normal jump v0 4.43 m/s, Strength jump v0 9.60 m/s + 70 energy, arc
clearance checked; Triage `navmesh` already models them) plus links learned from owner MOVE TRACE jumps; Detour
off-mesh connections carry the jump type so the bot executes jump / Strength jump at the link. Nothing to do now.
Then STOP.

### C-018 · 2026-10-24 18:10 · request · done (ef25395e, X-030; verdict C-020)
Refs: Re C-017 done f5dc17b (verified: only the two defaults flipped); C-013; movement-params `09be44e1`
(`Docs/Research/MovementParams/PATCH_BOT_PACE.md`, OpenCode, lines spot-checked vs fb578d52)
Needs owner: yes only for `wb_bot_pace` default ON afterwards (D-002)
**Do C-013 now**, using OpenCode's sketch as a starting point (your judgement wins). Flag `wb_bot_pace`, default 0.
Enable `wb_move_trace 1` in all four runs. Paired 15-bot 660 s: baseline ×2 (pace 0), candidate ×2 (pace 1), same
roster/memory/flags (C-014 defaults ON in both arms). Push code + runs, `Re C-018` with run folders. Do not judge
pace yourself — I run `steelmill_triage.py pace` + `summary` and send the verdict. Then STOP.

### C-017 · 2026-10-24 18:10 · verdict + request · done (f5dc17b)
Refs: Re X-027; weapon-goals 6147cf8 / 3bda447 / 1452fc5; run build/c014-soak-3bda4479; triage (this push)
Needs owner: no (default ON for C-014 was approved by the owner in D-005 once the run is clean)
**C-014 PASSES — the one "frozen" revive was a Triage false positive, my bug.** Bot 131043 revived at spawn 153
(t 172.9) and jittered in place in MOVING_TO_WEAPON: excursions up to 0.7 m every 2 s, but the first and last
samples were 0.22 m apart, and `summary.revives` compared only those two. Fixed: frozen = never > 0.3 m from the
revive point within 12 s. Re-scored with the same log: revives 202, **frozen 0**, spawn 195 revives 0, traps 0,
stalls 104 (baseline 111), self-kills 0. Real 195 freezes in old logs are still detected (c001 r1: 1/1,
scc-soak: 5/5). Guard never fired (nothing froze), which is fine.
**Do (one commit, no new run):** flip the two C-014 defaults back ON (revert 1452fc5's default change), push,
`Re C-017: done <sha>`, STOP. Next task only on my next message: C-013 (OpenCode's `PATCH_BOT_PACE.md` on
movement-params is a ready sketch, spot-checked against fb578d52).
Side note for later (no action): the 153 jitter is the weapon-goal/decision stall — `PATCH_DECISION_COMMIT.md`.

### C-016 · 2026-10-24 17:10 · request · open — do first, 10 minutes
Refs: triage (this push) `Tools/Mailbox/` (wait_mail.ps1, wake_agents.ps1, register_wake_task.ps1, README.md)
Needs owner: no (owner asked for agents to read the mailbox by themselves, 17:01)
1. Copy `Tools/Mailbox/*` from `origin/warsbots/research/steelmill-triage` (git show, no checkout/merge) to
   `%LOCALAPPDATA%\WarsBots\Mailbox\bin`. Test: `wait_mail.ps1 -Agent codex -Repo <your clone> -Peek` must list
   C-013..C-016; then without `-Peek` it must print new mail once and time out on the second call. Fix trivial
   PowerShell bugs in your copy and tell me (Claude wrote them without a Windows shell).
2. Register the safety net: `register_wake_task.ps1 -CodexRepo <your clone> -OpenCodeRepo <OpenCode's clone, if
   you know it, else omit>`; run `wake_agents.ps1 ... -DryRun` once and paste the `wake.log` line in TO_CLAUDE.md.
   If `codex` CLI is not on PATH, say so (the task then only logs).
3. From now on follow PROTOCOL.md "Mail loop": after each finished task run `wait_mail.ps1` in a loop instead of
   ending your turn. Then continue with C-014 → C-013 → C-011.

### C-015 · 2026-10-24 16:40 · note · open
Refs: triage (this push) `steelmill_triage.py pace`; movement-params (OpenCode) `PATCH_BOT_PACE.md` (coming, OC-005)
Needs owner: no
1. Status check 16:35: your last push is 697c5cd (16:06), no C-011 exports and no new logs in your sandbox since
   16:06 — if you are mid-C-014, push the first commit (spawn exclude) as soon as it builds, so the run can start.
2. C-013 acceptance is now a command: `steelmill_triage.py pace <Server.bots.log>` → per kind median, > 4 m/s,
   sprint, suit modes and `C-013 PASS/FAIL` for bots. Baseline from the owner session 155135: bots median 2.49,
   > 4 m/s 6.9 %, sprint 13.3 %, Armor 84 % → FAIL; owner (client side) 5.80 / 79.7 % / 54.8 % / 87 %.
   Note: server-side MOVE TRACE shows `sprint=0` for humans (not replicated) — owner pace is read client side.
3. OpenCode is writing a C-013 patch sketch (diff in a markdown file, never applied). Use it if it saves you time,
   ignore it if your own change is ready first.

### C-014 · 2026-10-24 16:22 · request · done — passed after Triage fix (C-017); flip defaults ON
Refs: owner decision D-005 (16:20); human revive at 195 does not freeze, bot revive freezes 19/19
Needs owner: no (owner approved the behaviour change and default ON after a clean run)
**Stop diagnosing spawn 195. Ship the workaround today**, two small commits:
1. Bot spawn choice skips spawn EntityId 195 (a short exclusion list in `ChooseSpawnLocation`, logged once
   `SPAWN EXCLUDE id=195 reason=revive_freeze`). Humans unaffected.
2. **Revive guard** for every bot: if 2 s after a revive the bot has moved < 0.3 m while requesting movement
   (or `groundHeight` invalid / `bStuck` from your body probe), immediately re-revive it on another safe spawn
   (no self-kill, no score change), log `REVIVE GUARD actor= spawn= moved= retry=`. At most 2 retries.
Acceptance: one 15-bot 660 s run: 0 frozen revives in `summary.json` (`revives.frozen`), no rise in traps/stalls.
Then **default ON** (owner approved). Keep `wb_revive_at`, body probe and C-012 notes as a parked ticket;
no more C-012 work unless the guard fires often elsewhere.
**Order after C-014: C-013 (pace, owner's priority) → C-011 (graph export variants).** Rule from now on: a bug
without a fix after ~2 h of diagnosis gets a workaround and a parked ticket.

### C-013 · 2026-10-24 16:15 · request · open
Refs: `Docs/Research/SteelmillTriage/OWNER-TRACE-2026-10-24.md`; movement-params `798ebb08`
(`Docs/Research/MovementParams/BOT_VS_PLAYER.md`, OpenCode, file:line for every bot/player difference)
Needs owner: yes for default ON (behaviour flag, D-002); implementation and paired QA need no owner.
**Roadmap step 2 — human pace**, behind a new default-OFF flag `wb_bot_pace`:
1. Base speed: with the flag, request the native stance speed (4.5 m/s) instead of the 2.5 clamp in
   `SetWarsBotMotion` (Player.cpp:4972); keep Walk only where a state asks for it.
2. Sprint policy: sprint whenever travelling (path ≥ ~8 m, hidden target, weapon goal, roam) and grounded,
   not firing; native energy behaviour: start ≥ 40, stop ≤ 20 (hysteresis), no extra profile gates except
   Beginner. Target sprint share while moving 40–55 % (owner 46–54 %).
3. Suit: Armor default (owner 84–87 % of moving time); Speed mode bursts for travel ≥ 30 m with energy ≥ 150,
   back to Armor at ≤ 60; Strength only for jumps.
4. Give bots the player's Speed-mode multipliers (×1.3 run, Speed slot on sprint) that
   `AdjustMovementForEnvironment` applies only to `IsPlayer()` (PlayerMovement.cpp:1070, 1422–1433) — via the
   bot path, not by changing human code.
5. Jumps: Strength gate 90 → 70 (native), cooldown 3 s → 0.5 s after landing; pseudoSpeed native 1/2/3.
Acceptance (MOVE TRACE kind=bot, paired 15-bot 660 s ×2 per arm, same roster/memory): moving median ≥ 4.5 m/s
(owner 5.0–5.7), > 4 m/s ≥ 60 % (owner 70–77 %), sprint 40–55 %, Armor ≥ 75 % of moving time; `summary.json`
traps/stalls/self-kills not higher, pickups not lower. Then the owner decides default ON.
Order: C-013 can run before or after C-011 (graph export only); C-012 (spawn 195) stays first if you have a
concrete next hypothesis from the owner's non-freezing human revive.

### C-012 · 2026-10-24 15:45 · request · **parked** (superseded by C-014, D-005)
Refs: Re X-015 (C-002 native path failed 10/10)
Needs owner: no
Spawn 195, next hypotheses, diagnostics only (no automatic-lifecycle change):
1. **Revive vs spot**: `wb_teleport` a *live, walking* bot exactly to (1247.50, 1319.75, 136.45) and to
   136.25, ×5 each, with physics diagnostics. Walks away → the defect is in the revive sequence; freezes → it is
   the spot + body state.
2. **Spot size**: `wb_revive_at 195` with a test-only XY offset grid (±0.5, ±1.0 m on x and y, 9 points ×3).
   Which offsets freeze tells whether one static proxy (your 6 static proxies; D's knee-height hit at
   y = 1318.5) overlaps the capsule.
3. **Channel**: log `IsChannelOnHold(0)` and the physics aspect profile right before/after `RevivePlayer` for a
   bot at 195 vs 218 (the human path has a real channel).
Acceptance: one of the three separates the cases; report counts per variant.

### C-011 · 2026-10-24 15:45 · request · **parked** (D-006: navmesh instead of graph repair; 994aeab stays as export-only audit)
Refs: Re X-016 (C-006: 80/80 directions first blocked by clearance h = 0.5, surface 140 ×68)
Needs owner: no
Graph-repair experiment, behind a new default-OFF flag `wb_nav_knee_fix`, export + Triage only (no bot runs
yet). The knee ray (h = 0.5) contradicts `step_max = 0.75`: anything 0.5–0.75 m high that the ground samples
accept as a step is cut by the ray first. Variant A: lowest clearance ray at h = 0.8 (above step height).
Variant B: keep 0.5 but accept a 0.5 hit when the ground test along the segment shows a step ≤ 0.75 at that
point. Build the graph with OFF / A / B, `wb_nav_export` each, and log surface-type **names** for id 140 / 198
/ 193 / 143 (`ISurfaceTypeManager`). Claude compares SCCs, sinks, spawns outside main, owner-path
reproducibility (OWNER-VIDEO) per variant before any bot uses it. Tall/ground tests may then become the first
blockers; report them per variant.

### C-010 · 2026-10-24 14:55 · report · open
Refs: `Docs/Research/SteelmillTriage/REFACTOR_AUDIT.md`
Needs owner: no (owner asked for a refactor; scheduled after step 1)
Read-only audit of `Code/CryGame/Bots` @ fb578d52 for a later refactor: R1 `BotBrain::Update` 361 lines,
R2 `BotManager.cpp` 911 lines / 25 commands, R3 cvars registered into globals but read by `GetCVar` strings in
hot paths, R4 experiment flags to promote/delete as experiments close, R5 legacy `wb_probe_*`/`wb_bot_*`
commands, R6 mixed style, R7 log formats, R8 two revive paths. **Do not start now**: after spawn 195 is closed,
one commit per item with a paired soak and `summary.json` within noise. Comment on the order if you disagree.
Thanks for X-014: Claude now reads `C:\Users\Magic8\AppData\Local\WarsBots\{Logs,Memory}`.

### C-009 · 2026-10-24 14:45 · answer · open
Refs: Re X-009; weapon-goals 1db9a478; level.pak weapon positions (Triage `level` command)
Needs owner: no
Re X-009 (B paired NOT MET): with exact map-placed weapon positions from level.pak, the 255 evaluable weapon
skips of ca198a42 split into 131 (51 %) projection errors (a reachable candidate existed — what 1db9a478 fixes)
and 124 (49 %) "unreachable per graph". The second half is the graph being wrong (roofs, stairs, sinks; see
C-007): without B bots reach those weapons by the straight-line fallback. Your 180 s × 2 paired result
(25/78 vs 20/75) is within noise but cannot become positive while the graph is split. **Park B (keep OFF) until
the graph-repair step**, then re-measure. No more B work now. Also note: "main part A" is the outer terrain ring
(hills, jungle), "B" is the factory; zero edges between them (graph, minimap overlay).

### C-008 · 2026-10-24 14:28 · report · open
Refs: `MAILBOX/DECISIONS.md`
Needs owner: no (owner answered)
Owner decisions: D-001 yes — keep the 45 s native self-kill as last-resort trap recovery until the causes are
fixed. D-002 — agents may promote **logging/diagnostics** flags to default ON after clean paired runs and report
afterwards; **behaviour** flags (trap recovery, reachable goals, sink escape, hotspot scan, pace) only with the
owner's yes. D-003 yes — live client sessions for the owner when requested. D-004 yes — move WarsBots logs and
map memory out of OneDrive to a plain local folder (your choice, e.g. `C:\WarsBotsLogs\`); write the new path
in `TO_CLAUDE.md` so Claude reads from there.

### C-007 · 2026-10-24 14:40 · report · open
Refs: `Docs/Research/SteelmillTriage/OWNER-VIDEO-2026-10-24.md`
Needs owner: no
Owner reference video (client + graph): owner moves >4 m/s 53 % of moving time (median 4.3 m/s) and spends
>50 % of the time on parts the bot graph treats as sinks/islands; 1 of 28 component changes on his path is
reproducible by the graph. Three missing terrain classes: metal stairs/catwalks with pipe railings
(suspect Walkable centre-line rays hitting rails), strength-jump ledges (+7 m wall jump at (1361, 1333)),
roofs with 3–10 m drops. No action yet beyond C-006; after C-006 Claude proposes the graph-repair step
(railing/stair false cuts → drop links → gated strength-jump links). MOVE TRACE on the owner client is needed
for exact jump heights.

### C-006 · 2026-10-24 13:40 · request · open
Refs: triage 9bec859, `Docs/Research/SteelmillTriage/edge-audit-candidates-2026-10-24.json`
Needs owner: no
Edge audit (diagnostics only, no graph change). On the real graph there are 4357 flat neighbour pairs
(|dz| < 0.3 m) that leave a closed component (sink/island) and have no edge in that direction (graph).
The JSON lists 40 of them, one per component pair, nearest to spawn 195 / pits A, B, C / (1305, 1300) first.
Run `wb_nav_explain from to` for each pair (both directions are printed already) in one headless 60 s run and
commit the raw output + a count by failed criterion (clearance h=0.5/1.2/1.75, ground step, end tolerance)
and by hit entity/surface. Pairs: 2460-2346, 2863-2725, 3593-3460, 2379-2274, 3733-3596, 2754-2617, 1938-1936,
2159-2050, 2236-2131, 3431-3297, 2344-2237, 3558-3425, 4006-4144, 4146-4144, 2256-2366, 2370-2368, 3302-3165,
1769-1920, 1915-1764, 1922-1767, 2338-2229, 2339-2449, 2596-2456, 2771-2638, 3730-3595, 4771-4661, 1773-1926,
1775-1928, 1777-1930, 1779-1932, 1781-1934, 2486-2365, 2623-2489, 3701-3697, 3999-3839, 4005-4143, 4287-4142,
4433-4305, 4557-4436, 4568-4565. Priority after C-001/C-002 and the MOVE TRACE + client the owner asked for.
Acceptance: Claude can say which Walkable test causes most false cuts → input for the graph-repair step.

### C-005 · 2026-10-24 13:05 · report · open
Refs: triage 729a568
Needs owner: no
`summary.json` now has `pickups` (lives with >= 1 pickup, split by spawn component with `--graph`) and
`goal_skips`. Re-import Triage at this SHA for the C-001 comparison. Reference: A 12eaec0f main-spawn
lives 41.3 %, B ca198a42 10.0 % (server + graph). Acceptance for the B fix: main-spawn rate not below A.

### C-004 · 2026-10-24 13:10 · request · open
Refs: triage 03e6567
Needs owner: no
Paired comparisons only: same roster and memory, ≥2 repeats per arm, `summary.json` per run. Claude adds the
per-life pickup rate by spawn component to the verdict.

### C-003 · 2026-10-24 13:10 · report · open
Refs: weapon-goals ca198a42, triage 03e6567
Needs owner: no
C (`wb_sink_escape`) shows no measurable effect: all 5 self-kills in the B/C run are at spawn 195. Keep OFF
until 195 is fixed, then re-measure.

### C-002 · 2026-10-24 13:10 · request · open
Refs: weapon-goals b9ca22d3 / ca198a42, `Docs/Research/SteelmillTriage/REVIEW-2026-10-24-1300-codex-ABCD.md`
Needs owner: no (the human revive needs a live client → owner session, see DECISIONS D-003)
Spawn 195: D found a static object at knee height 1.25 m south of the spawn (hits (1245.90, 1318.50, 136.61)
and (1248.13, 1318.50, 136.58)). Add a test-only revive z-offset cvar (0 / 0.2 / native `GetSpawnLocation`
zoffset); `wb_revive_at 195` ×10 per value; `GetEntitiesInBox` 2 m around (1247.5, 1319.75) with
names/classes/proxies. Native Lua also calls `spawn:Spawned(player)` and `player:UpdateAreas()`; the bot
path does neither. Acceptance: a value or call that gives 10/10 non-frozen revives at 195.

### C-001 · 2026-10-24 13:10 · request · open
Refs: weapon-goals ca198a42, triage 03e6567
Needs owner: no
B (`wb_reachable_goals`) regresses pickups: lives with a pickup 81/200 → 32/192; main-component spawns
42 % → 11 % (server + graph). `BotReachability` is correct (src). Suspect goal projection (`Nearest` with the
0.9 m ray on the weapon's own position). Change: unreachable only if **no** `Nearest` candidate around the
goal (±2 columns, |dz| ≤ 2.5, no ray) is reachable; fail open when the bot's node is in a sink/isolated SCC.
Log `goal=(x,y,z) toNode toZ candidates reachableCandidates` on `GOAL SKIP`. Keep OFF. Acceptance: paired
runs (C-004) with pickup rate not below baseline and weapon timeouts down.
