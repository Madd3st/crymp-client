# Agent handoff — WarsBots for CryMP (updated 2026-10-25)

C071 acceptance FAIL: actualdc9808c8=requested6be78151+known compile-only fix91a.
MSVC+15units PASS/runtime no crash, but stalls135>48 and pickups195 vs247 fail;
traps0/level4=0/kills241 pass. Frozen-revive metric flags3 atspawn120 need triage.
Raw build/c071-on-dc9808c8 includes c071-gates.json and focused stair traces;
8actors/1363MOVE samples near stairs. X065 sent, no merge/promotion or extra runs.
Owner preset271 unchanged; memory restored/no game active. D013 unchanged.

C070: acceptedc648 merged c78259cd. Requested293 failed MSVC (Player.cpp5221 ZERO);
one-line compile-only91a01bbc pushed codex/c070-msvc-zero, fixed MSVC PASS.
15 already passed units unchanged/reused; ON15x660 actual91a runtime PASS, native
manifest errors0/INTENT WHY present. Raw build/c070-on-91a01bbc, X063 sent.
Matched pair frozen play-91a01bbc, not falsely labelled293. GUI/NET POSE client
NOT RUN; memory restored/no game active. Waiting Claude verdict; PS export gate blocked.

C0696f6e97f1 MSVC+15units PASS. First ON invalid (CRLF broke manifests), stopped;
canonical Git-data rerun15x660 runtime PASS/mismatches0. Evidence in
build/c069-on-canonical-6f6e97f1 and c069-data-evidence.json. LF tooling fix0a1fc6b4;
needs propagation to claude-sprint, no game code changed. X062 sent, verdict pending.
Memory restored/no game active. PS metadata extracted; OBJ/crop BLOCKED by IA-only
GeometryExportCommand/export script; separate exporter gate requested from Claude.

C068c648ea3b: MSVC+14units PASS, ON15x660 tactics runtime completed/no crash;
TRAPS1 observed, needs Claude verdict; not auto-accepted/merged. HOLD markers present.
Raw build/c068-on-c648ea3b; frozen matching play-c648ea3b, X061 sent.
Owner default27144dc6 unchanged; optional tactics preset AST PASS, GUI NOT RUN.
Memory restored/no games active. C068 PS geometry extra queued, NOT completed.

C067: accepted68c50704 merged via302423fc, pushed. Next27144dc6 MSVC+13units+
ON15x660 all9/handling/vantage runtime PASS; raw build/c067-on-27144dc6, X060.
VANTAGE246 load only, no claims; awaiting Claude quality verdict before merge271.
Owner preset now defaults frozen play-27144dc6 with names1/clientSmooth1/trace2;
PrepareOnly/AST/flags PASS, GUI NOT RUN. Memory restored, no game active; D013.

C06668c50704: MSVC+13units+ON15x660 all9/handling1/vantage1/names0 runtime PASS.
New spot data installed via official script; VANTAGE246 loaded, no claim markers,
so tactical holding NOT confirmed. Raw build/c066-on-68c50704; X059 sent.
Frozen play-68c50704 (matching ammo pose pair); owner preset ready, GUI NOT RUN.
Memory restored/no games active. Pending Claude verdict before merge; D013 unchanged.

C065fe08fb70: MSVC+12units+ON15x660 all9/handling1/names0 runtime PASS.
Frozen play-fe08fb70 includes two new optional ammo fields; matched client/server
required. Raw build/c065-on-fe08fb70; X058 sent, waiting Claude before merge.
Owner preset supports handling/names for new manifest; old0470 remains default.
GUI not launched, memory restored, no game active. D013 no Codex gameplay edits.

C064 owner717 finished by explicit owner message, evidence archived under
LOCALAPPDATA/WarsBots/Logs/Owner717-20261025-143230 (demo10316frames readable).
C063 SAME717853ff ON15x660 all9 runtime PASS; build/11units reused, no rebuild.
Raw build/c063-on-717853ff; frozen play-717853ff. Original mem/nav restored,
servers stopped. X057 sent; waiting Claude verdict before merge717. D013 unchanged.

C062 follow-up8214f16a: MSVC+11units+ON-only15x660 all9 runtime PASS;
accepted OFF0470 reused as specifically requested, no duplicate smoke/control.
Raw build/c062-on-8214f16a, frozen play-8214f16a; waiting Claude verdict before
merge8214. Owner preset remains accepted0470, all9ON/clientSmooth1/trace2.
No games active, original map memory restored. D013: no Codex gameplay changes.

C062 accepts C061: merged0470fe7e via67201705, default gates remainOFF.
Owner preset Tools/Play-WarsBotsSprint.ps1 uses frozen play-0470fe7e, nine gatesON,
clientSmooth1/trace2; PrepareOnly/AST PASS, no game launched. Next8214f16a ON-only
per mailbox, accepted OFF control reused. D013 no Codex gameplay changes.

C0610470fe7e: MSVC+11units/default9OFF+clientSmooth0/smoke6/pair15x660
runtime PASS. New link meshes installed, former data build/c061-navmesh-before/.
Raw demos/receipts build/c061-pair-0470fe7e; source/hash frozen play-0470fe7e.
No gameplay fixes; memory restored, no games active. X052 sent; waiting Claude
metrics/verdict before merge. Official research wait_mail.ps1 imported for idle polling.

C0586f941cda: build+9units/default8OFF/smoke6/15x660OFF-ON runtime PASS;
metrics verdict pending Claude, no merge. C060 accepts C055 code with stuck/moving
metric split. Next C0610470fe7e. D013: no Codex gameplay edits. Memory restored;
servers stopped. Raw build/c058-pair-6f941cda; X051 in Coordination/TO_CLAUDE.md.

