# PS-0 patch list: bots on Shore, 3–4 per team (DS-030)

Date: 2026-10-26. Read-only plan at `claude-sprint` `2ffc95fa`. Goal: on `Shore` (`sv_gamerules PowerStruggle`)
bots spawn on their team's side, capture/fight, and the round actually starts. Ordered so items 1–5 give the first
playable state; 6–10 make PP/score/revive/buy honest.

## 1. Open the gate for PS and for a non-loopback server

| Where | Change | Test |
| --- | --- | --- |
| `Code/CryGame/Bots/BotMatch.cpp:14,23,76` | `IsBot`/`Collect`/`Unregister` accept `PowerStruggle` in addition to `InstantAction` (same class-name check, no other logic). | `wb_add 1` on Shore logs `PS participant` instead of `IA registration failed`. |
| `Code/CryGame/Bots/CryMPEngineAdapter.cpp:17-20` | `LocalIAGate`: allow `PowerStruggle` when the caller asks; split the public-server condition from `sv_bind=127.0.0.1` + `sv_lanonly=1` behind a new `wb_allow_public=1` (or `wb_server_mode=public`). | Gate log shows `mode=PowerStruggle` and `bind=0.0.0.0` accepted only with `wb_allow_public=1`. |
| `Code/CryGame/Bots/BotManager.cpp:312` | Pass `allowPowerStruggle=true` to `LocalIAGate` (today the default `false` refuses PS). | `Add` no longer returns null on Shore. |

## 2. Team before spawn

| Where | Change | Test |
| --- | --- | --- |
| `BotInstance::Spawn` `BotInstance.cpp:64-74` | After `CreateActor`, call `g_pGame->GetGameRules()->SetTeam(team, id)` **before** `BotMatch::Register`/`EquipLikePlayer`; take `team` from the new `IBotGameMode` (PS: alternate 1/2, 3–4 per team). | `[WarsBots] #N created ... team=2`; `GameRules::GetTeam(id)` returns 1/2, not 0. |
| `BotInstance::DoRevive` `BotInstance.cpp:289,296,316` | Stop requiring `m_owner.ChooseSpawnLocation` (IA list) when the mode is PS; choose a PS spawn group/team spawn; pass `rules->GetTeam(id)` to `RevivePlayer` instead of `0`. | Revive log `team=2 spawnId=<bunker/HQ group>`, bot appears on its side. |
| `BotManager::ChooseSpawnLocation` `BotManager.cpp:251-274` | Dispatch by mode: PS uses `GetSpawnLocationTeam(playerId, deathPos)` / `GetSpawnLocation(..., groupId, ...)` with `player.spawnGroupId = GetTeamDefaultSpawnGroup(team)`; keep IA path unchanged. | No revive on the enemy HQ; `SPAWN` log contains the team's group. |

`SetTeam` already adds any actor to `m_playerteams` (`GameRules.cpp:1738,1750-1754`), so scoreboard/team counts see
bots once this is done. It also writes `m_channelteams[0]` for every bot (`:1764-1775`) — fix in item 7.

## 3. PS registration of a channel-0 actor

`PowerStruggle.Server:OnClientConnect` (`PowerStruggle.lua:326-336`) and `OnClientEnteredGame` (`:340-355`) only run
for real channels, so a bot has no PP/CP/score/revive entry and no `SetupPlayer`. Add a server hook
`PSBots:Register(entityId, teamId)` in a new `Scripts/WarsBots/PowerStruggle.lua`, attached the same way as
`Scripts/WarsBots/InstantAction.lua` (`BotMatch.cpp:38-47`), that does:

1. `local player = System.GetEntity(entityId)`; `player.warsBot = true`; `self.botPlayers[entityId] = true`.
2. `self:ResetScore(id); self:ResetPP(id); self:ResetCP(id); self:ResetRevive(id)` (functions exist: `PowerStruggle.lua:330-335`).
3. `self:SetupPlayer(player)` (ammo capacities; `InstantAction.lua:731`), then `self:RevivePlayer(0, player)`.
4. `player.spawnGroupId = self.game:GetTeamDefaultSpawnGroup(teamId)` (required because `USE_SPAWN_GROUPS=true`,
   `PowerStruggle.lua:33-35`, and `RevivePlayer` returns false without a group at `InstantAction.lua:853-856`).
5. `self.game:SetTeam(teamId, id)` (safe to repeat; PS `OnSetTeam` updates objectives).

Do not use `SpawnPlayer(0,...)`: `CGameRules::SpawnPlayer` looks up by channel first (`GameRules.cpp:968-970`), so the
second call returns the first bot.

## 4. Round start with bots

| Where | Change | Test |
| --- | --- | --- |
| `TeamInstantAction.lua:446-457` | `PlayerCountOk` must count bots: either a new `GetMatchTeamCount(team)` (channels + bots on that team) or `GetTeamPlayerCount(team)` (already includes bots because `SetTeam` adds actors). Do **not** widen `GetTeamChannelCount` (`GameRules.cpp:1668-1681`): it is also used by autobalance (`TeamInstantAction.lua:611-614,1034-1051`). | With `g_minteamlimit 1` and 0 humans, the state leaves `PreGame` after `wb_add 3` per side. |
| `PowerStruggle.lua:756-782` | On bot death, call `self:QueueRevive(botId)` (the server `OnPlayerKilled` path already runs for bots, `:670-683`); `FlushReviveQueue` (`:801-815`) then calls `RevivePlayer(0, player)`. Guard `channelSpectatorMode[0]` at `:780`. | Bot death → revive in the next wave, not immediately and not on the enemy side. |

