# C126 remaining sniper probes: FAIL; native comparison blocked

Exact **493f2cab2e58f814db4065148da423b691a711c3**, accepted for remaining C126 by C131 / 7e545900, green CI **40528417384** and frozen binary hashes verified. Legacy harness: DSG1 3x60 seconds, Gauss 3x75, sniper_reach1/link_failfast1, navmesh1/jump_links1/vantage1/arbiter0, canonical manifest-verified data. Preflight and all six trial effective recipes pass. Same historical fixture memory restored between trials; native independent RNG. No completed successful run was repeated.

| Probe | Observed pickups | Unreachable per trial | LINK failures per trial | Failure reasons, total |
| --- | --- | --- | --- | --- |
| DSG1 | 0/3 | 1,1,1 | 0,1,2 | missed_landing 3 |
| Gauss | 0/3 | 0,0,0 | 3,6,4 | missed_landing 5; no_energy 3; no_takeoff 5 |

DSG1 gate **FAIL (requires >=2/3)**. Gauss also FAIL. All six trials report zero stalls; each Gauss trial completes one Strength link, landingZ 139.07/139.09/139.10. Link completion does not establish pickup. Assertions, raw first40 evidence per trial, native metrics, effective settings and SHA256s are in [C126_SNIPER_RESULTS.json](C126_SNIPER_RESULTS.json).

Native runner step1 remains **BLOCKED before QA**: exact native ParseScenario does not process repository JSON `setup` arrays; BuildSetupCommands therefore omits wb_add/teleport. `wb_probe_run` also requires id/seconds, absent in the old mailbox recipe. No zero-actor test, invented comparison or gameplay change. Claude must supply a compatible scenario/recipe. Step3 already completed, not repeated. C126 steps4-6 (15-bot runs / owner preset) **NOT RUN** after failed probe gate; no nightly C129 soak started under the protocol's probe-FAIL stop rule. C131 and C137 already reported separately, not repeated.

Previous worker left `build/c126-sniper_reach_dsg1-493f2cab/` with preflight and partial first trial, no result/restoration receipt. Intermediate memory/nav archived, original C126 snapshots hash-verified and recovered before this resumed batch. Fresh output roots `build/c126-resume-sniper_reach_{dsg1,gauss}-493f2cab/` preserve the interrupted evidence. DSG1 completed three trials; its expected assertion-failure exit was raised as NativeCommandError by the outer wrapper, which nevertheless restored memory/nav. Only the unfinished Gauss batch then ran through a wrapper that handles nonzero assertion exits as data. No completed trial rerun.

Final actual original memory/nav hashes verified. Own servers stopped; no game/client active, no GUI/local build/gameplay edits/promotion. Foreign dirty Recording.ps1 hash remains C08CF236BB4FEC9B30A73775D11CBA0BA26766E1DC5FD30D8851B16EBCECD710; untracked owner wrapper retained. Current native-runner and sniper failures need Claude's correction before further acceptance QA.
