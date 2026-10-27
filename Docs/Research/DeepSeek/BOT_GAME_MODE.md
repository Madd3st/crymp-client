# IBotGameMode (DS-050 #2)

One policy object replaces the `strcmp("PowerStruggle")` / `IsPowerStruggle` branches. Engine-free in
`Code/CryGame/Bots/BotGameMode.h`:

- `BotGameModeKind { Unknown, InstantAction, PowerStruggle }` (`:9`).
- `BotGameModePolicy` (`:11`): `botRules`, `teamBeforeSpawn`, `teamSpawn`, `waveRevive`, `captureGoals`,
  `reviveBuy`, `attachParticipant`, `spawnGroups`; `PolicyForGameRulesClass` fills IA (none) or PS (all true).
- `IBotGameMode` (`:50`) + `BotGameModeStatic`; `BotGameModeFor(className)` (`:68`) returns the IA/PS singleton.
- `BotMatch::Mode(rules)` (`BotMatch.cpp:18`) resolves the rules class once; `BotMatch::IsPowerStruggle` is now a
  compatibility wrapper over `Mode(rules).Kind()`.

## Replaced branches

| Site | Old | New |
| --- | --- | --- |
| `BotInstance.cpp:71` | `IsPowerStruggle` | `Mode(*rules).Policy().teamBeforeSpawn` |
| `BotInstance.cpp:287` | `IsPowerStruggle` | `.waveRevive` (PS REVIVE SEEN) |
| `BotInstance.cpp:320` | `IsPowerStruggle` | `.teamSpawn` (`GetSpawnLocationTeam`) |
| `BotBrain.cpp:476` | `IsPowerStruggle` | `.waveRevive` (30 s fallback) |
| `BotBrain.cpp:477` | `ps` | `.reviveBuy` (PS-2 buy) |
| `BotBrain.cpp:752` | `IsPowerStruggle` | `.captureGoals` (PS-1) |
| `BotMatch.cpp:47` | `IsPowerStruggle` | `.attachParticipant` (WarsBotsPS register) |

The revive policy, spawn choice and the capture/buy goal hooks now all read the same policy; adding a third mode
means one new `PolicyForGameRulesClass` branch, no new string compares.

## Tests

`Tests/BotGameModeTests.cpp` (CI `BotGameMode`): IA has no PS flags; PS has all seven; null/unknown is Unknown;
`BotGameModeFor` returns the right singleton and PS `reviveBuy`. MSVC `/std:c++17 /permissive-`: pass.
