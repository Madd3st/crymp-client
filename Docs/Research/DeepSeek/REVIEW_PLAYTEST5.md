# Playtest 5 (C-119) re-grade — DS-045 #2

C-119: `C119-6e2713f9-20261027-105159-Server.bots.log`, 8 bots, **1,436.5 s**, 274 kills / 284 deaths, build
`6e2713f9` (pre-DS-044; first/withdraw/finish are the baseline for the new flags). c116/c117 = 15-bot night batch.

| Metric | C-119 | c117 (0fa9c1cb) | c116 (fd12a5eb) |
| --- | ---: | ---: | ---: |
| first primary p50 / p90 (s) | 35.55 / 61.74 | 33.67 / 73.99 | 33.48 / 57.72 |
| SOCOM share of TACTIC samples | **25.9 %** | 44.0 % | 35.7 % |
| withdraw deaths within 2.5 s | **142 / 193 = 73.6 %** | 81.8 % | 79.8 % |
| switch-away from <=25 HP | **N/M** | 17.9 % | 22.4 % |
| high ground (z >= 140) | **26.8 %** | 19.9 % | 21.1 % |
| height kills / known | 8 / 271 = 3.0 % | 10 / 293 = 3.4 % | 9 / 287 = 3.1 % |
| accuracy (shots -> hits) | **N/M** | 32.3 % | 29.8 % |
| hit distance p50 / p90 (m) | **N/M** | 13.38 / 23.49 | 12.41 / 21.27 |
| target distance p50 / p90 (m, proxy) | 14.34 / 28.85 | 15.33 / 28.82 | 14.83 / 29.12 |
| jumps / bot-min (total) | 1.88 (15.04) | 2.17 (32.54) | 1.96 (29.36) |
| link failures / starts | 52 / 182 = **28.6 %** | 33.1 % | 30.4 % |
| NOFIRE / min (blocked / ammo / aim) | 52.17 (588 / 461 / 200) | 108.13 (456 / 549 / 188) | 0 (no NOFIRE) |
| bursts per kill | 21.6 | 24.4 | 19.8 |

- **Moved vs c117:** SOCOM 44.0 -> 25.9 %, withdraw deaths 81.8 -> 73.6 %, high ground 19.9 -> 26.8 %, link failures
  33.1 -> 28.6 %, NOFIRE 108.1 -> 52.2/min, shots/kill 24.4 -> 21.6.
- **Did not move:** first primary 33.7 -> 35.6 s (p90 74.0 -> 61.7), height kills 3.4 -> 3.0 %.
- **N/M (not measurable in C-119):** `FIRE SHOT` = 0, `FIRE HIT` = 0 (`wb_fire_diagnostics` off), so accuracy, hit
  distance and switch-away from <=25 HP cannot be measured; target distance is the `TACTIC SAMPLE visible=1` proxy.
- **Caveat:** C-119 is 8 bots / 24 min vs 15 bots / 11 min; the night batch is the cleaner A/B.
- **For DS-044:** the three new flags should move first primary (35.6 s), withdraw deaths (73.6 %) and switch-away
  (baseline only in c116/c117: 17.9-22.4 %). Re-run the same C-119 recipe with each flag.
