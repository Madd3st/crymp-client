# C100 scenario probes on CI ceb6e5b2

CI run40249051922, artifact SHA256 checked; setup fixed, native RNG independent.

| Probe | Repeat results | Overall | Median |
| --- | --- | --- | --- |
| DSG1 | FAIL / FAIL / FAIL | FAIL | no DSG1; FY71 pickup1.43s |
| Gauss | FAIL / FAIL / FAIL | FAIL | no Gauss; SCAR pickup7.55s |
| Elite vs Beginner | FAIL0:0 / FAIL0:0 / PASS1:0 | FAIL | kills0:0 |

Unreachable and TRAP RECOVERY counts0 in all nine trials. No crash.
Short duel results are too sparse to establish global difficulty quality.
Missing target pickup is censored/fails; absent kills in complete logs are0, so0:0 never passes ratio.

Evidence build/c100-comparison.json, build/c100-{dsg1-final,gauss,duel}-ceb6e5b2/probe-result.json.
First DSG1 trial recovered exactly from completed v2sidecar; only logging/process teardown fixes, no duplicate completed gameplay trial.
Preflight checks settings before any bots; engine startup serializes map then setup commands as existing Test-WarsBots runner.
All memory/nav restored to initial hashes; no GUI; no gameplay code fixes.
