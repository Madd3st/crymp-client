# PS-0 patches 1–5 (apply on 2fee59c2, in order)

Date: 2026-10-26. Five `git apply`-able diffs in `Docs/Research/DeepSeek/patches/`. They are a sequence: 2–5 are
written against the previous patch, so apply in numeric order. Verified by applying all five on a clean `2fee59c2`
worktree (`git apply --check` + apply, no conflict). No game build; the two new engine-free tests were built with MSVC.

| # | Patch | Files | Check |
| --- | --- | --- | --- |
| 1 | `ps0_1_mode_gate.patch` | `BotMatch.*`, `CryMPEngineAdapter.*`, `BotConfig.*`, `BotManager.cpp`, new `BotModePlan.h` + test | `BotModePlanTests`: all pass. Server log on PS: gate open for `mode=PowerStruggle`, no `GATE closed`; `wb_allow_public 1` removes the loopback requirement. |
| 2 | `ps0_2_team_spawn.patch` | `BotInstance.cpp`, new `BotTeamPlan.h` + test | `BotTeamPlanTests`: all pass. Log: `[WarsBots] #N PS team=1/2 spawnGroup=… before registration`; `created` + `SetTeam` before `Register`; `DoRevive` uses `GetSpawnLocationTeam` and `RevivePlayer(..., GetTeam(id), ...)`. |
| 3 | `ps0_3_ps_register.patch` | `BotMatch.cpp`, new `Pak/Scripts/WarsBots/PowerStruggle.lua` | Log: `[WarsBots] PS participant registered EntityId=… team=… channelId=0`; `ResetPP/CP/Score/Revive` succeed, no Lua error; `warsBotsPSAttached=true`. |
| 4 | `ps0_4_playercount_wave.patch` | `Pak/CryMP/Scripts/GameRules/PowerStruggle.lua` | Log: `WarsBots PS PlayerCountOk: team … has N/M (bots included)`; with `g_minteamlimit 1` and 0 humans the state leaves `PreGame`; bot death enters `reviveQueue` (`IsInReviveQueue`/wave). |
| 5 | `ps0_5_ps_scores.patch` | `Pak/CryMP/Scripts/GameRules/PowerStruggle.lua` | Bot kill: PP/CP awarded (`AwardKillPP/AwardKillCP`), score increments; no `IsPlayer()` rejection in `ProcessScores`. |

## Apply

```
cd <claude-sprint worktree at 2fee59c2>
for p in Docs/Research/DeepSeek/patches/ps0_1_mode_gate.patch \
         Docs/Research/DeepSeek/patches/ps0_2_team_spawn.patch \
         Docs/Research/DeepSeek/patches/ps0_3_ps_register.patch \
         Docs/Research/DeepSeek/patches/ps0_4_playercount_wave.patch \
         Docs/Research/DeepSeek/patches/ps0_5_ps_scores.patch; do
  git apply --check "$p" && git apply "$p" || break
done
```

Then build/run the two engine-free tests:

```
Tests/BotModePlanTests.cpp
Tests/BotTeamPlanTests.cpp
```

## What each patch actually does

1. **Mode/gate.** `BotMatch::IsBotMode` accepts `PowerStruggle` as well as `InstantAction`; `BotManager::Add` passes
   `allowPowerStruggle=true`; `CryMPEngineAdapter::LocalIAGate` gets `allowPublic` and `wb_allow_public` to bypass the
   `sv_bind=127.0.0.1`/`sv_lanonly=1` requirement. `BotModePlan.h` is the pure policy.
2. **Team before spawn.** After `CreateActor` and before `Register`, a PS bot gets
   `SetTeam(TeamForSlot(number))`, `SetPlayerSpawnGroup(GetTeamDefaultSpawnGroup(team))`, and the revive path uses
   `GetSpawnLocationTeam` + `rules->GetTeam(id)` instead of team 0.
3. **PS registration.** `BotMatch::Register` no longer refuses `USE_SPAWN_GROUPS` for PS; it loads the new Lua shim,
   attaches, and calls `WarsBotsPS:Register(id, team)`, which creates the PS player entry (score/PP/CP/revive,
   `SetupPlayer`, spawn group). The bot still has channel 0 and `IsPlayer=false`.
4. **Start + wave.** `PowerStruggle:PlayerCountOk` uses `GetTeamPlayerCount` (bots are in `m_playerteams` after
   `SetTeam`) instead of the channel-only `GetTeamChannelCount`; `OnPlayerKilled` queues a bot into the PS revive wave
   so the 5 s `BotBrain` revive does not fight it.
5. **PP/CP.** `ProcessScores` accepts `shooter.actor:IsPlayer() or self.botPlayers[shooter.id]`; `AwardKillPP/CP` are
   entity-id based and now run for bots. Team filtering was already `BotTargeting.cpp:43-44`.

## Limits

- The patches are a first PS-0 slice, not a complete PS mode: buy zones/vehicles/scoreboard/HUD are later items.
- `BotTeamPlan::TeamForCounts` is tested but patch 2 uses `TeamForSlot`; the balance hook is ready for a follow-up.
- `PowerStruggle.lua` is a game script; the patch is a diff only, Codex owns the build and integration.
- No claim that a full PS match runs; the checks are unit tests plus the named log lines.
