# Agent handoff — WarsBots for CryMP (updated 2026-10-30)

## Current update: C154 Shore server route accepted

C154 current: `ed72987c` on `codex/shore-ladder-exit` (PR2), SERVER WHOLE-ROUTE PASS. Physical exit14.55s z99.76 nativeOn0/exiting0/grounded1; Strength22.25s; water30.819s; dryshore102.819s HP100/dry7.258s. Full WarsBots CI41086406959 and all PR matrix41086417565 SUCCESS; exact pair12992223176 SHA PASS; DS134 preflight and DS135 `da16fa8f` independent postrun PASS. Demo/archive and exact MSIX Memory4/Navmesh16 restoration PASS. [Result](https://github.com/Madd3st/crymp-client/blob/warsbots/feature/weapon-goals/Docs/Coordination/C154_SHORE_EXIT_RESULT.md). Client visual: OWNER_PLAYTEST_PASS (owner feedback, two grounded exits at94.68s/146.56s in interactive logs; supplied video not independently inspected). Full PS/other ladders NOT TESTED. PR2 depends on PR1, neither merged; canonical307 preserved. No game active; wake/backlog loops OFF. Future runtime requires a fresh exact gate, no silent quality repin.

Earlier 307/owner-pause text below records the earlier handoff. It is superseded by this result and current CLAUDE_STATE, not permission for autonomous backlog work.

## Current state, 2026-10-30 — read this first

Sections below are history from 2026-10-23/24. They are not the current task and their
"next" lines are not assignments.

- Only explicitly assigned work is performed. Autonomous wake-ups, `wait_mail` loops and
  self-selected backlog are off. **Agents may not launch the game or a server** until the
  owner explicitly allows it, even when no game process is running. "Owner played on
  29.10" is history, not the current PC state.
- Game code source of truth: `warsbots/feature/claude-sprint` at
  `307037bc08050d39f26310dc6cc7071dddddf4a6` (full CI `40919291234` SUCCESS, DS126
  `4c710aca` approved code/data/scenario). Working copy is detached HEAD at
  `QA/build/ds076-code`. `Code/` on QA branches is stale; do not build or review it.
- Coordination and docs: this branch (`warsbots/research/steelmill-triage`), canonical
  status in [Research/STATUS](Research/STATUS.md). QA tools/results:
  `warsbots/feature/weapon-goals`. Independent review: `warsbots/research/deepseek`
  (`4c710aca` is the review head, do not move it for background work).
- C154 (lower Shore ladder) is PAUSED_OWNER. Candidate `307037bc` is reviewed and green,
  but the **physical top exit is unverified**; four controlled attempts are kept
  (V1 invalid pack, V2 climb stall, V3/V4 upper exit blocked). After explicit release:
  fresh pins/review/process check plus a NEW live-data snapshot, then ONE combined
  lower545 -> native ladder -> physical exit/top floor -> Strength -> water -> dry shore
  run, then archive/demo/recovery and independent audit. Do not retry an unchanged
  failure and do not add a patch without a specific cause.
- C155 (classic Steelmill, IA8, exactly two bots per difficulty) is delivered on E.
  C156 recovery is done: current sources, CI pair, V5, wiki, original logs and the four
  attempts' demos are restored and hash-checked; original per-attempt Memory/Navmesh
  snapshots and some temporary receipts were NOT recovered and were not fabricated.
- Open backlog: R4 ammo-search deadline, R9 generic-jump direction contract, D032
  `first_primary` default (owner decision). DS125/DS126 are closed, not new tasks.
- Local checkout of this repo is on `warsbots/research/steelmill-triage`; the frozen
  `warsbots/feature/ia-participants` branch ends at `96a41b30` plus a docs sync commit.
  Claude's return is expected around 2026-11-01, but role changes are confirmed
  explicitly, never by date.

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
6/15-bot scaling regression remain untested. Frozen pair `build/play-8f0abd5f/`,
prepared manual launchers (port64150, no auto bots, SOCOM QA fixture OFF).
Next: human naturalness/replication and further weapon/range checks, then spectator.
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


Gemini: назначено первое архитектурное/кодовое ревью и разбор цикла разработки, включая крышу Steelmill. Постоянная роль не назначена; реализация/уборка пока не поручены. [Задание](https://github.com/Madd3st/crymp-client/blob/warsbots/research/steelmill-triage/Docs/Research/GEMINI_INITIAL_REVIEW.md). Codex проверяет выводы; владелец выбирает дальнейшие задачи.
