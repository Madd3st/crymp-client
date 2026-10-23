# CryMP bot validation

Build with MSVC x64: `Tools/Build-WarsBots.ps1 -Experimental`.
Without that switch the option defaults OFF. Output: `build/warsbots/CryMP-{Client,Server}64.exe`.
RC uses `/c65001` because the generated config includes an absolute resource path.
Copy both EXEs into original Crysis 1.2.1 root (next to Game/Bin64).
Do not use `-mod WarsBots`: CryMP embeds CryGame and rejects DLL mods.

Server, from that game root (no automatic bot command):
```text
CryMP-Server64.exe -headless -logfile WarsBots-CryMP-Server.log +sv_bind 127.0.0.1 +sv_lanonly 1 +sv_port 64110 +sv_gamerules InstantAction +map multiplayer/ia/steelmill
```
For interactive console remove `-headless`. Once map readiness is confirmed, enter:
```text
wb_probe_local_test 1
wb_bot_verbose 1
wb_bot_start
wb_bot_status
```
Client: `CryMP-Client64.exe -dx9 -logfile WarsBots-CryMP-Client.log +connect 127.0.0.1:64110`.
Enter the match as a live player. Do not terminate before connection completes.
`wb_bot_stop` cancels actions/respawn; `wb_probe_remove` removes only the owned actor.
`wb_probe_spawn x y z` is the stationary probe; `wb_probe_revive` revives a dead one.
Ability diagnostics: `wb_bot_abilities`, `wb_bot_gait walk|run|sprint|auto`,
`wb_bot_jump normal|strength [delaySeconds]`, `wb_bot_suit defense|speed|strength|auto`.
Select one alternative per command. Cloak remains explicitly isolated OFF by default.

Record A create/remove/recreate, B walk/turn/idle, C aimed left/right strafe,
D shooting while moving, E native Speed chase, F normal jump, G Strength jump with
energy/landing, H lost/reacquired target, I real damage/death/respawn, J late second client.
For C/E/G record successful attempts / total, server physical velocity and energy,
client animation and any rejection. No direct HP changes, teleports or fake channels.
Repeat cleanup/map-change checks and inspect logs for crashes/invalid actor access.
Default OFF and runtime gate closed must leave ordinary players unaffected.

Distinguish decision, executor acceptance, physical result and client display.
Current outcomes and startup errors are in STATUS; unexecuted cases are NOT RUN.

`wb_bot_strafe left|right|auto` performs a six-second aimed lateral test.
Automated physical checks on an isolated QA server: `Tools/Test-WarsBotsServer.ps1
-GameDirectory <CrysisRoot> -ServerExe <builtServer> -Case left -Trial 1
-SpawnPosition 1242,1351,136`. Also supports right, run, sprint, normal, strength.
It stops only its own no-client QA process after physical evidence or a bounded
failed test; the user's server on64110 is separate. Results are under build/server-qa.

## Multi-bot (warsbots/feature/multibot) — M1, M3, M4 SERVER-LOG PASS; M2/M5/M6 open

Same server line. After `wb_probe_local_test 1`:
```text
wb_add 1            // one autonomous bot, style by slot, skill from wb_skill
wb_add 5 veteran    // up to wb_maxbots (default 6, hard 16)
wb_list             // number, EntityId, profile, state, target
wb_status           // gate, per-bot status, PROFILE line (update ms, rays, scans)
wb_debug 1          // telemetry for all bots + PROFILE every 10 s
wb_remove 3         // by display number, NOT EntityId
wb_remove_all
wb_select 2         // legacy wb_bot_*/wb_probe_* target
```
Record each as passes/attempts with log lines:
- M1 build ON and OFF; OFF loads IA with no bot commands registered.
- M2 one bot: same behaviour as caf1a815 (Universal/Normal parameters unchanged).
- M3 two bots, no human: `Bot <id> acquired enemy ... kind=bot`, hits with `botShooter`/`botTarget`,
  KillPlayer, revive of the same EntityId, Tab/score: kills+deaths each once.
- M4 1→2→4→6 bots: distinct spawn points, no lockstep strafing, PROFILE costs recorded.
- M5 remove/re-add during a match, map change, `wb_remove_all`: no stale EntityId in `wb_list`,
  no crash, scores cleared.
- M6 human + 6 bots, then a second simultaneous client.

## Automated QA loop (Tools/ClaudeQA, Tools/wb_log_report.py)

`ClaudeQA\Claude-Runner.bat` (started once on the Windows PC) polls `ClaudeQA\inbox\*.json`:
```json
{"id":"j01","build":"on|both|none","qa":[{"bots":2,"seconds":240,"tag":"m3"}]}
```
It imports `ClaudeQA\warsbots-multibot.bundle` into the separate worktree `..\crymp-multibot`,
builds incrementally (`build/multibot-on|off`), runs headless loopback servers on port 64120 with
`wb_add N`, copies logs to `build\claude-qa`, and writes `ClaudeQA\outbox\<id>.json` (with HEAD).
Only these fixed job types exist; it never touches game-root EXEs or the user's port-64110 session.
`python3 Tools/wb_log_report.py <log>` prints created/resets/acquisitions/hits/kills/revives/profile
and an M3 verdict. GUI is reserved for client visuals and two-client tests.

## Owner test (needs a human; Claude cannot judge visuals)

Build: `<checkout>\..\crymp-multibot\build\multibot-on\CryMP-{Server,Client}64.exe` (check `wb_status` HEAD in
`ClaudeQA\outbox\*.json`). Server on a free port (not your 64110 session), then in its console:
`wb_probe_local_test 1`, `wb_add 4`, `wb_debug 1`. Join with the matching ON client, play 10 minutes.
Report: (1) do bots look like they strafe/jump/sprint naturally, (2) do they chase you when you run,
(3) Tab shows bots with correct kills/deaths/headshots, (4) any frozen/sliding/T-pose bots, (5) server log.
