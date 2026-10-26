# Review of the DS-038/040/039 integration (`fd12a5eb..0fa9c1cb`) — DS-043 #1

Date: 2026-10-27. Read-only review of `0fa9c1cb`; two patches below. Findings (12 lines):

1. **Double revive (PS).** `BotBrain.cpp:404` self-revives at 5 s; the PS wave (`PowerStruggle.lua:962-990`) then calls `RevivePlayer` on the already-alive bot. `ds043_ps_revive.patch` skips the 5 s self-revive in PS (30 s fallback) and logs `PS revive delegated`.
2. **Team id is sent as a handle.** `BotMatch.cpp:62` passes `ScriptHandle(rules.GetTeam(...))`; Lua `WarsBotsPS:Register` uses that `teamId` in `GetTeamDefaultSpawnGroup(teamId)` (`Pak/Scripts/WarsBots/PowerStruggle.lua:28`). Pass a plain int instead.
3. **Speed suit left on after the sprint drop.** `BotBrain.cpp:1043-1044` drops `gait` but not `suitMode`; `ApplySuitPolicy` (`:1359`) keeps `m_speedTravel=Speed`, so the bot runs/aims in Speed and drains energy. Add `WBI(suitMode,NANOMODE_DEFENSE)` when `m_blackboard.mode==NANOMODE_SPEED` and `proFire==1`.
4. **NOFIRE misses the sprint case.** `BotCombat.cpp:309` logs only `blocked/ammo/aim`; when `CanFire()` is false (sprint/reload) it returns silently. Add a rate-limited `NOFIRE reason=canfire` or the next batch still cannot prove the DS-040 cause.
5. **PS Register order.** `BotMatch.cpp:62` calls `WarsBotsPS:Register` before the common `ResetScore` at `:82`, and the Lua `Register` itself loops `ResetScore/ResetPP/ResetCP/ResetRevive` (`PowerStruggle.lua:22`): `ResetScore` runs twice. Move the PS call after the common reset or drop `ResetScore` from the Lua loop.
6. **Missing PS functions.** `ResetPP/ResetCP` are not defined in our pak (only called at `PowerStruggle.lua:325-326`), so they are C++/stock; `SetupPlayer` exists only in `InstantAction.lua:749`. The guarded calls are correct — do not treat the "missing" log as a bug.
7. **PlayerCountOk fallback is valid.** `PowerStruggle.lua:1614` checks `TeamInstantAction.PlayerCountOk`, defined at `TeamInstantAction.lua:578`.
8. **Upgrade gate order.** `BotUpgradeGate.h:14` returns true on `force` before the `enemyVisible` check; a forced weapon tick can still search with a visible enemy beyond `holdRange`. If that is not intended, swap the two lines.
9. **Minor:** `BotMatch.cpp` has a trailing blank line at EOF (the BOM/CRLF issue was the real one; our repo files are now UTF-8 no BOM, LF).
10. **Boat driver:** the split default-arg constructor (`BotBoatDriver.h`) is the correct GCC/Clang fix; `BotBoatDriverTests` compile.
11. **`m_speedTravel` kept:** fine because the sprint drop runs after the policy, but only with `proFire==1`; with the flag off the old Speed travel remains (expected).
12. **Patch order:** `ds043_ps_revive.patch` and `ds043_decision_commit_p1.patch` each apply to `0fa9c1cb`; use `ds043_p1_plus_ps.patch` when applying both (the separate patches overlap in `BotBrain.h`).

## Patches

| Patch | Contents | Check |
| --- | --- | --- |
| `patches/ds043_ps_revive.patch` | PS owns the revive: 5 s self-revive skipped in PS, 30 s fallback, delegated log | applies to `0fa9c1cb` |
| `patches/ds043_decision_commit_p1.patch` | `wb_bot_state_commit=2`: one change per serial, 1.5 s attack leave + 4 m band, MoveCommit, `STATE CONFLICT`, tests | applies to `0fa9c1cb`; `BotDecisionCommitTests` all pass (MSVC 19.44) |
| `patches/ds043_p1_plus_ps.patch` | both of the above in one diff | applies to `0fa9c1cb` |

The DS-038/DS-040 behaviour and NOFIRE logs are otherwise integrated as intended; `wb_allow_public` correctly not taken,
BOMs stripped, and the `BotTeamPlan::TeamForSlot` compile fix is in place.