## 5. Combat sanity

| Where | Change | Test |
| --- | --- | --- |
| `Code/CryGame/Bots/BotTargeting.cpp:43-44` | Already filters `rules->GetTeam(self) == rules->GetTeam(other)`; with item 2 this becomes a real team filter. Keep `team 0` = no filter only for IA. | No friendly fire in a 4v4 log; `TARGET CHOICE` never picks a same-team bot. |
| `PowerStruggle.lua:1128-1138` | `ProcessScores` gates on `shooter.actor:IsPlayer()`; accept `self.botPlayers[shooter.id]` too, otherwise bots earn 0 PP/CP. | Bot kill → `IA SCORE kills` / PP increases on the shooter's entity. |

## 6. Scoreboard, PP and rank for bots

- `AwardPPCount`/`AwardCPCount` write synched values by entity id (`PowerStruggleBuying.lua:227-233`), so they work
  once `ProcessScores` lets bots through.
- `GetPlayerRank` reads `RANK_KEY` per entity (`PowerStruggleRank.lua:80-84`); `UpdateRanks` sorts `GetTeamPlayers`
  (`:160-190`), which includes bots after item 2. No change, but verify a bot can be rank 1 and its `equip` list is
  given by `EquipPlayer`.
- HUD/scoreboard (`HUD.cpp:1578` uses `GetTeamPlayerCount`) already counts bots; check the scoreboard does not call
  `GetChannelId` on them.

## 7. Channel-0 side effects in `SetTeam`

`GameRules.cpp:1756-1778` runs for every actor: `UpdateObjectivesForPlayer(GetChannelId(id)=0, team)`,
`ReconfigureVoiceGroups(id, ...)` and `m_channelteams[0] = team`. Patch: guard the channel/voice block with
`if (int channelId = GetChannelId(id); channelId > 0)`, and keep `m_entityteams`/`m_playerteams` for bots. This
removes the "last bot wins `GetChannelTeam(0)`" bug and channel-0 RMI traffic.

## 8. Per-client RMIs addressed to channel 0

PS calls `onClient:Cl...` with `player.actor:GetChannel()`:
`ClResetBuyZones`/`ClEnterBuyZone` (`PowerStruggle.lua:346,350`), `QueueRevive` (`:780`), `FlushReviveQueue`
(`:806`), and `ClPP/ClRank` in the buying/rank code. For bots, either skip the RMI (`if channel>0`) or replace it
with a synched entity value/all-clients RMI. `ClHQHit` already checks `channel>0` in
`PowerStruggleAlert.lua:797-812` — use that as the pattern.

## 9. Buy zones and purchases

- `inBuyZone[player.id]` is filled by `OnEnterBuyZone` on the server (`PowerStruggleBuying.lua:1194-1207`), which
  fires from the entity trigger, not from a channel. A bot that walks into the zone gets the same callback; verify
  with a `BUY ZONE` log. The server also loops `inBuyZone` and calls `ClEnterBuyZone(player.actor:GetChannel())`
  (`PowerStruggle.lua:528-532`) — guard channel 0 there too.
- `SvBuy` is a client RMI (`PowerStruggle.lua:204`); bots have no client. Add a server-side `PSBots:Buy(botId, item)`
  that calls the same `BuyItem` path (`PowerStruggleBuying.lua:496,766-800`) with `noPrice=false`.
- `CanBuy` checks factory team/power/level (`PowerStruggleBuying.lua:766-800`); the bot must be on the factory's team
  and the team must own the factory. Test: buy a SCAR at the tan small-vehicle factory, PP −175.

## 10. Vehicles (PS v1)

- Entry: `IVehicleSeat::Enter` after `CanEnterVehicle` (team/owner/claim, `PowerStruggle.lua:571-584`); the bot must
  have its team set first (item 2). `VEHICLES.md` has the sequence.
- Exit: `seat->Exit(false,false)`; clear the vehicle goal on death/destroy (`GameRules.cpp:659-663,1005-1008`).
- PS claim window is 90 s and the owner is stored on the vehicle (`PowerStruggle.lua:350-370`); only enter an
  unowned or own-team vehicle. Test: tan bot enters a neutral 4WD, drives 50 m, exits at a capture point.

## Acceptance for PS-0 (first five)

1. `sv_gamerules PowerStruggle`, Shore, 0 humans, `wb_fill 3` → round leaves `PreGame`; 3 bots on tan, 3 on black.
2. Every bot spawns within its team's HQ/bunker group, not on the enemy side or neutral centre.
3. Bots capture a neutral factory: `CAPTURED by team` in the log, PP +300 to the last capturer.
4. A bot kills an enemy: score +1, PP +100, no friendly fire in 5 minutes.
5. A dead bot revives in the next PS wave on its own side; no bot is stuck in `reviveQueue` for > 2 waves.

## Risks / open

- `CreateActor(0,...)` + `SetTeam` is not the same as `SpawnPlayer`; `CGameRules::OnClientConnect` never runs, so any
  script that assumes `self.players[channel]` must be replaced by the PSBots hook (items 3, 6, 8).
- `GetSpawnLocationTeam` uses `player.death_pos`; a fresh bot has none — pass the team spawn group explicitly.
- AutoTeamBalance (`TeamInstantAction.lua:1034-1051`) may try to move bots because they are in `m_playerteams`; skip
  bot ids in `AutoTeamBalanceSwitchPlayer` (or keep it off for PS-0).
- `sv_gamerules` is read by `LocalIAGate` (`CryMPEngineAdapter.cpp:19`); changing it mid-session closes the gate.
