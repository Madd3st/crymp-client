# Native per-client sends a bot can hit (DS-050 follow-up)

C-126 already drops Lua `SendTextMessage` to a bot in `ScriptBind_GameRules.cpp:1551` (guard by
`BotMatch::IsBot`). Audit of the remaining native `eRMI_ToClientChannel` sends reachable from Lua:

| Native send | Where | Lua trigger / bot risk | Status |
| --- | --- | --- | --- |
| `ClSelectItemByName` | `Actors/ScriptBind_Actor.cpp:1584` | `player.actor:SelectItemByNameRemote`; stock PS `BuyItem` ammo (`PowerStruggleBuying.lua:873`) -> channel 0 | **fixed in ds050** (`:1588-1591`, `IsBot` guard) |
| `ClSetSpectatorMode` | `Player.cpp:277`, `:6674` via `CGameRules::ChangeSpectatorMode` (`GameRules.cpp:1254`) | IA/PS Lua `ChangeSpectatorMode` / `player.actor:SetSpectatorMode` for a dead bot (`InstantAction.lua:467/491/537/578/592/597/603`, `PowerStruggle.lua:843`) | open: guard in `CPlayer::SetSpectatorMode` for bots |
| `ClMoveTo` | `GameRules.cpp:1249` (`MovePlayer`) | Lua `game:MovePlayer` spectator relocation (`InstantAction.lua:586`) for a dead bot | open: skip RMI for bots (the world TM set stays) |
| `ClForbiddenAreaWarning` | `GameRules.cpp:3330` | Lua-exposed (`ScriptBind_GameRules.cpp:1625`), PS/IA can warn a bot | open: return early for bots |
| `ClChatMessage` | `GameRules.cpp:3276/3289/3308` | Lua `SendChatMessage`; bots never chat, but a script can | low risk |
| `ClRadioMessage` | `GameRules.cpp:4360` | native `SendRadioMessage` iterates the team incl. bots; PS alerts | open: skip bot ids |
| `ClSimpleKill` | `Actor.cpp:450` | stock kill of a bot (self-kill / GameRules) | check `IsPlayer` guard; bots die natively |
| `SendTextMessage` (native) | `Item.cpp:1604/1609` | item pickup "cannot carry more"/ammo max, bypasses the Lua guard | open: guard `Item` callers with `IsBot` |

Already safe: `SendHitIndicator` (`ScriptBind_GameRules.cpp:2301`) and `SendDamageIndicator` (`:2322`) return early
for `!pActor->IsPlayer()`; bots are `IsPlayer=false`.

Do not blanket-drop channel 0: the listen-server host also uses channel 0. Guard by `BotMatch::IsBot(entityId)` at
the call site, as the two fixed rows do.