## C055 not accepted; D013 roles acknowledged

50a516f7 ON15x660 wall159/493.9s vs OFF14/46. Second19989a6d
MSVC+LookPolicy unit PASS, ON6x120 wall24/68.4s vs retained control0/0.
No merge/default promotion; next gameplay correction belongs to Claude per C057.
Review branch codex/c055-policy-bypasses; C0586f941cda fetched, QA pending.
Servers stopped, original memory restored. Evidence: [C055 report](Experiments/c055-look-2026-10-25.md).

## C044 candidate fixed; arbiter next

52df15c1 on claude-sprint: capped final owned-bot view after native rotation;
first implementation regressed and was corrected.6x120 fixed candidate yaw>720
0.945% vs control10.495%; no sampled rates>profile cap+5%; default remainsOFF.
Client remote MOVE TRACE mode2 available, live visual test pending; matched new
client/server required (optional pose bit). See Experiments/c044-turncap-2026-10-25.md.
Full batch compiled;6 new modules+config/trace units PASS. Not merged before full QA.
C043acceptedC052; C046not adopted,1x retained. Next arbiter then C050 gate batch.

## C046 complete; C044/C050 batch next

Same C043 b902dfb4 control/roster/memory/flags at scale2 and4,660game s each.
1x/2x/4x pickups134/92/28,stalls69/117/152,kills236/217/186; no adoption.
Keep1x for acceptance. Report Experiments/c046-timescale-2026-10-25.md.
Memory restored; no QA/game active. C050ae798245 ready; C051 restores Codex
ownership of game integration. Review/build next, C044 before arbiter per owner.

## C043 pair complete; C046 next

Pre-refactor40378ebe vs C027b902dfb4,15bots660s each1x, matched fixture.
Runtime PASS,frozen0/pinned0/traps0; kills240->236,pickups115->134,stalls63->69.
No equivalence verdict/default change asserted; Claude scores noise.
Report: Experiments/c043-paired-2026-10-25.md. Memory hash restored.
Owner order now C043->C046->C044->arbiter. C048: Claude owns new gameplay code;
Codex builds/tests/merges BATCH READY, after timed QA to avoid CPU confounds.

## C031 base probe harness and baseline, 2026-10-25

Harness c6bd056d; game build b902dfb4 unchanged, no C++ build/client/gameplay changes.
20 fixed-coordinate30s scenarios completed without runtime/engine errors:
8 Steelmill,4 Quarry,4 Armada,4 Outpost. Specific weapon4/10, native jump3/5;
known misses preserved.5 analyzer units PASS. Steelmill decision probe45 A-B-A.
Results/log hashes: Experiments/c031-probes-2026-10-25.md and sibling JSON folder.
Two existing memory/nav files restored by hash;6 initially absent paths absent.
No game/QA active. Foreign launcher edit preserved. Off-mesh jump-link execution
still awaits C036; stationary metric does not claim geometry wall-stare detection.
C043 long pair deferred to follow owner's latest C027-finished/next-C031 scope;
no equivalence acceptance claimed. C044/C045 acknowledged, not implemented here.
Next approved work: arbiter C032/C035, then C033, subject to new owner/mail steering.

## C027 complete; owner recording archived

R3 6e576693 BotConfig, R1 caddc3ea Update split, R4 b902dfb4 closed experiment
removal compiled (MSVC x64 client/server);5 affected unit tests PASS. A60s15bot
smoke passed:17kills,11pickups,976shots,340hits,frozen0,pinned0%,traps0;6stalls.
This is stability evidence, not paired behavioral equivalence. No new defaults.
OwnerRecord7-20261024-233350 server stopped by owner request; four logs+demo
archived in LOCALAPPDATA/WarsBots/Logs/OwnerRecord7-20261024-233350/.
Demo20996frames/1049.6s/20.003Hz reads cleanly, original retained in Demos.
Owner memory restored after QA. No game/QA active. Foreign launcher revert
retained; only own obsolete sink0 token removal staged separately.
Details/hashes: Experiments/c027-refactor-2026-10-24.md. C042 acknowledged for
later C033; next approved development step C031 probes. No further work started.

## C025/C040 complete; next C027 reduced refactor

Recorder+pickup fix40378ebe, diagnostic aim5d5276a9. Two660s baseline demos
preserved in LOCALAPPDATA/WarsBots/Demos (initial pickup gap; fixed has all required
EVENTs).20Hz; fixed average0.0339ms/frame. Codec/wire/reader tests PASS. Target yaw
error baseline3.4/3.6deg; native-eye3D short probe separately reported, no fabricated
historical3D. Data/commands/paths in Experiments/c025-demo-2026-10-24.md.
No active QA/owner client, memory restored. Foreign launcher edit remains unstaged.
C021accepted/defaultON, constants frozen. C030/D008 queue now C027(R1+R3+R4 only),
C031probes,C032/C035arbiter,C033travel look,C022weapon,Detour+jump,perception,
aim,suit,C026after navmesh green2maps. C041 ladder/lift export low priority.
Between tasks use wait_mail.ps1; no builds/heavy QA during owner play; flag exits required.

## Historical checkpoint: C-018 paired pace QA complete

Game source ef25395e; wb_bot_pace default0. Four15bot660s paired runs completed
661.49/661.73/661.55/661.30s. Same roster/memory/EXEs/flags, trace1, C014defaultsON.
All4 frozen0,traps0; watchdog selfkills B1=1, others0. All8 graphs identical. Memory restored,
QA servers stopped. MSVC x64 client/server +policy unit+28Triage tests PASS.
Pace verdict belongs to owner/Claude; no default promotion. C019 not started.
[Results, commands and artifact paths](Experiments/c018-2026-10-24.md).
Raw batch: C:\Users\Magic8\WarsBotsBuild\crymp-multibot\build\c018-paired-ef25395e\.
STOP after final report push; no mailbox wait-loop or other tickets.

