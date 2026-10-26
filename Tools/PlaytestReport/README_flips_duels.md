# flips.ps1 / duels.ps1 (DS-037 #4)

Read-only analyzers for a `*.bots.log`. Windows PowerShell 5.1, no external modules. Output is JSON next to the log
unless `-Out` is given.

## flips.ps1

```
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/PlaytestReport/flips.ps1 `
  -Log ...\C106-...-Server.bots.log -Out C106.flips.json
```

Fields (all stable, for gating):

| Field | Meaning | C-106 | C-108 |
| --- | --- | ---: | ---: |
| `session.seconds`, `.minutes`, `.bots` | session | 2181.9 s / 8 | 662 s / 15 |
| `owner.reversals_per_min` | owner direction reversals while active | 8.06 | — |
| `bots.reversals_per_bot_min` | same for bots | 34.51 | 22.00 |
| `bots.state_ab_a_2s_per_bot_min` | A→B→A state flips within 2 s | **89.12** | **38.85** |
| `bots.state_conflicts` | `STATE CONFLICT` log lines (post-patch) | 0 | 0 |
| `bots.goal_ab_a_2s_per_bot_min` | arbiter goal flips | 0.44 | — |
| `bots.target_ab_a_2s_per_bot_min` | target A→B→A | 0 | — |
| `bots.attacking_chasing_per_bot_min` | ATTACKING↔CHASING | 9.51 | 14.01 |
| `bots.per_bot[]` | number, reversals, state changes/flips | — | — |

A reversal is a horizontal velocity dot product < 0 with both speeds > 1 m/s and Δt ≤ 0.6 s, same as DS-034. The
owner is `kind=human`; bots are every other `MOVE TRACE`.

## duels.ps1

```
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/PlaytestReport/duels.ps1 `
  -Log ...\C108-...-n15.bots.log -Out C108.duels.json
```

Fields per skill: `kills`, `deaths`, `shots`, `hits`, `hit_rate`, `headshots`, `headshot_share`,
`first_shot_delay_median_s`, `first_shot_delay_samples`, `burst_median`, `moving_shot_share`,
`hits_by_band {0-10,10-20,20-30,30-50,50+}`, `hits_20m_share`.

- Kills/deaths come from `GameRules::KillPlayer` and the `created ... profile=style/skill` map.
- Hits use `FIRE HIT phase=before-Lua` only (no double count).
- First shot = first `FIRE SHOT` by the same pair after a `TARGET CHOICE visible=1`, within 5 s.
- Burst = consecutive shots by one shooter with ≤ 0.35 s gaps; median.
- `moving_shot_share` uses the last `MOVE TRACE` speed within 0.5 s (> 1 m/s).
- Band counts are from the hit distance (reliable); shot-distance bands are not emitted.

C-108 baseline (15 bots):

| Skill | Kills | Deaths | Shots | Hits | Rate | Delay | Burst | 20 m+ |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Beginner | 25 | 86 | 1,331 | 304 | 0.228 | 1.04 s | 1 | 3.9 % |
| Normal | 87 | 75 | 2,864 | 998 | 0.348 | 0.66 s | 2 | 4.9 % |
| Veteran | 93 | 66 | 3,554 | 1,098 | 0.309 | 0.41 s | 2 | 12.3 % |
| Elite | 70 | 48 | 2,320 | 753 | 0.325 | 0.38 s | 3 | 6.8 % |

C-106 has no `FIRE SHOT` diagnostics (shots = 0), so use C-108 for duels and C-106 for flips. `C106.duels.json` is
therefore not kept.

## Gate for C-111

Suggested (from DS-034/DS-035):

- flips: `state_ab_a_2s_per_bot_min < 10` (target < 2), `reversals_per_bot_min < 20` (target < 12),
  `attacking_chasing_per_bot_min < 3`, `state_conflicts` should be visible and trend to 0.
- duels: `elite.kills > veteran.kills`, `elite.deaths < veteran.deaths`,
  `elite.hits_by_band["20-30"] > veteran.hits_by_band["20-30"]`, hit rate B < N < V < E, delay monotonic with
  Elite ≤ 0.18 s.

Baselines are in `Docs/Research/DeepSeek/baselines/` (`C106.flips.json`, `C108.flips.json`, `C108.duels.json`).

## Limits

- `STATE CONFLICT` is only emitted after the decision-commit patch; the baseline count is 0 by construction.
- `flips.ps1` counts logged state changes; if two writers change state in one tick, both lines are logged and counted
  (that is the point).
- `duels.ps1` needs `FIRE SHOT`/`FIRE HIT` lines (`wb_bot_fire_diagnostics` / move trace); without them it reports
  kills/deaths only.
- One session is not a trend; run the same script on the A and B halves of C-111 and compare the JSON fields.
