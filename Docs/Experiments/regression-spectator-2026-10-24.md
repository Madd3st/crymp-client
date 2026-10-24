# Steelmill regression and native bot spectator, 2026-10-24

Feature branch: [weapon-goals](https://github.com/Madd3st/crymp-client/tree/warsbots/feature/weapon-goals).
[Metrics, source/log hashes and matched build hashes](regression-spectator-2026-10-24.json).
No main/research merge, active-game interruption, graphical client launch or PS changes.

## Frozen P0 regression: 8f0abd5f

Four completed headless matches, 120 seconds each, 2 repeats of 6 and 15 bots.
Existing mixed skills/styles; skill rotation on repeat 2. Same memory snapshot restored
before each run and afterwards (SHA256 in JSON); RNG/physics timing not fixed.
This is scaling/stability validation, not a controlled skill ranking or a new before/after
comparison. The earlier P0 comparison remains in [duel evidence](duel-aim-2026-10-24.md).

| Run | Kills | Native shots | Unknown callbacks | Actor hits / head hits | Median bot Update sum, ms/frame | Worst single bot Update, ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 6, repeat 1 | 4 | 123 | 7 | 50 / 2 | 0.0765 | 2.59 |
| 15, repeat 1 | 37 | 1344 | 64 | 535 / 19 | 0.2325 | 6.16 |
| 6, repeat 2 | 12 | 340 | 15 | 156 / 3 | 0.0825 | 2.59 |
| 15, repeat 2 | 37 | 1351 | 55 | 570 / 10 | 0.2360 | 4.42 |

No crash, fatal/script error markers, replica-shot callbacks or exact duplicate
(time, shooter, weapon, projectile) callbacks in these server logs. SOCOM min interval
0.24 s; run medians 0.36–0.38 s. SCAR/FY71 min 0.09 s, SMG 0.06 s (timestamps have
0.01 s precision). Contiguous authoritative magazine transitions decrement by one.
141 callbacks lack a resolvable projectile/replica state and are excluded from native
cadence claims. No diagnostic cap hit; one unmatched before-hit at forced end of
15-bot run 1 is censored. Server-only evidence does not establish client shot replication.
The CPU figures measure bot Update work, not whole-process CPU or frame-rate guarantees.

Found/fixed an exporter defect in [2a2ac39d](https://github.com/Madd3st/crymp-client/commit/2a2ac39d):
FY71 clip 17 before death was compared with clip 7 after revival/reacquisition ~32 s
later. Lifecycle/gap/unknown-state boundaries now break clip comparison. Added regression
test; 11 Python tests pass. Runtime firing code was not changed. Existing navigation
limits persist: weapon timeouts 35/43/23/65 are not evidence of new spectator defects
(see actual per-run JSON for precise counts); no navigation tuning in this task.

Actual commands (MSVC environment through existing build script):

```powershell
./Tools/Test-WarsBotsTactics.ps1 -GameDirectory 'E:\Steam\steamapps\common\Crysis' -BaselineDirectory build/play-8f0abd5f -CandidateDirectory build/play-8f0abd5f -ExpectedBaseline 8f0abd5f -ExpectedCandidate 8f0abd5f -CandidateOnly -FireDiagnostics -Seconds 120 -Repeats 2 -Counts 6,15 -OutputDirectory build/p1-regression-8f0abd5f
python -B Tools/wb_tactics_report.py build/p1-regression-8f0abd5f/runs.json --output build/p1-regression-8f0abd5f/report.json --episodes build/p1-regression-8f0abd5f/episodes.jsonl
python -B -m unittest discover -s Tests -p 'test_*.py'
```

`python` above was the bundled Python executable. Raw engine/bot logs, restored memory
snapshot, 520 bounded telemetry episodes and full report remain under the local output
folder. Git contains compact metrics/hashes, not game resources or executables.

## Spectator implementation

[aeda53df](https://github.com/Madd3st/crymp-client/commit/aeda53df) adds:

- Native `GetNextSpectatorTarget` includes existing `GetMatchParticipants` only on an
  opted-in local IA server. Real channel enumeration and team/PS rules stay intact.
- `wb_watch list|1..16|next|prev|status|off` uses real local observer + existing native
  ChangeSpectatorMode/RMI. Requires IA and already being in native spectator mode.
  Targets require the replicated bot marker/channel 0, not merely a matching name.
- Native F1 toggles first-person and preserves native input/weapon/HUD flow. Bot FP
  direction derives from the already replicated aim pose; native smoothing is reused.
  View rotation is renderer-local, not written to authoritative movement/view rotation.
  No wire-format changes or invented INetChannel.
- `wb_watch status`: followed bot name/EntityId, native spectatorHealth (-1 unknown),
  weapon, replicated suit/energy, FP flag. No AI target coordinates sent to clients.
- `wb_watch_info`: local IA **server console** only; prints current decisions/status
  solely for owned bots followed by real connected spectators. No extra debug RMI.
- Pure selection/privacy tests cover next/prev wrap, empty/removed target, singleton,
  human/dead/hidden/spectating eligibility and alive-player/mode rejection.

[a6ee9e9e](https://github.com/Madd3st/crymp-client/commit/a6ee9e9e) fixes the QA pilot:
use NULL_ENTITY handle, and query 500 ms after deferred removal rather than within the
same startup frame. Initial pilot failed on engine error markers despite its Lua
assertions returning PASS; it is retained as failed evidence, not counted as success.

BUILD PASS: MSVC x64 Release experimental server + client, source **a6ee9e9e**.
C++ BotSpectatorTests PASS. Native headless QA PASS (60 s): five actual Lua/C++ query
batches, 31 assertions, participant counts **0,1,3,6,5** (last after removal), no engine
error markers. Dedicated client-only command refusal and zero-observer diagnostic logged.
No channel-0 actor was made an observer; queries do not exercise network camera switching.
Default-OFF not rebuilt. FP/TP visuals, HUD, real-player switching, death/respawn follow,
map changes and late-client behavior remain manual checks.

```powershell
./Tools/Build-WarsBots.ps1 -Experimental -BuildDirectory build/multibot-on -Jobs 6
cl /nologo /std:c++17 /EHsc /I Code Tests/BotSpectatorTests.cpp /Fo:build/BotSpectatorTests.obj /Fe:build/BotSpectatorTests.exe
./build/BotSpectatorTests.exe
./Tools/Test-WarsBotsSpectator.ps1 -GameDirectory 'E:\Steam\steamapps\common\Crysis' -BuildDirectory build/play-a6ee9e9e -OutputDirectory build/spectator-a6ee9e9e -Seconds 60
```

Matched EXEs/manifest/manual launchers: `build/play-a6ee9e9e/`, preserved separately
from frozen 8f0abd5f and installed game EXEs. Build logs `build/p1-spectator-*.log`.
All owned QA servers stopped; original Steelmill memory restored.

## Manual client scenario (not run)

1. Start `build/play-a6ee9e9e/Play-Server.cmd`, then matching `Play-Client.cmd`.
   Loopback LAN IA Steelmill, port 64150, no automatic bots, SOCOM fixture OFF.
2. Server: `wb_add 6 normal` (or `wb_maxbots 15`, `wb_add 15 elite`).
3. Client: enter native spectator using menu or `spectator 1`. Native entry may reset
   inventory/score; this follows existing game behavior. Alive `wb_watch 1` must refuse.
4. Client: `wb_watch list`, `wb_watch 1`, `wb_watch next`, `wb_watch prev`.
   Native spectator cycling should include humans and bots. Use **F1** for FP/TP.
5. `wb_watch status`: compare camera pitch/yaw, weapon/reload, HUD health/suit/energy
   with action. Server `wb_watch_info` should show only this followed bot's decisions.
6. Observe death/5-second respawn, switch/remove target (`wb_remove 1`), remove all,
   recreate, then reload Steelmill. Native death follow may choose another target after
   ~3 seconds; singleton may return to free camera. Select again after revival if needed.
7. Connect a second matching client later, repeat selection/F1, check independent views
   and normal player controls. `wb_watch off` returns to native free spectator, not a spawn.
8. Preserve both client logs and server `.log`/`.bots.log`. Do not mark replication or
   naturalness confirmed until this live test succeeds.

Claude research **47ccfacf**, `Docs/Research/CryMP/INTEGRATION_OPPORTUNITIES.md`, was
read directly from its research branch. Proposals remain read-only; branches not merged.
Next: live spectator validation, then fix only reproduced camera/HUD defects before
continuing combat-naturalness and broader weapon/range evaluation.
