# Snipers never get DSG1/Gauss (DS-046 #2)

Raw probes: `build/c113-fallback-roof_strength-0fa9c1cb`, `build/c117-sniper_reach_dsg1-0fa9c1cb`,
`build/c117-sniper_reach_gauss-0fa9c1cb` (all on `0fa9c1cb`; X-157 0/3 each, roof_strength 0/3). Item positions
from `Steelmill` mission: DSG1 `(1307.23,1330.63,159.11)`, Gauss `(1225.44,1331.80,149.15)`.

## Per trial

| Probe / repeat | pickups (s) | `weapon unreachable` | LINK starts (kind @ a->b) | LINK failures | last z | cause |
| --- | --- | --- | --- | --- | ---: | --- |
| roof 1 | FY71 1.5, SCAR 14.9 | 98@11.4, 263@48.0 | jump 1292, strength 1236, strength 1229, drop | no_energy, no_takeoff | 138.5 | goal never chosen |
| roof 2 | FY71 1.5, SCAR 15.0 | 98@11.3, 263@50.0 | jump 1292, strength 1236 (x3) | missed_landing, no_energy, missed_landing | 136.0 | goal never chosen |
| roof 3 | FY71 1.6, SCAR 14.9 | 98@11.3, 263@52.0 | jump 1292, strength 1236, strength 1229, drop | no_energy, no_takeoff | 137.5 | goal never chosen |
| dsg1 1 | FY71 1.4, SCAR 33.4 | 98@29.4 | jump 1292, strength 1236 (x2) | missed_landing | 136.0 | goal never chosen |
| dsg1 2 | FY71 1.5, SCAR 23.5 | 98@19.3 | jump 1292, strength 1236, strength 1229 (x2) | no_takeoff | 142.8 | goal never chosen |
| dsg1 3 | FY71 1.4, SCAR 23.4 | 98@19.5 | jump 1292, strength 1236, strength 1229 (x2) | no_energy, no_takeoff | 139.1 | goal never chosen |
| gauss 1 | SOCOM->116 49.9 | 263@27.7, 280@43.3 | strength 1236, strength 1229 (x3) | no_energy, no_takeoff, missed_landing | 137.2 | goal never chosen |
| gauss 2 | 196 41.9, 181 45.1 | 263@27.0, 280@35.2 | strength 1236, strength 1229 (x3), drop | no_energy, no_takeoff, no_takeoff | 138.5 | goal never chosen |
| gauss 3 | 116 54.8 | 263@53.4 | strength 1236, strength 1229 (x2) | missed_landing, no_energy, no_takeoff, missed_landing | 136.0 | goal never chosen |

No trial starts the 1355 link (`a=1354.96,1347.60,147.74`); all attempts are at 1292/1236/1229 and end at
z 136-142.8.

## Why

1. **Goal never chosen (primary).** `FindNearby` queried a box `origin +- 12 m` in z. The probe spawn is z 138.8;
   DSG1 is **+20.3 m**, Gauss (from its spawn z 136.2) is **+12.95 m** вЂ” both outside the box. Offline `NavMeshCore`
   on the same mesh says DSG1 is reachable by a 141.6 m **walk** route (last z 157.94) and Gauss by 166.2 m with six
   links, so this was a search-box bug, not a navmesh hole. The logs show `GOAL SKIP navmesh kind=weapon skipped=5`
   at spawn and the bot takes the nearby FY71/SCAR; `weapon 98/263 unreachable` later are the sniper goals after the
   path timeout, not a link failure.
2. **Link 1355 -> 1351.9 is not the DSG1 route.** From its landing (152.04) to DSG1 is another 76.2 m; the walk route
   is shorter, so no path selects it. It is also **+4.30 m** (147.74 -> 152.04), while the measured Strength jump rise
   is **3.62-3.65 m** (`JUMP LANDING strength=1 rise=...`); the link is physically unexecutable. It is now rejected as
   `too_high` and banned, not retried.
3. **Secondary executor failures** on 1236 -> 1229 (+3.05 m): `no_energy` starts at 28-35 energy (Strength needs 90),
   `no_takeoff` at 3.5 s when the bot is 6 m from `a`, `missed_landing` with `displacementZ=0.00`; these are real
   executor issues but not the sniper-goal cause. Fix them separately after the goal is reachable.

## Patch (`patches/ds046_sniper_reach.patch`, default OFF `wb_bot_sniper_reach=1`)

- `BotWeapons::FindNearby`: sniper-minded searches (`typicalRange > 30`) use a **32 m** vertical box instead of 12;
  a `sniperOnly` filter keeps only `DSG1`/`GaussRifle` until one is held. `HoldsSniper` helper.
- `BotBrain::UpgradeCandidate`/`UpdateUpgrade`: sniper-only first pass, `pathCost` on, 20 s progress / 90 s total
  instead of 8/60; falls back to normal rifles if no sniper is reachable.
- `BotJumpLink`: `LinkFail::TooHigh`, `maxStrengthRise=3.72`; an impossible Strength link is banned immediately.
- `Tools/Probes/roof_strength.json`: now grades the reachable DSG1 roof (`pickup:DSG1 <= 60 s`, `stalls=0`);
  `sniper_reach_gauss.json`: 75 s / pickup <= 60 s. The old `link:strength:done` assertion asked for the
  unexecutable east link and is removed.
- Tests: `Tests/BotSniperReachTests.cpp` (class filter, 32/12 m box, TooHigh reject; MSVC all pass). CI list adds
  `BotSniperReach`; patch applies clean on `4d399cf4`.

Target `roof_strength >= 2/3`: with the 32 m box + sniper-only pass the bot picks DSG1 first and walks 141.6 m
(~30 s), within 75 s. The engine probe was not run here (no DLL build); the offline mesh query and unit test are the
verification.

