# PS revive root cause (DS-047 #1)

Raw data: `WarsBotsBuild/crymp-multibot/build/c120b-ps0-4d399cf4/{Shore,Plantation}/C120B-*-run.bots.log`,
`ps-evidence.json`, built from `4d399cf4` (X-165). The question was which gate stops the PS revive wave:
bot never queued, `CanRevive` false, `spawnGroupId` nil, wave never fires, or fallback refused.

## Answer: no gate stops it; the revive works and the counter is wrong

Every delegated bot in C-120b came back on the next wave, and the alive transition is the ground truth:

| Map | death | delegated (5 s) | alive again | dead time |
| --- | ---: | ---: | ---: | ---: |
| Shore | 3.12 (65481, 65466) | 8.13 | 20.37 | 17.25 s |
| Shore | 34.61 (131003, 131008) | 39.60 | 51.85 | 17.24 s |
| Plantation | 3.09 (65518, 65493) | 8.06 | 20.34 | 17.25 s |
| Plantation | 34.67 (131029, 131044); 36.97 (131039) | 39.64 / 41.95 | 51.93 | 17.26 / 14.96 s |

4/4 Shore and 5/5 Plantation delegated bots revived. The 30 s fallback (`BotBrain.cpp:478-489`) was never
needed because the wave is faster, so no `PS fallback revive` line is expected either.

Each gate in order:

1. **Queued.** `PowerStruggle.Server:OnPlayerKilled` queues a bot when `self.botPlayers[id]` is set and it is not
   already queued (`Pak/CryMP/Scripts/GameRules/PowerStruggle.lua:670-677`); `Pak/Scripts/WarsBots/PowerStruggle.lua`
   registers every WarsBot there. The death log proves the kill event arrived (`KillPlayerObserved=1`).
2. **`CanRevive`.** `InstantAction:CanRevive` only needs the player entity and a spawn group when
   `USE_SPAWN_GROUPS` is on (`InstantAction.lua:831-842`); `TeamInstantAction:CanRevive` adds `team ~= 0`
   (`TeamInstantAction.lua:651-658`). `ps-evidence.json` first spawns have team 1/2 and spawnGroup 120/537, and
   `QueueRevive` reads the same live player entity, so both are satisfied.
3. **`spawnGroupId`.** Non-nil in the evidence and unchanged through death; `UpdateReviveQueue` skips only the
   `announced` RMI to a channel-0 bot (DS-045 guard at `PowerStruggle.lua:928`), which is not a revive gate.
4. **Wave fires.** `UpdateReviveQueue` runs when the cycle expires (`PowerStruggle.lua:959-993`) and calls
   `RevivePlayerInQueue` (`:997-1001`). The 17.2 s gap is exactly the revive cycle, not a stall.
5. **`RevivePlayer`.** `PowerStruggle:RevivePlayer` (`:1022-1027`) delegates to
   `TeamInstantAction.RevivePlayer` -> `InstantAction:RevivePlayer` (`InstantAction.lua:846-909`), which calls
   `self.game:RevivePlayer(...)` with a spawn from `GetSpawnLocationTeam`. No inventory/freeze/channel check exists
   in this path.

## Why C-120b reported "0 native revives"

The probe counted only the DLL's own revive path: `BotInstance::DoRevive` logs `revive requested` / `revive result`
and records the demo revive event (`BotInstance.cpp:291-339`). The PS wave calls the Lua GameRules revive directly,
so `DoRevive` never runs. The bot only notices the new life on the next health sample and logs at verbose level 2
(`BotInstance.cpp:276-277`), which C-120b did have (`wb_debug 1`), but no metric keyed on it. Thus:
`0` lines with `revive result` = 0 *self* revives, not 0 revives.

This is the same accounting gap in both directions: fallback logs are absent because the wave worked, and
`RESPAWNING` is absent because only the fallback enters that state (`BotBrain.cpp:478-481`).

## Patch `patches/ds047_ps_revive.patch` (apply-clean on `d6a9a521`)

- `BotInstance.h/.cpp`: `m_selfRevivePending` marks the revive that `DoRevive` requested. On an alive transition
  without that mark in PowerStruggle, log at level 1:
  `PS REVIVE SEEN EntityId=<id> health=<hp> source=wave.` The transition itself is unchanged.
- `BotProbeRunner.h`: `revive` now counts `PS REVIVE SEEN` as well as `revive result ... alive=1`, so the probe
  metric matches the engine truth.
- No Lua, no spawn, no game-rule, no fallback change. Importing `GameRules::GetSpawnLocationTeam`/`RevivePlayer`
  is untouched.

C-121/X-166 confirmation: expect `PS QUEUE` at death, `PS WAVE ... canRevive=true group=<id> dead=true
spec=3 overdue=true`, and `PS REVIVE result=true` around +17 s, followed by `PS REVIVE SEEN`. If X-166 instead
prints `canRevive=false` or `group=nil`, that gate becomes the cause and this conclusion must be revised; the
C-120b alive transitions would then mean the log predicate (`WarsBotsIsBot`) is wrong for those ids, not that
the bot stayed dead.
