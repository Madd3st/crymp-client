# C126 step3 / C130: Shore QA; C127 gate FAIL

Exact claude-sprint **4f3a1d72e1269a644c3e602701ca170b5b59d044**; [CI40526830694](https://github.com/Madd3st/crymp-client/actions/runs/40526830694) completed success. Downloaded exact artifact and verified SHA256 against both downloaded and used client/server pair; 30 unit-run logs available. No local build or game-code edits.

Three isolated six-bot 300-second arms completed. Same restored initial memory and Normal/profile roster, 3 bots/team; independent native RNG. Actorless preflight verifies effective CVars before each roster. Recipe deliberately follows the prior C121 PS test: navmesh/jump_links 0, high_ground/threat_scan/vantage 0, state_commit 1. It is not a C119 feature acceptance run or a navmesh-loaded test. Full effective values and command lines are in each raw arm directory.

| Arm | Deaths/wave seen | Stalls/traps >=60s | Successful items | Buy skips | Lua/channel/crash | Demo frames |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| base | 4/4 | 72/1 | 0 | 0 | 0/0/0 | 6077 |
| capture | 3/3 | 96/1 | 0 | 0 | 0/0/0 | 6082 |
| capture-buy | 3/3 | 113/0 | 0 | 3 | 0/0/0 | 6073 |

**C127 blocked:** capture+buy logs three `PS BUY skip pp=0` and zero successful bot purchases, failing the required >=1 purchase. `PS CAPTURE target=...` proves target selection only. Five capturable points found; completed captures/uncaptures per team remain UNKNOWN because no completion/team-change telemetry was emitted by this pinned build. Total PP spent is UNKNOWN; successful purchases observed 0. No fabricated 0 capture count, forced PP grants, substituted SHA, or gameplay changes.

No C127 smoke, preset, or client launch. Other C126 steps and C131 have not run. Shore navmesh is absent: installed and exact-source manifests cover the four IA maps only; C121 PS used navmesh0. The requested C127 navmesh wording needs a concrete accepted PS data recipe before it can prove navmesh-loaded readiness.

Raw local archives: `C:/Users/Magic8/WarsBotsBuild/crymp-multibot/build/c126-ps-{base,capture,capture-buy}-4f3a1d72/`. Each has engine/bot logs, readable demo, warsbots-run-summary/1, PS metrics, raw capture/buy/error evidence, effective-config/argument receipts, QA harness copies and hash records. Original LOCALAPPDATA logs/demos retained.

Triage's legacy native-revive count stays zero because PS wave path uses different telemetry; the table uses explicit `PS REVIVE SEEN source=wave` and matches all observed deaths. Initial actor registration logs include the ordinary round reset; count of registration events is not unique participant count. Stalls/traps are parser results, not visual QA.

All original memory files restored and verified by actual SHA256 after all arms; own servers stopped, no client active. Foreign dirty `Tools/Play-WarsBotsRecording.ps1` preserved. See [C126_PS_RESULTS.json](C126_PS_RESULTS.json) for hashes, schemas, complete per-arm summary and UNKNOWN fields.
