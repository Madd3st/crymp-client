# C131 fundament wave: no evident IA regression; PS stable with known purchase gap

Exact green CI artifacts **4f3a1d72 / 40526830694** and **493f2cab / 40528417384**, both client/server SHA256 pairs verified. One Steelmill six-Normal-bot 300-second arm per build, identical starting memory manifests and roster; C119 combat preset, navmesh/jump_links1, state1, first_primary/withdraw_cover0 in both arms. Independent native RNG. Actorless effective recipe checks passed; both real arms loaded the manifest-verified Steelmill navmesh. No local build/gameplay edits/GUI.

| Metric | 4f3a1d72 | 493f2cab |
| --- | ---: | ---: |
| Kills | 39 | 44 |
| Stalls | 5 | 5 |
| Traps >=60s | 0 | 0 |
| A->B->A / bot-min | 9.69 | 7.01 |
| Reversals / bot-min | 21.94 | 19.32 |
| First primary p50, s | 4.043 | 3.744 |
| First primary p90, s | 12.172 | 14.408 |
| Observed primary lives | 33 | 36 |
| Eligible lives | 44 | 48 |
| Demo frames | 6033 | 6034 |

No evident regression in these single samples: kills increase, stalls/traps unchanged, first-primary timing close, decision churn lower. This does not establish a causal improvement or statistical equivalence; the mailbox gave no numeric noise threshold and no additional repeats are authorized here. Use 493f2cab for remaining C126 QA as permitted by C131; no gameplay/default promotion.

Shore 493f2cab, six Normal bots x300, capture/buy1, navmesh/jump_links0: **4 deaths / 4 wave revives; crash/Lua/channel0**, 89 stalls/0 long traps, 6035 readable demo frames. `wb_ps_summary` parses: five points, initial team-held0/0, six bots PP0/rank1/buys0. It is an initial snapshot, not final accounting. Three PP0 skips, zero observed purchases. Capture completion and total PP spent UNKNOWN. This is the known pre-C134 purchase gap; C135 on newer 1a7ec84e already established PP100 plus persistent buy Lua failures and wrong initial spawns.

First primary uses selected demo frames plus pickup/inventory ownership; initial alive lives are censored, no-primary lives retained in the eligible denominator. A->B->A/reversals use the existing DeepSeek flips.ps1; not a new metric definition.

Raw `build/c131-{ia-Steelmill-4f3a1d72,ia-Steelmill-493f2cab,ps-Shore-493f2cab}/` retain commands, effective flags, logs, demos, metrics, memory snapshots and hashes. `build/c131-nav-before/` and nav-restored.json preserve the exact installed navmesh. Every original memory and nav file restored with actual SHA256 verification; run-created memory files archived before restoring absent-file state. Own servers stopped/no client, foreign Recording C08C unchanged. No already-successful run repeated.
