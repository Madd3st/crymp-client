# WarsBots for CryMP: status, 2026-10-24

## STOP checkpoint: C-018 paired pace QA complete

Game source ef25395e; wb_bot_pace default0. Four15bot660s paired runs completed
661.49/661.73/661.55/661.30s. Same roster/memory/EXEs/flags, trace1, C014defaultsON.
All4 frozen0,traps0,selfkills0; all8 graphs identical. Owner memory restored,
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

## Historical checkpoints below (superseded by current checkpoint)

Repository: https://github.com/Madd3st/crymp-client.
Integration `warsbots/main` stays at `caf1a815`; see current feature checkpoint above.
Local checkout: `<checkout>`.
See [port provenance](WARSBOTS_PORT.md), [tests](AI_TESTS.md), [handoff](AGENT_HANDOFF.md).

## Implemented

Latest Wars abilities/Combat MVP migrated into CryGame/Bots, with scoped CryMP
Player/suit/network/GameRules/validator integration and a local engine adapter.
Default OFF; explicit loopback IA opt-in; one owned actor, no fabricated channel.
Native CryMP code is preserved outside bot scopes. No new binary patches/CryAI controller.
Independent move/look strafe, moving combat and native Speed chase selection are implemented.
User confirmed visible left/right combat strafe in the new build.

## Actually checked on CryMP

- Clean upstream MSVC 19.44 x64 Release client/server BUILD PASS; Resource Compiler
  requires `/c65001` for this Cyrillic checkout. Default-OFF port also BUILD PASS
  and loaded IA Steelmill, listening on 127.0.0.1:64110, without bot commands.
- Experimental client/server BUILD PASS. Initial map crash was an integration bug:
  the define only reached CryGame while Base allocated a smaller CGame. Fixed by
  publishing the define from Base to all consumers. No native DLL patch was needed.
- Fixed build loaded Steelmill and explicitly created Player `WarsBot_01`, server
  EntityId=65501, BindToNetwork=true (required by successful CActor::Init).
- Physical movement 2.50 m/s and native test SCAR fallback observed. This run did
  not prove map pickup; logs distinguish fallback from real pickup.
- One real client joined late; motion/suit/jump replica logs received. User confirmed
  visible bot, movement and shooting. Server recorded actual bot shots damaging
  and killing player via GameRules::KillPlayer.
- Player Nomad killed the bot; death and native Revive result sameEntity=1,
  alive=1, health=100 recorded, followed by another SCAR fallback.
- Strength takeoff vz=9.60, energy about130.3, rise=3.62m and landing recorded;
  client received strength jump sequence. Repeatability fraction is NOT established.
- User observed a one-headshot death. Log: incoming damage26, health100 -> -1067
  after Lua. Damage multipliers/health were not changed; IA parity needs investigation.

Experimental artifacts in `build/baseline/` and installed in original Crysis root:
`<CrysisRoot>` (CrySystem 6156). The build directory name is
historical: CMake cache there is currently experimental ON, not the original baseline.
SHA256 client `B29621EF5565480630D0BE66F371EE1EBF1AB375647EDFF0EE1BE59885D10640`;
server `2C71B62D567D045D7FD7575B2403C0EF71B8B7FEE46556F73D7B1EE4E513F6F3`.

Logs are under `<Documents>\My Games\Crysis`, not game root:
`WarsBots-CryMP-Port-Fixed.log`, `WarsBots-CryMP-Port-Client.log`.
Local build logs: `build/baseline-build.log` (first RC failure),
`baseline-retry.log` (clean PASS), `default-off-build.log`, `port-abi-build.log`.
Full machine logs and game binaries are deliberately not committed.

## Still unproven / next

Manual remove/recreate/map cleanup, two simultaneous clients/reconnect, visual
respawn/jump animation and repeatable normal/Strength/Speed tests remain open.
Strafe now passes physical/visual checks; chase against a fleeing player still needs a specific trial. Channel0/IsPlayer=false
does not make a match participant, IA score/headshot parity is unverified, PS untouched.
No full navigation, multiple bots or server-only stock-client compatibility claim.
Do not replace these CryMP results with older Crysis Wars validation or vice versa.

## Locomotion follow-up

Base Combat MVP saved as `1fe4be11` and integrated in warsbots/main. New runtime is
launched from build/baseline EXEs with `-dir "<CrysisRoot>"`.
Game-root EXEs stayed file-locked after process exit; do not assume they were updated.
Active test server/client IDs are in build/locomotion-*-pid.txt; never stop user gameplay
without agreement. Current build hashes (different from MVP above):
client `6037CC4747884698984B0E0FFC6A89BD30EC17736D9018BAC4030A0DBC5AD3CD`;
server `F93293507298B76A4CA01F9EC0E43E7B500C07471660F9571989067F72BC260F`.

