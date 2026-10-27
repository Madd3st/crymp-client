# C127 owner Shore playtest: spawns vs revives (DS-052 #1)

Archive: `build/owner-playtests/C127-4f3a1d72-20261027-185023/` (18:50:23-18:58:43, 6 bots, 3+3, base
`4f3a1d72`, `wb_bot_navmesh 0`, PS Shore).

## The "bunker spawns" are the initial spawn, not a revive

At `0.06` all six bots log `BODY STATE ... origin=create`, i.e. the first `Spawn`, not `DoRevive`:

| actor | team (PS) | spawn id | position | nearest SpawnGroup |
| --- | --- | ---: | --- | --- |
| 65486 | 1 | 669 | 1965.9,1536.2,100.9 | neutral 689 (1969.0,1535.9) |
| 65481 | 2 | 126 | 1844.3,1235.5,111.5 | team 120 (1839.8,1238.6) |
| 65476 | 1 | 131 | 1841.3,1245.0,111.5 | team 120 |
| 65471 | 2 | 558 | 2111.3,1885.9,103.5 | team 537 (2109.3,1884.3) |
| 65466 | 1 | 522 | 2122.1,1884.4,100.2 | team 537 |
| 65461 | 2 | 76 | 1963.1,1313.4,118.1 | neutral 96 (1964.0,1310.0) |

So two bots spawned on neutral bunkers (689/96), the rest near the team groups; this is the random IA
`ChooseSpawnLocation` path in `BotInstance::Spawn`, not `GetSpawnLocationTeam` and not the wave. C-134 (`1a7ec84e`)
replaces it with the team spawn group.

## First revive is the wave

- Deaths at `3.12` (65481, 65466), `PS revive delegated` at `8.09`, `PS REVIVE SEEN ... source=wave` at `20.38`.
- No `revive requested`, `revive result`, `PS fallback revive` in the whole log; `PS REVIVE SEEN` is the only revive
  source (7 events total).
- `summary.json` `revives.total=0` is the old DoRevive-only counter, not the wave count.

Teams/spawn groups are correct before registration (`PS team=1 spawnGroup=120`, `team=2 group=537`, 3+3), but the
first spawn ignores them. Also `PS BUY skip pp=0` at `5.10`: `ppList.START` is missing in this build (C-134 adds it).
