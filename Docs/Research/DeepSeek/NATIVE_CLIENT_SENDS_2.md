# Native per-client sends: what a bot can trigger (DS-051 #3)

C-131 already fixed Lua `SendTextMessage` (`ScriptBind_GameRules.cpp:1551`) and DS-050 fixed
`SelectItemByNameRemote` (`ScriptBind_Actor.cpp:1588-1591`). Remaining open list, with triggerability
(base `493f2cab` lines):

| Send | Call site | Can a bot trigger it? | Guard in `ds051.patch` (by identity, never channel 0) |
| --- | --- | --- | --- |
| `ClSetSpectatorMode` | `Player.cpp:277`, `:6674` via `CGameRules::ChangeSpectatorMode` (`GameRules.cpp:1254`) | **yes**: IA/PS death switches a dead bot to spectator 3 (`InstantAction.lua:467/491/537`, `PowerStruggle.lua:843`; `player.actor:SetSpectatorMode` at `:578/592/597/603`) | `CPlayer::SetSpectatorMode`: `perClient` at `Player.cpp:6609`, the three RMIs at `:6633/6656/6681`; the local mode change and `SetAspectProfile` stay. `CPlayer::InitClient` also guards at `:279` (defensive; bots never get a client). |
| `ClMoveTo` | `GameRules.cpp:1249` (`MovePlayer`) | **yes**: spectator relocation (`InstantAction.lua:586`) can run for a dead bot | `CGameRules::MovePlayer` at `:1252`: skip the RMI for `BotMatch::IsBot`, still set the server world TM at `:1255`. |
| `ClForbiddenAreaWarning` | `GameRules.cpp:3330` | **yes**: Lua-exposed (`ScriptBind_GameRules.cpp:1625`), PS/IA warning for any target | `CGameRules::ForbiddenAreaWarning` at `:3336-3337`: return early for a bot target. |
| `ClRadioMessage` | `GameRules.cpp:4360` | **yes**: `SendRadioMessage` iterates the team, bots are team members; PS alerts | `CGameRules::SendRadioMessage` at `:4371-4372`: `continue` on bot ids in the team loop. |
| Item text (`@mp_CannotCarryMore`, `@ammo_maxed_out`) | `Item.cpp:1604/1609` native `SendTextMessage` | **yes**: bot ammo/pickup at limit; bypasses the Lua guard | `CItem::PickUp` at `Item.cpp:1608` computes `perClient` once; both messages are skipped for a bot, the pickup restriction still returns. |
| `ClChatMessage` | `GameRules.cpp:3276/3289/3308` | only if a script sends chat as/for a bot; bots never chat | not guarded (low priority, no trigger found). |
| `ClSimpleKill` | `Actor.cpp:450` | stock kill of a bot victim | not guarded: stock `IsPlayer`/channel path; bots die natively. |

Already safe: `SendHitIndicator` (`ScriptBind_GameRules.cpp:2301`) and `SendDamageIndicator` (`:2322`) return for
`!pActor->IsPlayer()`; bots are `IsPlayer=false`.

The shared predicate is `WarsBots::PerClientSendAllowed(bool isBot)` (`BotClientSend.h:8`); the engine guards pass
`BotMatch::IsBot(...)` (or `CPlayer::IsWarsBotActor()` where the actor is already a `CPlayer`). Channel 0 is never
used as the test because the listen host uses it too. `BotAuditFixesTests.cpp:48-49` covers the predicate; all 30
engine-free suites pass and the experimental MSVC client+server build is green on the patch.