## Historical checkpoint: C-017 accepted C-014, defaults ON

Owner explicitly approved restoring both C-014 defaults. wb_spawn_exclude195=1 and
wb_revive_guard=1 (globals + console defaults), matching tested3bda4479 source exactly.
Claude corrected metric e54ebbc and rescored the SAME660.67s log:202revives,frozen0,
spawn195revives0,traps0,stalls104 vs111,selfkills0. Previous frozen153 was a false positive
(first/last positions hid0.7m excursions). C-014 acceptance PASS per C-017; X-027 superseded.
No new run/build/local rescore; existing frozen binaries not rebuilt. One commit/push,STOP.
C-013/C-016 and other parked tasks not started. Original metric report preserved as history.

## Historical checkpoint: C-014 initial metric verdict (superseded by C-017)

Owner latest scope overrides mailbox: old queued tasks cancelled; no C013/C016 or auto-loop.
C011994aeabc pushed unchanged. C014 code6147cf8a/3bda4479 compiled + guard unit PASS.
Exactly one15bot660s run, observed660.67s, completed/no engine errors: spawn195never used,
guard0events,pinned0%,traps0,stalls104 vs111 baseline. **revives.frozen1 at153**, so
acceptance0frozen FAIL. Both new defaults heldOFF; no defaultON promotion or extra run.
Owner memoryC60C1D76...813D6 restored; all own QA stopped. One final C014 push thenSTOP.
[Full result/commands/metrics](Experiments/c014-2026-10-24.md). Next work needs owner command.

## Current: local data, C-006 complete, MOVE TRACE ready

Latest matched game source **8b9236f4**, MSVC x64 Release experimental client/server PASS;
artifacts build/play-8b9236f4/. wb_move_trace defaults0, frame-latched human/bot records,
fixed250ms grid. Unit tests PASS; OFF60s zero rows; corrected ON60s1373 rows,3.9987Hz.
Human/local-client trace still needs owner recording; no GUI launched in this QA step.
[Schema, limits, launch and metrics](Experiments/move-trace-2026-10-24.md).
Owner recording launcher:Tools/Play-WarsBotsRecording.ps1 (6Normal,DX9,displayInfo1,
B/C/fullhotspot0,recovery1,physicsdiag1,trace1 both sides; refuses existing sessions).
Logs:C:\Users\Magic8\AppData\Local\WarsBots\Logs; memory/exports:sibling Memory.
173 files migrated with hashes; ordinary Crysis profile untouched. Old frozen builds use
old profile paths; current QA rejects them. QA ports46040/record46050 avoid MSI64140.
C-002 NOT fixed:native selector+Spawned+UpdateAreas10 valid body+5 samples pinned195.
C-006 PASS40pairs/80directions; all first blockedclearance h0.5; raw/counts committed.
Graph arrays unchanged; no graph repair/speed changes. B parked OFF perC-009, C OFF;
refactor deferredC-010. All own QA processes stopped, local memory restoredD2E84331...9418.
Next: owner trace/video and native body/contact diagnosis, then reviewed graph proposal.

## Historical reboot checkpoint: all QA stopped, 2026-10-24

Owner requested reboot. All own servers/runners stopped; original map memory restored.
Last built sourcef9cb6599; matched artifacts build/play-f9cb6599/. No GUI/client started.
C-001 conservative projection built/unit PASS, paired2x180s/arm15bots complete;
behavioral acceptance FAIL/unproven: main pickups32.1% B0 vs26.7% B1, timeouts162 vs165.
B/C remain OFF. C-002 zero-offset test interrupted154.1s:9 complete freezes/9, last
window incomplete; not PASS. Legacy/native/hooks still pending. No graph changes.
[Resume instructions/evidence](Experiments/steelmill-mailbox-checkpoint-2026-10-24.md).
**After reboot:** fetch research mailbox first, continue C-001/C-002; C-006 when published
is diagnostics only (bidirectional wb_nav_explain), no graph fixes yet. Human D-003 waits.

## Current: explicit local reproduction commands and automatic Triage

Commands in **b9ca22d3**, matched MSVC x64 Release experimental client/server PASS.
`wb_revive_at 195 [botNumber|me]` uses native kill/revive at registered spawn;
`wb_teleport botNumber x y z` relocates owned live bot with native broadcast.
Local loopback LAN IA gate required; manual only. Stopped/running AI state preserved.
60s command smoke reproduces spawn195 at+5s; closed gate/invalid inputs refuse.
22 Triage tests PASS; updated QA runner6bots60s PASS, automatic summary.json created.
Summary adapter updated to Claude31e9394b: warsbots-run-summary/1 per run.
23 Triage tests and saved6/15-bot log replays PASS; no game/build rerun.
Analyze stays steelmill-triage/1 for HTML. Labels encode scenario; no cross-scenario verdict.
QA stopped and memory restored. Human me/client replication still need live check.
[Commands, caveats and evidence](Experiments/steelmill-debug-commands-2026-10-24.md).
B/C remain default OFF after prior pickup regression; no speed/graph/PS changes.

## Current: native body diagnostics and gated SCC experiments

