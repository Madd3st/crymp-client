# PS channel-0 RMI inventory (DS-045 #1)

Base `6e2713f9`. Evidence: C-118 PS runs (`build/c118-ps0-09fd9520/{Shore,Plantation}/C118-*-run.log`); C-119 IA
(`C119-6e2713f9-...-Server.bots.log`) has no PS RMI errors. Stock Lua was read from `GameData.pak` and `ZPatch1.pak`
(the effective `PowerStruggleBuying.lua` is ZPatch1, 49,244 B). Patch: `patches/ds045_ps_bot_rmi.patch`.

**C-118 actual:** Shore `ClEnterBuyZone` 31 stacks (21 + 10 `UpdateAreas`), `ClPP` 17; Plantation 45 and 16. All are
`RMI No such server channel 0`; no `ClEnterServiceZone`, `ClRank`, `ClMDAlert`, capture/HQ stacks appeared.

Guard helper (added to `InstantAction.lua`, global, available to all PS scripts):

```lua
function WarsBotsIsBot(rules, playerId)
    if (playerId == nil) then return false end
    local p = System.GetEntity(playerId);
    if (p and p.warsBot == true) then return true end
    return rules ~= nil and rules.botPlayers ~= nil and rules.botPlayers[playerId] == true;
end
```

| # | File:line (effective stock) | Trigger | Bot reaches? | Fix |
| ---: | --- | --- | --- | --- |
| 1 | `Scripts/GameRules/PowerStruggleBuying.lua:296` `ClPP` | `AwardPPCount` from kill/capture/revive top-up | **yes** — C-118 17/16 stacks; `AwardKillPP`, `CommitRevivePurchases`, `Capture` | guard `playerId` |
| 2 | `...Buying.lua:1159` `ClEnterBuyZone(true)` | `OnEnterBuyZone` (buy zone overlapping) | **yes** — C-118 31/45 stacks | guard `player.id` |
| 3 | `...Buying.lua:1171` `ClEnterBuyZone(false)` | `OnLeaveBuyZone` | **yes** — 3-12 stacks | guard `player.id` |
| 4 | `...Buying.lua:1211` `ClEnterServiceZone(true)` | `OnEnterServiceZone` | yes (bots roam service zones; no C-118 sample) | guard `player.id` |
| 5 | `...Buying.lua:1219` `ClEnterServiceZone(false)` | `OnLeaveServiceZone` | yes (same) | guard `player.id` |
| 6 | `...Buying.lua:486/488` `ClBuyOk/Error` | `Server:Buy` request | no (no bot buy path) | leave |
| 7 | `...Buying.lua:843/845` `ClBuyOk/Error` | server-side buy result | no (no bot buy path) | leave |
| 8 | `Scripts/GameRules/PowerStruggleRank.lua:189/192` `ClRank` | `UpdateTeamRanks` after PP/CP; bots earn PP | **yes** (bot PP exists; C-118 ClPP proves it) | guard `playerId` |
| 9 | `Scripts/GameRules/PowerStruggleUtils.lua:77` `ClInit` | `OnPostInitClient` | no (bots have no net client) | leave |
| 10 | `CryMP/.../PowerStruggle.lua:345` `ClEnterBuyZone` | `OnClientEnteredGame` | maybe (bot registration path) | guard `player.id` |
| 11 | `...PowerStruggle.lua:523/534` buy/service leave | `OnVehicleDestroyed` | maybe (bot in vehicle zone) | guard `player.id` |
| 12 | `...PowerStruggle.lua:875/925/974/975` `ClReviveCycle`/`ClSpawnGroupInvalid` | `ResetRevive` / `UpdateReviveQueue` | **yes** (PS wave owns bot revive) | guard `playerId`/`player.id` |
| 13 | `CryMP/.../PowerStruggleAlert.lua:656/666` `ClEnterCaptureArea` | enter/leave capture area | no (`actor:IsPlayer()` guard) | guard `entity.id` |
| 14 | `...PowerStruggleAlert.lua:751` `ClPerimeterBreached` | enemy enters base perimeter | **yes** (team players incl bots; currently `channel>0`) | guard `p.id` |
| 15 | `...PowerStruggleAlert.lua:774/776` `ClTurretHit/Destroyed` | turret damaged/destroyed | **yes** (same loop) | guard `p.id` |
| 16 | `...PowerStruggleAlert.lua:805` `ClHQHit` | HQ damaged | **yes** (same loop) | guard `p.id` |
| 17 | `...PowerStruggleAlert.lua:826/835` `ClMDAlert`/`ClMDAlert_ToPlayer` | MD weapon bought | **yes** (enemy team players incl bots; buyer path no) | guard `player.id`/`thisPlayer.id` |
| 18 | `CryMP/.../TeamInstantAction.lua:526` `ClPlayAlert` | "almost lost" team alert | **yes** (enemy team players incl bots; currently `channel>0`) | guard `enemy.id` |
| 19 | `CryMP/.../InstantAction.lua:544` `ClSetupPlayer` | `OnClientEnteredGame` | no (net clients only) | guard `player.id` |
| 20 | `...InstantAction.lua:1318` `ClSetSpawnGroup` | spawn group change | maybe (PS `ResetPlayers`/RequestSpawnGroup) | guard `player.id` |
| 21 | `...InstantAction.lua:1344/1365` `ClSetPlayerSpawnGroup` | spawn group selection | maybe | guard recipient / changed `playerId` |
| 22 | `...InstantAction.lua:1392` `ClSpawnGroupInvalid` | `OnSpawnGroupInvalid` | maybe | guard `player.id` |
| 23 | `...InstantAction.lua:1746+` `ClStart/Step/StopWorking` | work action | no (bots do not work) | leave |
| — | `PowerStruggle.lua:1449`, `Alert:352/619/725`, `Utils` capture, `Buying:1253-1257` | `allClients` broadcasts | safe (no channel) | none |

## What the patch changes

- `InstantAction.lua`: adds `WarsBotsIsBot` and guards `ClSetupPlayer`, `ClSetSpawnGroup`, both
  `ClSetPlayerSpawnGroup` calls and `ClSpawnGroupInvalid` by identity (host channel 0 still gets its RMIs).
- `PowerStruggle.lua`: replaces all seven `GetChannel() ~= 0` guards with `WarsBotsIsBot` (buy/service/revive/spawn).
- `PowerStruggleAlert.lua`: guards capture/perimeter/turret/HQ/MD alerts. `ClEnterCaptureArea` keeps the player check
  and adds the identity check.
- `TeamInstantAction.lua`: "almost lost" alert by `enemy.id`, not `channelId > 0`.
- Adds full `PowerStruggleBuying.lua` (ZPatch1 stock) and `PowerStruggleRank.lua` (GameData stock) overrides because the
  C-118 errors come from those stock files, not from the CryMP `PowerStruggle.lua`; only the listed RMI lines differ.
- Encoding: all six Lua files are Latin-1, LF, no BOM. `git apply --check` clean on `6e2713f9`.

## Residual

`ClSetSpawnGroup(channelId, groupId)` at `InstantAction.lua:1318` receives only a channel, no player id; it is guarded by
`player.id` (the changed player), which is sufficient because the call is in that function's scope. `ClInit` and the
buy/work paths are marked unreachable; no guard is added. `ResetScore` still runs twice in `WarsBotsPS:Register` (minor,
not an RMI error).