Separate QA servers (64120), known open point (1242,1351,136), no clients:
left strafe 2/2; right strafe 2/2; sprint 2/2; normal jump 1/1; Strength 2/2.
Strafe measured actual 2.50, lateral about +/-2.45, facingDot=1.00.
Sprint actual5.46, sprint=1, mode0, energy132.1/132.2. Strength vz9.60,
rise3.62/3.63, takeoff energy130.4/130.3, landing displacementZ=0.
A preceding right-strafe test at the map's enclosed default spawn failed (0/1):
local avoidance redirected movement forward. The first QA attempt was a harness
file-sharing error, not a gameplay result. Preserve these limitations.
Logs: build/server-qa; helper Tools/Test-WarsBotsServer.ps1. These physical trials
are not a complete route/chase or client-animation success rate.

One second client process joined late and received a replica; first had System:Quit
and was no longer running. TWO SIMULTANEOUS CLIENTS remain unconfirmed.
User reported losing sight after a kill: native respawn moved the actor to another
map spawn; server/client logs continue alive movement. No full pathfinder exists.

Latest user priority: include the bot in the IA player table and match participation,
using EntityId and scoped game-mode handling, never a fake network channel.

## SAVE CHECKPOINT: IA participants WIP — BUILD FAIL, NOT DEPLOYED

Stopped on the user's explicit request to save remaining quota (2026-10-23).
Do not continue gameplay/rebuilds merely because the earlier long task is incomplete.
Working main remains caf1a815; do NOT merge this WIP into main yet.

Prepared: separate EntityId-based match participant APIs, HUD inclusion, Lua match
count/list binding, owned bot score reset/cleanup, instance-local IA damage/scoring
wrappers, connection-only enumeration during native ReviveAllPlayers. Native IsPlayer,
real channel list, voting denominator and ShotValidator were not globally changed.
New files BotMatch.h/.cpp and Pak/Scripts/WarsBots/InstantAction.lua; changes also in
CMake, BotSpawner, GameRules, GameRulesClientServer, ScriptBind_GameRules and HUDScore.

Compilation attempted, failed: BotMatch.cpp lines41/46, C2660 Script::Call has no
four-argument overload for table/method/argument. Fix using the actual CryMP script
call API; preserve Lua colon Attach(self,rules) argument semantics. No fix or repeat
build after the save request. Log build/ia-build.log. CMake runtime output was redirected
to build/ia-bin to avoid locked working EXEs; no IA participant EXEs were produced.
Lua runtime/HUD/scores/cleanup and ordinary-player regression are NOT RUN for this WIP.
The user still runs the proven locomotion build: scoreboard omission and old AI damage
table selection are expected until a corrected participant version is built/tested.

Next steps: fix two script-call compilation errors; build ON and OFF; test isolated IA
registration/remove/recreate, then obtain permission before replacing/restarting the
user's session. Verify Tab row, count, kills/deaths/headshots without double human credit,
normal-vs-head damage parity, death/respawn, round transitions, late join and cleanup.
Spawn groups are explicitly unsupported/refused by participant registration; PS untouched.

## 2026-10-23 (cloud session): P0 fix + multi-bot registry — NOT COMPILED

Authored in a Linux sandbox without MSVC; nothing below is BUILD PASS or tested in game.

- `6a8c241c` (on top of WIP `6718af16`): BotMatch uses `Script::CallMethod`, which pushes the
  table as `self` (`WarsBotsIA:Attach(rules)`, `rules:ResetScore(id)`). Signatures checked in
  `IScriptSystem.h`; `ResetScore(playerId)` checked in `Pak/CryMP/Scripts/GameRules/InstantAction.lua`.
  Participant flag is now set only after a successful reset.
- `a8a56583` branch `warsbots/feature/multibot`: BotSpawner removed. BotManager = EntityId
  registry + commands; BotInstance = one independent bot; BotTargeting = channel-independent
  hostility (bots target bots); BotProfile = 6 styles × 4 skills; safe spawn selection for spawn
  and revive; per-bot profiling. See [design](MULTIBOT_DESIGN.md), [review](ARCHITECTURE_REVIEW.md).
- Headshot −1067: hypothesis = stock SP AI damage table chosen before the IA wrapper; unverified.

Verification level: implemented → **not compiled** → not tested. Do not merge into warsbots/main
before Build-WarsBots ON and OFF pass and AI_TESTS sections M1–M6 are run.

### Same day, on the user's PC (driven by Claude via ClaudeQA/*.bat, separate worktree `..\crymp-multibot`)

- **BUILD PASS** MSVC 19.44 x64 Release: ON (`build/multibot-on`) and OFF (`build/multibot-off`) at `ce36de5b`;
  ON again at `8e26fb32`. The C2660 blocker is gone. ON server SHA256 at ce36de5b `FE542682…BBBA33`.
