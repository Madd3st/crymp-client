# Native per-client sends: what a bot can trigger (DS-051 #3)

C-131 already fixed Lua `SendTextMessage` (`ScriptBind_GameRules.cpp:1551`) and DS-050 fixed
`SelectItemByNameRemote` (`ScriptBind_Actor.cpp:1588-1591`). Remaining open list, with triggerability:

| Send | Call site | Can a bot trigger it? | Guard proposal (by identity, never channel 0) |
| --- | --- | --- | --- |
| `ClSetSpectatorMode` | `Player.cpp:277`, `:6674` via `CGameRules::ChangeSpectatorMode` (`GameRules.cpp:1254`) | **yes**: IA/PS death switches a dead bot to spectator 3 (`InstantAction.lua:467/491/537`, `PowerStruggle.lua:843`; `player.actor:SetSpectatorMode` at `:578/592/597/603`) | in `CPlayer::SetSpectatorMode`: skip `InvokeRMI` when `BotMatch::IsBot(*g_pGame->GetGameRules(), GetEntityId())`; keep the local mode change |
| `ClMoveTo` | `GameRules.cpp:1249` (`MovePlayer`) | **yes**: spectator relocation (`InstantAction.lua:586`) can run for a dead bot | in `CGameRules::MovePlayer`: for a bot set the world TM and return |
| `ClForbiddenAreaWarning` | `GameRules.cpp:3330` | **yes**: Lua-exposed (`ScriptBind_GameRules.cpp:1625`), PS/IA warning for any target | return early for `BotMatch::IsBot` |
| `ClRadioMessage` | `GameRules.cpp:4360` | **yes**: `SendRadioMessage` iterates the team, bots are team members; PS alerts | `continue` on bot ids in the loop |
| Item text (`@mp_CannotCarryMore`, `@ammo_maxed_out`) | `Item.cpp:1604/1609` native `SendTextMessage` | **yes**: bot ammo/pickup at limit; bypasses the Lua guard | skip when `BotMatch::IsBot(*g_pGame->GetGameRules(), pActor->GetEntityId())` |
| `ClChatMessage` | `GameRules.cpp:3276/3289/3308` | only if a script sends chat as/for a bot; bots never chat | low priority |
| `ClSimpleKill` | `Actor.cpp:450` | stock kill of a bot victim | check existing `IsPlayer`/channel guard; bots die natively |

Already safe: `SendHitIndicator` (`ScriptBind_GameRules.cpp:2301`) and `SendDamageIndicator` (`:2322`) return for
`!pActor->IsPlayer()`; bots are `IsPlayer=false`.

Status: the table is the deliverable this cycle; the guards are **not** in `ds051.patch` (it is the flags/navmesh
patch). The DS-050 `SelectItemByNameRemote` guard is in the base. Apply the five guards together with the next engine
patch and add `BotAuditFixes` coverage for the predicate.
