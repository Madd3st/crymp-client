# WarsBots for CryMP: status, 2026-10-23

Repository: https://github.com/Madd3st/crymp-client.
Integration `warsbots/main`; working baseline `caf1a815` on `warsbots/main`; current WIP `warsbots/feature/ia-participants`.
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