Latest built game source **ca198a42**, MSVC x64 Release experimental client/server PASS.
A baseline15bots660.52s and B/C/D15bots660.01s soak PASS for stability; focused6bots60s
native ray explain smoke PASS. Pure physics schedule/SCC/watchdog tests PASS.
**Behavioral promotion withheld:** pickups98->42, stalls111->142 despite fewer
fallbacks1904->1510 (one RNG-unfixed run/source). New flags remain default OFF.
Spawn195 native revive defect confirmed, not fixed; no teleports or speed changes.
wbnav/1 and native graph arrays unchanged. QA stopped; memory restored01F4C7F4...668AE.
Matched artifacts `build/play-ca198a42/`; installed EXEs untouched; no GUI launched.
[Report, commands and next isolation test](Experiments/steelmill-body-scc-2026-10-24.md),
[metrics/hashes](Experiments/steelmill-body-scc-2026-10-24.json).
Next: isolate weapon vs hotspot filtering/C, investigate native revive contact,
then human FP/TP check. Do not promote B/C or change speed from these results.
Research21c4e075 read without merging. Owner recovery6 impressions still pending.

## Current: recovery and full-hotspot soak passed

Latest built game source **d4b0854c**, MSVC x64 Release experimental client+server PASS.
6-bot recovery soak659.24s PASS; final15-bot recovery/full-hotspot soak659.96s PASS.
Final summed bot Update median0.243ms/frame, worst single Update10.7ms; not total CPU.
Two graph exports validated/matching. F1 at spawn195 detected despite blocked0/grounded0;
native self-kill after45.02s and native same-actor respawn confirmed. F1 root cause remains.
All runtime feature defaults remainOFF. No speed/ordinary-player/PS changes.
[Report/commands](Experiments/steelmill-recovery-2026-10-24.md),
[metrics/hashes](Experiments/steelmill-recovery-2026-10-24.json).

Owner finished6-Normal playtest (883.55s). Server147120 stopped with explicit permission,
final logs archived: `build/owner-playtests/recovery6-20261024-045803/final/`.
Owner impressions pending. Three recovery suicides and same-actor~5s revives in logs;
visual recovery/overall naturalness not claimed. QA servers stopped; original map memory
restored (01F4C7F4...668AE). Client was not forcibly closed; check processes before QA.
Next: bounded native physics diagnostic/controlled F1 reproduction and human spawn195
comparison; then native gait/sprint investigation. Native stand max4.5/pseudo1,2,3 confirmed
in source; bot speed caps not changed. Research442296db read without merging.

## Current P1: native bot spectator, 2026-10-24

Latest built game source **`a6ee9e9e`**, feature `aeda53df`; MSVC x64 Release
experimental client+server PASS. Frozen matched EXEs/manual launchers:
`build/play-a6ee9e9e/` (port64150, no auto bots); installed game EXEs untouched.
Native spectator chooses owned local IA bots; `wb_watch list|1..16|next|prev|status|off`
requires an existing real spectator, native F1 toggles FP. FP view uses replicated aim
and native smoothing. `wb_watch_info` prints followed bot decisions on server only.
No fake channels, changed network layout, PS changes or research merge.

Frozen `8f0abd5f`: 4 completed 120s headless regressions (6/15 bots x2), no crash/script
errors or exact duplicate shot callbacks. 3158 native shots; 141 unclassified callbacks
excluded from cadence. SOCOM min0.24s, contiguous native clip changes1. Median bot
Update sum0.0765–0.0825ms/frame (6), 0.2325–0.236ms/frame (15); not total server CPU.
Exporter lifecycle/gap defect fixed in `2a2ac39d`; 11 Python tests pass.
Spectator C++ tests PASS; 60s headless native Lua/C++ queries PASS: 31 assertions over
0/1/3/6/5 participants, including deferred removal. Failed QA pilot preserved separately.
See [experiment, commands and live checklist](Experiments/regression-spectator-2026-10-24.md)
and [metrics/hashes](Experiments/regression-spectator-2026-10-24.json).

Automated QA servers stopped and original QA memory restored before this live session.
### Owner spectator playtest started, 2026-10-24 04:15 MSK

Matched source `a6ee9e9e`: Steelmill, **6 Normal** bots, loopback port64150.
Server PID135448, client PID54148. Client log confirms map loaded; server telemetry
reports clients=1. **Historical session: stopped with explicit permission; final logs archived in its `final/` directory.**
Session metadata: `build/owner-playtests/spectator6-20261024-041505/session.json`.
Profile logs: `WarsBots-Spectator6-20261024-041505-{Server,Client}.log` and server
`.bots.log`. Owner positively confirmed spectator/FP camera. HUD and complete network/
lifecycle coverage remain unverified. [Feedback and triage](Experiments/owner-spectator6-2026-10-24.md):
bot1 stationary254.3s after revive (requested movement, actual speed0, grounded0),
random travel Strength jumps, stairs/drop/ladder complaints. Next priority: physical
movement after revive, then purposeful traversal; preserve working spectator.


**Pending:** broader FP/TP/HUD validation, real-observer switching, death/respawn/removal/map
and late second client; default-OFF build not rerun. Do not claim client replication PASS.
Read Claude research `47ccfacf` without merging. Next priority: matched live spectator
validation and reproduced fixes, then weapon/range naturalness. Main remains untouched.


## Current P0: native 1v1 and aim candidate, 2026-10-24

Built/tested game source **`8f0abd5f`**. Fixed redundant local NetShoot only for owned
channel-0 server bots: native Shoot already created the projectile/consumed ammo;
replay consumed again and cleared native shot cooldown. Real-client broadcast stays.
Aim now tracks perceived points with finite lag/error floor; rare visible head-zone
choice is separate from precision. No damage/health/energy/ShotValidator changes.

