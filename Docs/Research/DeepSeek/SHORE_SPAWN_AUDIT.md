# Shore spawn audit: first life vs wave (DS-052 #1)

Source: `C127-4f3a1d72-20261027-185023` (owner playtest, 6 bots, 493.7 s, build `4f3a1d72`, Shore).
Raw: `C127-20261027-185023-Server.bots.log`; map groups cross-checked in `ps_shore.json`.

## Verdict

The bunker spawns at first life were **neither a self-revive (`GetSpawnLocationTeam`) nor the wave**.
They were the normal initial `BotInstance::Spawn`: `BotManager::AddWith` picks `ChooseSpawnLocation(0,0)`
(`BotManager.cpp:384`), then `bot->Spawn(where)` (`:391`) calls `CreateServerActor` at that position
(`BotInstance.cpp:58`). `GetSpawnLocationTeam` is only in `DoRevive` (`BotInstance.cpp:321-322`), which PS
skips by delegating to the wave.

Evidence: 12 `created EntityId=` lines (6 at 0.06, 6 after the 31.76 game reset), no `revive requested`
lines; all 6 `PS REVIVE SEEN` lines are `source=wave` (`:672/674/1824/4232/8105/11059`) and 0 are self.
The 5 `PS revive delegated to the wave` lines (`:370/371/1462/4012/10828`) confirm PS never ran `DoRevive`
in this session.

## First six spawns (0.06)

| Bot | Team | Spawn | Position | Group from `ps_shore.json` | Correct base? |
| --- | --- | --- | --- | --- | --- |
| #1 | tan | 669 | 1965.9,1536.2 | neutral `SpawnGroup1` (1968.96,1535.86) | no |
| #2 | black | 126 | 1844.3,1235.5 | tan `SpawnGroup2`/Base_NK (1839.81,1238.62) | **enemy base** |
| #3 | tan | 131 | 1841.3,1245.0 | tan `SpawnGroup2`/Base_NK | yes, by picker luck |
| #4 | black | 558 | 2111.3,1885.9 | black `SpawnGroup_US5`/Submarine_US (2109.25,1884.25) | yes |
| #5 | tan | 522 | 2122.1,1884.4 | black `SpawnGroup_US5`/Submarine_US | **enemy base** |
| #6 | black | 76 | 1963.1,1313.4 | neutral `SpawnGroup1` (1964.01,1309.99) | no |

The team/group metadata was already correct before the pick: `#N PS team=T spawnGroup=G` at 0.06
(`:5,15,24,33,42,51`), with tan group 120 (Base_NK, spawnpoints 126/131/...) and black group 537
(Submarine_US, spawnpoints 522/558/...). The position picker simply ignored it.

## Second spawn after the 31.76 game reset

`Game reset: restored 6/6 bots` (`:1153`) re-created the same six bots with new EntityIds. The positions are
again mixed: #6 spawn 83 / neutral (1964,1312), #5 191 / neutral (2150,1296), #4 223 / neutral (2295,1535),
#3 669 / neutral (1966,1538), #2 126 / **tan base** (1844,1235), #1 131 / tan base (1841,1245). So every
"restore" repeats the same wrong first spawn.

## Consequence for C-134 (`1a7ec84e`)

C-134 fixes `DoRevive` (`BotInstance.cpp:317-345` at `1a7ec84e`): on PS it asks `GetPlayerSpawnGroup` and
`GetSpawnLocation(..., group, ...)` instead of `GetSpawnLocationTeam`. That is right for revives, but the
observed first-life spawns never enter `DoRevive`; C-134 does not change `BotManager::AddWith` /
`BotInstance::Spawn`, so the owner's "bunker coordinates" at match start will remain. The initial PS spawn
must use the same team-group rule (choose the group spawn before `CreateServerActor`, or move the new actor
to it before registration), with the wave/neutral fallback only when the group has no free point.

## Other findings in the same log

* First-life PP was 0: `PS BUY skip pp=0` (`:317/318/1398`); C-134's `ppList.START` change addresses that.
* Purchases still failed with PP: `PS BUY pp=100 primary=shotgun ... ok=0 ammo=shotgunshell ok=0` (`:3944`,
  `:8095`) and `pp=200 primary=macs ... ok=0` (`:10775`), each preceded by
  `CheckVirtualInventoryRestrictions(inventory, itemClassName) expects parameter 2 to be type String and not Null`
  (`PS-error-context.log:5-8/19-22/33-36`, `powerstrugglebuying.lua:984`). This is a second, separate bug:
  START PP alone will not make PS-2 buying work.
* Shore ran with `wb_bot_navmesh 0` (`session.json`), and the summary counted 1,170 NAV fallbacks
  (24.59/bot-minute, 61.3 % `no_route`). That supports Claude's decision to hold the DS-051 BotNav removal
  until a Shore navmesh exists. The DS-051 native-send guards are independent and are in
  `patches/ds051_guards.patch`.
