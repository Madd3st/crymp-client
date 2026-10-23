# Agent handoff — WarsBots for CryMP (multi-bot, updated 2026-10-23 18:20 MSK)

## Current checkpoint: weapon goals and routing retry cache (2026-10-23)

Working branch `warsbots/feature/weapon-goals`, based on GitHub checkpoint `b7c95e12`.
Built/tested game code `eea973f0`; `warsbots/main` and `warsbots/feature/multibot` unchanged.
Actual checkout/build: `C:\Users\Magic8\WarsBotsBuild\crymp-multibot` (outside OneDrive).

Fixed native pickup eligibility revalidation while approaching an item; bots stop chasing
hidden/taken/unpickable weapons. Failed A* paths now honor their two-second retry deadline
instead of retrying after 0.75 s. Local steering fallback stays available for sparse graph gaps.
Failed route logs record real start/goal coordinates. No channel, Player, GameRules,
ShotValidator, suit energy, damage or serialization changes in this increment. OFF remains default.

MSVC x64 Release client+server BUILD PASS. New isolated server QA: 6 bots/240 s and
15 bots/300 s, no clients, no crash markers. Revised vs j13 baseline:
- 6 bots: weapon-state share 40.34 -> 13.73%, blocked 2.72 -> 1.64%, pickups 17 -> 17,
  kills 16 -> 12. Revised worst AI frame 2.79 ms.
- 15 bots: weapon-state share 30.87 -> 28.05%, blocked 2.80 -> 0.89%, pickups 38 -> 52,
  kills 87 -> 121. Revised worst AI frame 1.32 ms; 13 unavailable goals cancelled.
These are sample shares, one run each, with evolving map memory; not a causal or general
AI-superiority claim. Strict route-rejection experiment `20bf32bf` was tested and removed
because of pickup/blocked regressions. Full evidence and limitations: [AI_TRAINING](AI_TRAINING.md)
and [JSON results](Experiments/weapon-goals-2026-10-23.json).

Added [FPS AI research](FPS_AI_RESEARCH.md), [Steelmill notes](STEELMILL_AI.md),
`Tools/wb_ai_metrics.py`. Exporter checked against known real-log counts and sample totals.
No combat policy training or bandit adaptation implemented yet. OFF not rebuilt for this delta;
client visuals, simultaneous clients, leak checks and controlled repeat batches NOT RUN.

Next: owner visual test of this exact build; then distinguish failed graph projection from
disconnected routes at logged coordinates, measure actual route progress, and add bounded
per-bot combat episode data before policy learning. No unverified Strength roof links.


### Owner visual check / exact artifacts

Use `build/multibot-on/CryMP-{Server,Client}64.exe`, manifest source SHA `eea973f0`.
Server SHA256 `3AD475D184EEBE0DA5674028F8D57885631B1C7820F1DAFE82F273835DD50CB5`;
client `881C82BADEEA275EC62A8CE21D118697FDCE9E09D05C8C1B7215B5CCC5AE6986`.
Local launcher files remain in the old checkout's `ClaudeQA/Play-Server.bat` and
`Play-Client.bat`; they reference this outside-OneDrive build, port 64130, original Crysis.
Server does not automatically spawn bots: enter `wb_maxbots 15`, then `wb_add 15` manually.
Observe stale-weapon pursuit, AFK/stuck spots, sprint/jump naturalness and rooftop routes;
check Tab and real combat as well. Do not run the old default build job to test this feature:
it fetches `warsbots/feature/multibot` and would replace these EXEs with that older branch.

All QA processes started in this increment have ended. No user game was interrupted.
Do not claim the owner visually tested this code yet. Source/build SHA stays `eea973f0`
when the later commit only changes Markdown, JSON evidence and the offline metrics script.

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