BUILD PASS (MSVC x64 Release experimental server+client), UNIT TEST PASS (C++ fire,
profile/aim checks + 10 Python tests), SERVER QA PASS for 3+3 SOCOM 1v1 repeats,
1 mixed-weapon regression and diagnostic pilot. SOCOM minimum .24 s / median .25 s,
one-round decrements; SCAR min .09 s / median .10 s. First-duel impact-TTK median
3.53 ->3.74 s; observed fire-to-death3.56 ->4.76 s. Native head hit confirmed later
in a candidate run. Small sample/first-contact Armor differences prohibit a general
AI-superiority or pure aim-only causal claim. See [evidence](Experiments/duel-aim-2026-10-24.md)
and [JSON](Experiments/duel-aim-2026-10-24.json).

Old owner server stopped with explicit permission; full logs archived. All QA servers
ended, memory restored (hash in evidence). No client launched, active EXE replaced,
PS gate opened or ML added. Default-OFF not rebuilt; human/visual/replication and
6/15-bot scaling regression was pending at this historical checkpoint (completed above). Frozen pair `build/play-8f0abd5f/`,
prepared manual launchers (port64150, no auto bots, SOCOM QA fixture OFF).
Previous next step: human naturalness/replication and further weapon/range checks, then spectator; superseded above.
P0 remains open for full validation; previous blocked candidate was `2fcb4c0f`.

## Power Struggle planning, 2026-10-24

[PS design](POWER_STRUGGLE_DESIGN.md) records source evidence, proposed IBotGameMode /
PSCommander, channel-0 blockers and staged native-rule validation. Documentation only:
no PS gate opened, runtime code or playable source changed. Steelmill core remains
priority. Owner playtest launched with 11 Elite on source `5e8daa65`; client loaded
Steelmill and server observed clients=1. Owner finished; feedback archived in [Elite playtest](Experiments/owner-elite11-2026-10-24.md).
Old server now stopped with explicit owner permission; final logs archived. No skill-superiority claim.
Previous 6-Normal logs archived locally in `build/owner-playtests/normal6-20261023/`.
Counts differ (6 Normal vs 11 Elite), so this is not a controlled skill comparison.

## Previous checkpoint: tactical profiles and target utility (2026-10-23)

