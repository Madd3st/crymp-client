# Hardcode-ON flags (DS-051 #1)

Patch removes five default-ON flags and their OFF branches, and the `stateCommit>=2` path. Behaviour is identical
under the C-119 preset (`Play-WarsBotsC119.ps1`: `bot_pace 1`, corners/jump_sanity defaults 1, exclude195=1,
revive_guard=1, `state_commit 1`).

| Removed | Code now | C-119 equivalent |
| --- | --- | --- |
| `wb_spawn_exclude195` | `BotManager.cpp:310` always skips spawn 195 | 1 |
| `wb_revive_guard` | `BotInstance.cpp` always begins/checks the 2 s guard, max 2 retries | 1 |
| `wb_bot_pace` | `BotPace.h` `BotPaceEnabled()` always true (native pace only) | 1 |
| `wb_bot_jump_sanity` | random travel Strength hop deleted; `obstacleJumps = !(meshPath && trap<2)` | 1 |
| `wb_bot_corners` | `BotBrain::Steer` always uses `SteerCorners` on a mesh path | 1 |
| `stateCommit>=2` | `DecisionCommit`/`MoveCommit` branches removed; `stateCommit` is 0/1 only | 1 |

`wb_bot_state_commit` stays 0/1; the 1.5 s attack-hold/wide-leave-range mode 2 (C-118 FAIL) is gone. The dead
`ToCommit`/`IsUrgentCommit`/`OwnerOf` helpers and the `BotDecisionCommit.h` include are removed from `BotBrain`; CI
drops `BotDecisionCommit` and the engine-free module + test stay in the tree, unwired. `BotConfigTests` updated
(`BotPaceEnabled()` is now unconditional).

Verification on the final patch: all 30 engine-free suites pass (incl. `BotConfigTests`, `BotAuditFixes` and
`BotNavMeshCore` with Detour sources); the MSVC `WARSBOTS_EXPERIMENTAL_SPAWNER=ON` client+server build links green
(645/645 ninja objects). `ds051.patch` applies clean on `493f2cab`.