- **Server log, QA port 64120, no client, Steelmill:** `wb_add 2` → WarsBot_01 (65501, universal) and
  WarsBot_02 (65500, assault) on different spawn points ~100 m apart; both `IA participant registered`
  (matchCount 1→2), armed (test SCAR fallback), patrolling. PROFILE ≈0.02 ms/frame for 2 bots.
- Found: bots count as IA participants, so PreGame start timer fired → `RestartGame` → `ResetEntities`
  → our reset handler removed bots permanently (clean ENTITY_EVENT_DONE, no crash). Fixed in `8e26fb32`
  (roster restore). Bots never met with 10 m local patrol → spawn-point roaming in `8e26fb32`.
- A bot-only IA match therefore DOES start without humans (native timer, no fake connection).

### M3 bot-vs-bot: PASS (server log, `49f2bae2`, QA 64120, no human, 240 s)

`build/claude-qa/WarsBots-Claude-m3-173247.log` via `Tools/wb_log_report.py`:
2 bots; native IA RestartGame deleted both, both restored 2/2 with new EntityIds; real map weapon
pickups after reset; 8 mutual acquisitions `kind=bot`; 9 bot→bot hits through native weapons and
ShotValidator; WarsBot_02 killed WarsBot_01 (4×31 dmg) via stock KillPlayer; IA score shooter
kills=1, victim deaths+1; 2/2 revives of the same EntityId; ≈0.03 ms/frame for 2 bots; no crash.
Defect: 3 self-damage (fall) hits after obstacle Strength jumps off ledges, 1 death (suicide −1 kill,
native IA rule). Fixed in `7572a4f5` (Armor while falling, far-landing probe) — retest pending.
Not yet observed: headshot scoring, Tab rendering on a client, bot vs human after this refactor.

### M3b and M4: SERVER-LOG PASS (`7572a4f5`, QA 64120, no human)

- M3b `m3b-174130`, 2 bots, 240 s: restored 2/2 after IA restart, 8 bot→bot hits, 1 kill, IA scores
  1/0 and 0/1, **0 self-damage** (fall fix effective in this sample), no crash.
- M4 `m4-174530`, 6 bots (universal, assault, marksman, flanker, defender, hunter), first ~240 s of 300:
  6 distinct initial spawns (min separation 30.3 m); IA restart → 6/6 restored; 24 acquisitions all
  `kind=bot`; 61 bot→bot hits, 1351 damage; 4 kills by bots #2 (2), #3, #6; 4 deaths; 4/4 revives same
  EntityId; 1 self-hit (1 hp); PROFILE max 0.077 ms/frame for all 6 bots, worst frame 1.37 ms,
  ≤2868 rays per 10 s report; no crash.
Meaning: the server simulates 6 independent bots that find, shoot, kill and respawn each other through
native systems. Not proven: client view, animations, tactics quality, bot vs human on this branch.

### After the checkpoint (2026-10-23 evening)

- `6a30130a` movement/engagement fixes: BUILD PASS on the owner's PC. **Owner playtest**: bots now
  see and kill well; they sometimes do not know where to go and get stuck; when a player stands behind
  an obstacle they look at him but do nothing. Owner wants 10–15 bots.
- `ef376469` navigation graph + A*, head-aim over cover, barrel-blocked step-out, 15-bot QA:
  **not built, not run**. Measure against M4 baseline: blocked 60% of movement samples, ATTACKING 2%.

### Owner playtest 18:30 (15 bots) — build was the stale `8f56ede9`

Bots kill well; some stand AFK, move slowly, never sprint, rarely jump, pick up weapons lazily;
console spam on server and client. Causes found in the log: empty-handed respawns (IA EquipPlayer
was not called for bots), roaming always at Run, refused moves toward other floors (old build).
Fixed in the next commit (not built yet): equip like a human, weapon upgrades, Speed travel,
travel/dodge Strength jumps, per-map memory, `wb_log` levels (telemetry to file only).
Load: 15 bots ≈1.74 ms/frame average, worst frame 17.8 ms (old entity-iterator weapon search).

### 19:25 MSK checkpoint (`df3ceb8e`)
- 15 bots on `c4c9cec2`: 76 kills / 5 min, avg 0.31 ms/frame, worst 0.86 ms, 0 fall deaths, map memory persisted.
- 6 bots on `ef074f8c` (full telemetry): blocked movement 0%, Speed sprint used, all respawns armed;
  issues found and fixed in `48155dfe` (bots stuck under unreachable weapons 57% of samples; few jumps).
- Build infra moved out of OneDrive with EXE reuse; HEAD BUILD PASS; QA `j13` pending.
- Not re-verified by the owner since the stale-build playtest.