Branch [`warsbots/feature/weapon-goals`](https://github.com/Madd3st/crymp-client/tree/warsbots/feature/weapon-goals).
Latest built game source **`5e8daa65`**, MSVC 19.44 x64 Release experimental
client+server **BUILD PASS**. Baseline `eea973f0` preserved; `warsbots/main` unchanged.
Actual checkout: `C:\Users\Magic8\WarsBotsBuild\crymp-multibot` (outside OneDrive).

Expanded existing BotProfile with weapon-aware distance, different position-change
rhythms, own-health withdrawal and native Armor/Speed decisions. Skill remains
separate from style. FindEnemy uses visible-target utility, recent actual HP-damage
shooter, minimum hold and switch margin; hidden positions only use expiring memory.
Reaction and burst cooldown guards prevent instant retarget shots. No new Player,
GameRules, ShotValidator, physics, damage, energy or network/channel modifications.

26 completed headless Steelmill runs, each 120 s, 6/15 bots, restored identical map
memory, verified EXE SHA/roster and paired initial spawn positions. No crash/Lua-error
markers in completed logs. Core comparison `eea973f0` -> `2714d460`, two repeats:
- 6 mixed: kills 10 -> 15, effective HP damage 1820 -> 1874.
- 15 mixed: kills 85 -> 82, HP 10861 -> 10289; median reported native bot-update sum
  .172-.183 -> .208-.210 ms/frame (not whole-engine CPU).
- 6 Universal: kills 18 -> 17, HP 2409 -> 2057. No overall superiority/tier ranking proved.

Measured actual movement, including stationary/loop windows. Broad progress recovery
regressed Universal combat (17 -> 11 kills) despite 21/32 physical recoveries; rejected.
Final combat-only `wb_progress_experiment` defaults **0**. Latest default 15-bot smoke:
43 kills, gate 0. Two explicit gate-1 6-bot smokes: 8+8 kills, **zero escape triggers**;
recovery efficacy remains unproved. No blanket missing-A* rejection restored.

Standalone MSVC profile/utility/progress assertions and four Python exporter tests pass.
Bounded schema-2 combat episodes: max 10,000 / 8 MiB, real HP loss (no overkill),
perceived-distance/missing-context masks. Row and real-log byte limits checked.
No ML/online adaptation. Default-OFF build not rebuilt; GUI/client visuals, two real
clients, leak checks and bullet accuracy not checked this increment. Shared inherited
perception remains omnidirectional LOS, not a FOV/hearing model.

Full evidence, hashes, methods, commands and limitations:
[experiment](Experiments/tactics-2026-10-23.md), [JSON](Experiments/tactics-2026-10-23.json).
All owned QA servers ended; map memory restored; no user game interrupted.
Next: reproducible visible-chase stall, balanced four-rotation same-binary gate 0/1
comparison; then active weapon-pursuit timeout and route projection diagnostics.

### Historical baseline artifacts (current candidate above)

Frozen matching pair: `build/play-5e8daa65/`, source `5e8daa65`, hashes in
`claude-build.json` and the experiment. Prepared `Play-Server.cmd` / `Play-Client.cmd`
use original Crysis at `E:\Steam\steamapps\common\Crysis`, port 64150. NOT launched.
No bots auto-spawn; use `wb_add 1 elite assault`, `wb_add 1 elite marksman`,
`wb_list`, `wb_remove_all`. Keep `wb_progress_experiment 0` for the owner test.
Check reactions after cover, style-specific distance, Armor/Speed/retreat, walls,
Steelmill stalls, respawn and Tab. Visual conclusions remain pending owner feedback.

Do NOT use the old default ClaudeQA build runner: it fetches multibot and replaces
outputs. This task used `Tools/Build-WarsBots.ps1 -Experimental -BuildDirectory
build/multibot-on -Jobs 6`, runtime output `build/tactics-bin`. Use the feature branch
and expected source/EXE hashes. Later docs/tool commits do not change game source SHA.

Automated repeat runner: `Tools/Test-WarsBotsTactics.ps1`; it never builds/fetches and
refuses existing servers. Archived campaigns used rotation step 2; new default step 1
with four repeats balances every slot. Full logs/datasets stay in ignored
`build/tactics-*` and `build/progress-*`; public summary includes hashes and per-skill
metrics. One cancelled invalid preflight excluded. No user session was stopped.

## Previous handoff (historical; current checkpoint above takes precedence)

## 0. Current state in one screen (updated 2026-10-23 19:25 MSK — read this first)

Branch `warsbots/feature/multibot`, HEAD `df3ceb8e` (pushed). `warsbots/main` = `caf1a815` untouched.

| Commit | What | Verification |
|---|---|---|
| `8f56ede9` | multi-bot checkpoint: registry, targeting, profiles, IA-reset restore, fall fix | BUILD PASS; M3/M4 SERVER-LOG PASS |
| `6a30130a` | movement: other floors, wider avoidance, 3 m drops, fire while closing | BUILD PASS |
| `ef376469` | BotNav 3 m grid graph + A*, head aim over cover, step out when barrel blocked | built in later commits |
| `c4c9cec2` | equip like a human on (re)spawn (IA EquipPlayer + 5 s invulnerability), weapon upgrades, Speed travel, travel/dodge Strength jumps, per-map memory, `wb_log` levels | BUILD PASS; QA 6 bots 7 kills/4 min; **15 bots 76 kills/5 min, 0.31 ms/frame, worst 0.86 ms**, 0 fall deaths; map memory loaded+saved across runs |
| `ef074f8c` | trace file `<log>.bots.log` (engine file-only channel dropped telemetry) | BUILD PASS; QA 6 bots full telemetry: **blocked movement 0%** (was 60%), 87 sprint starts, all respawns armed; but 57% of samples MOVING_TO_WEAPON, only 3 Strength jumps |
| `48155dfe` | pickup reach 2 m + 2.2 m height, 10 s give-up + 90 s skip list; travel jump at energy >130 | built as part of HEAD |
| `94d50f43`…`df3ceb8e` | build infra: worktree+build outside OneDrive, manifest + EXE reuse, QA SHA guard, explain dry run | **HEAD BUILD PASS** (first build in `%USERPROFILE%\WarsBotsBuild\crymp-multibot`); QA job `j13` (6 and 15 bots) was running at handoff — **result not read yet** |

Owner playtest (18:30) ran a stale build (`8f56ede9`): AFK bots, slow, no sprint, few jumps, lazy pickups,
console spam — all addressed after `8f56ede9` but **not yet re-played by the owner**.

### Next agent: do these first
1. Read `ClaudeQA\outbox\j13-df3ceb8e.json` and `build\claude-qa\*m4g*.bots.log`, `*m15b*.bots.log` on the
   owner's PC; run `Tools/wb_log_report.py` on the `.bots.log` files. Check: MOVING_TO_WEAPON share
   (should drop far below 57%), Strength jumps count, blocked share stays ~0%, frame cost at 15 bots.
2. Read `build\claude-multibot-explain-on.log` after the next code change: it should show only bot
   objects rebuilding. If ThirdParty still rebuilds, investigate further (OneDrive hypothesis unproven).
3. Ask the owner to play the HEAD build (`ClaudeQA\Play-Server.bat` → `wb_maxbots 15`, `wb_add 15`;
   `ClaudeQA\Play-Client.bat`) and report AFK/sprint/jumps/pickups/stuck spots.
4. Then: jump links in BotNav (Strength jump to roofs), cover selection, target switching, team modes.

### How the automation works (owner's Windows PC)
- `ClaudeQA\Claude-Runner.bat` must be running (owner starts it; window shows every step;
  `ClaudeQA\runner-console.log`). Jobs: write `ClaudeQA\inbox\<id>.json`
  `{"id":"jNN-<sha8>","build":"on|none","expect":"<sha8>","qa":[{"bots":6,"seconds":240,"tag":"x"}]}`;
  result in `ClaudeQA\outbox\<id>.json`. Scripts in repo `Tools/ClaudeQA` (copy to `ClaudeQA`).
- Build fetches from GitHub, worktree is **detached** at the exact SHA, reuse via `claude-build.json`.
- Logs: engine log + `<name>.bots.log` in `Documents\My Games\Crysis` (QA copies to `build\claude-qa`).
- Map memory: `Documents\My Games\Crysis\WarsBots\Multiplayer_IA_Steelmill.mem` (delete to relearn).
- Push from the cloud works (Claude GitHub App installed). Computer use: Explorer/cmd are click-only.
- Old worktree `local\crymp-multibot` (OneDrive) still exists and holds branch `warsbots/feature/multibot`;
  safe to delete with the owner's permission.

## 1. Verification vocabulary (what PASS means here)

| Level | Meaning |
|---|---|
| BUILD PASS | MSVC 19.44 x64 Release, Ninja, `Tools/Build-WarsBots.ps1` on the owner's PC, zero errors |
| SERVER-LOG PASS | headless loopback IA server (port 64120, **no client**) log shows the event chain; parsed by `Tools/wb_log_report.py` |
| CLIENT/VISUAL | a real client watched it — owner playtest of `6a30130a` only (see §0) |
| USER | owner confirmed in play — `6a30130a`: kills well; navigation and cover behaviour insufficient |

A SERVER-LOG PASS proves server simulation, native damage/kill/score paths and lifecycle.
It does not prove replication to clients, animation, or "plays well".

## 2. Milestones

| | What | Status | Evidence |
|---|---|---|---|
| M1 | build ON + OFF | **BUILD PASS** at `ce36de5b`; ON re-built at `8e26fb32`, `49f2bae2`, `7572a4f5`, `8f56ede9`, `6a30130a`; `ef376469` pending | runner `build-on: PASS` |
| M2 | one bot = old behaviour | not re-run in isolation; covered by M3 (same code path, Universal/Normal = caf1a815 constants) | — |
| M3 | 2 bots, no human, fight | **SERVER-LOG PASS** twice (`49f2bae2`, `7572a4f5`) | see §5 |
| M4 | 6 bots | **SERVER-LOG PASS** (`7572a4f5`, partial 240/300 s read) | STATUS "M3b and M4" |
| M5 | remove/re-add/map change | partial: native IA reset → remove → restore PASS; `wb_remove`/`wb_remove_all` mid-match NOT RUN | — |
| M6 | human + bots, 2 clients | human + bots: owner played `6a30130a` (qualitative, §0); 2 clients NOT RUN | owner |

## 3. Architecture after this series

```
CGame::m_botManager : WarsBots::BotManager          (only when WARSBOTS_EXPERIMENTAL_SPAWNER)
  ILevelSystemListener / IEntitySystemSink / IGameFrameworkListener, console cmds + cvars
  unordered_map<EntityId, unique_ptr<BotInstance>> m_bots
  BotInstance                      one bot: identity, number 1..16, name WarsBot_NN, BotProfile,
    Spawn/Remove/Revive/OnEntityDone, per-bot profiling, owned-projectile check
    BotBrain                       FSM (SEEKING_WEAPON → SEARCHING_ENEMY → CHASING → ATTACKING → DEAD/RESPAWNING)
      BotBlackboard, BotIntent, BotCombat, BotLocomotion(BotMovement), BotSuit
BotTargeting   Kind(actor) {Human, Bot, None}; Classify(self, other) {Enemy, Ally, Dead, Spectator, ...}
BotProfile     6 styles × 4 skills, plain parameters (incl. fireRange)
BotNav         shared per-level grid graph (3 m, ≤5 floors/column, ray-verified edges), A*; owned by BotManager
BotMatch       IA participant registration (Lua wrapper Pak/Scripts/WarsBots/InstantAction.lua)
CryMPEngineAdapter  loopback-IA gate, ActorSystem::CreateActor(channel 0, name)
```

### BotManager (replaces BotSpawner, deleted)
- `Owns(id)` / `Find(id)` / `AIEnabled(id)` are the only questions engine hooks ask.
- During `BotInstance::Spawn` the instance is exposed through `m_spawning` so `BotMatch::Register`
  and `CPlayer::Init` (`IsCreatingActor(name)`) recognise it before it is in the map.
- `OnPostUpdate`: restore pending roster → update every bot in number order → `Sweep()` expired.
- `ENTITY_EVENT_DONE` never erases (it can fire inside our own `RemoveEntity`). If the entity was
  **not** removed by us (`BotInstance::Removing()==false`) and the level is live, the bot is added
  to a roster and re-created next frame with the same number/profile — see §6 "IA restart".
- `eAE_resetBegin` also snapshots the roster; level end / disconnect clear everything.
- `ChooseSpawnLocation(self, avoid)`: native `IsSpawnLocationSafe` (other Player bodies, bots
  included) + `TestSpawnLocationWithEnvironment`, per-bot salted start index.
- Commands: `wb_add [n] [skill] [style]`, `wb_remove <number>`, `wb_remove_all`, `wb_list`,
  `wb_status`, `wb_debug 0|1`, `wb_select <number>`; cvars `wb_maxbots` (6, hard 16), `wb_skill`,
  `wb_probe_local_test` (gate, 0), `wb_bot_cloak_experimental` (0). Legacy `wb_bot_*`/`wb_probe_*`
  act on the selected bot.

### BotInstance
Holds EntityId + `const IEntity*` identity guard (compared, never dereferenced). `Revive` uses the
shared spawn chooser and native `CGameRules::RevivePlayer`. Profiling: wall ms per Update.

### Targeting (BotTargeting)
Old `Enemy()` required `GetChannelId()!=0` + live `INetChannel` → bots could never target bots.
Now: Human = Player with a live server channel; Bot = owned by the registry; then self / spectator /
dead / hidden-or-in-vehicle are excluded; same non-zero `CGameRules::GetTeam` → Ally; else Enemy.
IA has team 0 → free-for-all. Perception unchanged: 80 m, LOS rays eyes→chest and eyes→eyes, muzzle
ray before firing; no wallhack.

### Brain changes
Per-bot `Configure(profile, number)`: golden-ratio phase for aim noise, staggered 10 Hz decision tick,
strafe side/period offsets, patrol order. Ranges, reaction, aim error, burst/gap from BotProfile.
`SEARCHING_ENEMY` roams between IA spawn points (public map knowledge) instead of 6–10 m local legs;
1.5 s blocked → one local detour leg. Falling faster than 3 m/s → Defense (Armor) even in freefall;
moving Strength jumps also probe the far touchdown (5.5 m, refuse drops > 2.5 m).

### GameRules / ShotValidator / Player / serialization
Only call-site changes: `GetSpawner().Owns(...)` → `GetBotManager().Owns(...)`, `TraceHit/TraceKill`
through the manager, `IsOwnedProjectileHit` through the manager. Network serialization
(`WarsBotMotion`, `WarsBotSuitActive` optional groups, `warsBotActor` spawn info) is **unchanged**;
`CPlayer::Init` marks a bot by `IsCreatingActor(entityName)` instead of the fixed name `WarsBot_01`.
GameRules participant APIs from the IA WIP (`IsMatchParticipant`, `GetMatchParticipants`,
`GetMatchParticipantCount`, `IsPlayerInGame`) are unchanged.

### IA integration (from WIP, fixed)
`BotMatch::Register`: `Script::CallMethod(hooks, "Attach", rules)` and
`Script::CallMethod(rules, "ResetScore", id)` (was nonexistent `Script::Call` overload → C2660).
Flag `warsBotParticipant` is set only after ResetScore succeeds.

### Navigation (`ef376469`, untested)
`BotNav` is built lazily once bots exist, 2 ms/frame (`Begin/Step` from `BotManager::OnPostUpdate`),
over the spawn-point bounding box + 40 m, z −15/+35 m. Nodes: ray down repeatedly per 3 m column,
floor normal z>0.7, 1.9 m headroom, not under ocean level. Edges to 8 neighbour columns: rise ≤1.9 m,
drop ≤3.2 m, body rays at 0.5/1.2/1.75 m, ground continuity at 4 samples (≤0.75 m step up).
Kept across IA resets; reset on level change/disconnect/shutdown. `BotBrain::Steer` follows
A* paths (repath on goal move >4 m, every 5 s, after 1.5 s blocked; min 0.75 s between queries);
straight-line fallback before `NAV ready`. Risks: 3 m cell can miss narrow doors; ray-only edges ignore
dynamic objects; no jump/strength-jump links yet.

### Cover behaviour (`ef376469`, untested)
`BotCombat::Observe` aims at the head when the chest is hidden from the eyes; `Attack` tries the
muzzle ray to chest then head and sets `ShotBlocked()`. In ATTACKING the brain then steps
forward/sideways after 0.4 s and flips side after 2.5 s.

## 4. Invariants (do not break)
No fake INetChannel / OnClientConnect(0); global `IsPlayer()` unchanged; ShotValidator accepts only
an owned bot's real non-remote projectile of its own weapon; no teleport, no direct HP writes, no
infinite energy; experimental CMake OFF by default; runtime gate 0; loopback LAN InstantAction only.

## 5. Evidence

M3 `49f2bae2` (`WarsBots-Claude-m3-173247.log`, 240 s): 2 bots; native IA RestartGame removed both,
2/2 restored (new EntityIds); REAL PICKUP of map weapons after reset; 8 acquisitions `kind=bot`;
9 bot→bot hits; WarsBot_02 killed WarsBot_01 (4×31) via stock KillPlayer; IA SCORE shooter 1/0,
victim deaths +1; revives 2/2 same EntityId; avg ≈0.03 ms/frame; no crash. 3 fall self-hits (fixed next).
M3b `7572a4f5` (`m3b-174130`, 240 s): restored 2/2; 8 bot→bot hits; 1 bot→bot kill, scores 1/0 and 0/1;
**0 self-damage**; no crash.
M4 `7572a4f5` (`m4-174530`): 6 bots, 6/6 restored after IA restart, 24 bot→bot acquisitions,
61 bot→bot hits, 4 kills by three different bots, 4/4 revives, 0.077 ms/frame for all six, no crash.

Performance: per-bot update ≈0.01–0.02 ms average, worst frame ≤1–2 ms (first frames, item spawn);
≤350 rays per 10 s report for 2 bots. Not yet compared with a bot-free baseline frame time.

## 6. Known bugs / limits
- **IA restart**: bots count as participants, so PreGame start timer fires and native RestartGame
  deletes runtime actors via `GameFramework::Reset` without `eAE_resetBegin`. We re-create them
  (new EntityIds, scores reset by the native restart). Works; but it is detection-by-side-effect.
  Better: hook the IA `RestartGame`/`OnResetEntities` path explicitly.
- Each commit changes `version.h` (git describe) → most of CryGame rebuilds (~2–3 min).
- No pathfinding: straight lines + local avoidance; enclosed spots still block.
- Headshot: incoming 26 → health −1067 in an old run; hypothesis in ARCHITECTURE_REVIEW, unverified.
- Not observed on this branch: client view of N bots, Tab rendering, bot vs human, two clients,
  `wb_remove` mid-match, map change, team modes, Power Struggle.
- Cloak OFF by default. Test SCAR fallback still used before map pickups exist (pre-reset phase).

## 7. Refactoring recommendations
1. Replace reset detection with an explicit IA hook (Lua wrapper `RestartGame` → C++ notify).
2. Split BotBrain (≈420 lines) into decision (FSM) and tactics (range/strafe/suit) modules.
3. Target scans: O(N²); time-slice when N>8 after a baseline measurement.
4. Move test-ability commands out of BotBrain into a debug controller.
5. Navigation provider interface (CryAI if nav data exists on IA maps, else waypoint graph).

## 8. QA automation
`Tools/ClaudeQA/*` (copy into `<checkout>\ClaudeQA\`, write `game-dir.txt` with the Crysis root):
runner polls `inbox\*.json` → fresh `Claude-Job.ps1` per job (build on|both, qa runs, push) →
`outbox\<id>.json` with HEAD. Separate worktree `..\crymp-multibot`; never touches game-root EXEs or
the owner's port-64110 session. `Tools/wb_log_report.py <log>` → summary + M3 verdict.

## 9. Next task
1. Build `ef376469`, run job `j05` (6 and 15 bots), compare blocked share / ATTACKING share with the
   baseline in §0, check `NAV ready` node/edge counts and frame cost at 15 bots.
2. Owner playtest of the same build (navigation, cover, 15-bot load).
3. Then: jump links in BotNav (Strength jump to roofs), cover selection, target switching, damage reaction.
