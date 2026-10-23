# Agent handoff — WarsBots for CryMP (multi-bot, updated 2026-10-23 18:20 MSK)

## 0. Current state in one screen (read this first)

| Commit | What | Verification |
|---|---|---|
| `8f56ede9` | multi-bot checkpoint (registry, targeting, profiles, reset restore, fall fix) | BUILD PASS; M3/M4 SERVER-LOG PASS (§5) |
| `6a30130a` | movement/engagement: no refusal of other floors, wider avoidance fan, 3 m drops, fire while closing (fireRange 32–65 m), abandon blocked chase | BUILD PASS (owner built it); **owner playtest**: bots see and kill well, still sometimes lost/stuck, stare at a player behind cover |
| `ef376469` | BotNav grid graph + A*, head-aim over cover, step out when barrel blocked, QA for 15 bots | not built yet (owner's playtest ran the stale `8f56ede9`, see below) |
| `ef074f8c` | trace file | QA 6 bots with full telemetry: **blocked movement 0%** (was 60%/40%), 87 sprint starts / 86 Speed switches, 16/16 respawns equipped, 75 bot→bot hits, 5 kills, 0.11 ms/frame; but **57% of samples MOVING_TO_WEAPON** (bots stood under unreachable items) and only 3 Strength jumps → fixed in the next commit |
| `c4c9cec2` | first BUILD PASS of the gameplay series below | QA: 6 bots 7 kills/4 min, 0.11 ms/frame; **15 bots 76 kills/5 min, 0.31 ms/frame avg, worst 0.86 ms**, 0 fall deaths, map memory loaded + saved across runs |
| HEAD (this commit) | equip like a human on spawn/revive (IA EquipPlayer + 5 s invulnerability), weapon upgrades, Speed travel ("Shift"), travel and dodge Strength jumps, map memory (`%USER%/WarsBots/<level>.mem`: stuck spots, kill/death hotspots), console log levels (`wb_log`) | **NOT BUILT, NOT RUN** |

**Owner playtest 18:30, 15 bots — it ran `8f56ede9`, not the newer code** (stale bundle on the PC;
the build script now fetches from GitHub and refuses a wrong SHA). Findings: some bots AFK, slow,
never sprint, rarely jump, lazy weapon pickup; console spam. Log (`WarsBots-Play-Server.log`):
15 bots avg 1.74 ms/frame, worst 17.8 ms; 58 bot kills; movement blocked 40% of samples; ~25% of
samples with no weapon (cause: C++ revive skipped IA `EquipPlayer`, bots respawned empty-handed);
sprint almost never executed (roaming always requested Run). AFK = chasing a target on another
floor (refused by the old `Move`, fixed in `6a30130a`).

Baseline to beat (M4 on `7572a4f5`, `Tools/wb_log_report.py`): movement blocked 60% of samples,
state shares CHASING 52% / SEARCHING 38% / ATTACKING 2%. Owner wants 10–15 bots next
(`wb_maxbots 15`, hard limit 16). The owner tests visuals himself; automated QA covers the rest.


Branch under review: `warsbots/feature/multibot` (fork `Madd3st/crymp-client`).
Base: `warsbots/feature/ia-participants` WIP `6718af16` → `6a8c241c` (C2660 fix) → multi-bot series.
`warsbots/main` (`caf1a815`) is untouched; nothing here is merged. Upstream is never pushed.

Read with this file: [STATUS](STATUS.md) (evidence log), [AI_TESTS](AI_TESTS.md) (how to run),
[MULTIBOT_DESIGN](MULTIBOT_DESIGN.md), [ARCHITECTURE_REVIEW](ARCHITECTURE_REVIEW.md),
[COMBAT_REFERENCE](COMBAT_REFERENCE.md), [AI_LEARNING_FEASIBILITY](AI_LEARNING_FEASIBILITY.md).

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
