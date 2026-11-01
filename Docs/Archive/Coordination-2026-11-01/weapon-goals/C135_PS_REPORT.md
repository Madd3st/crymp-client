# C135 PS spawn / purchase check: FAIL

Exact **1a7ec84e8ed1f73d9f457e1b7e7e599e41507948**, green CI40537292508; downloaded client/server match artifact SHA256. One Shore six Normal bots, 300 seconds, capture/buy1, legacy navmesh/jump_links0, C119 combat flags + first_primary/withdraw_cover/tactics/fire_diagnostics1. Actorless effective recipe check passed before roster. No GUI, local build or gameplay edits.

Completed: 6014 demo frames, 300.724s demo window, deaths/wave **4/4**, crash/channel **0/0**, stalls/traps **102/1**. **Four Lua error episodes**, `CheckVirtualInventoryRestrictions(inventory, itemClassName)` expects String, received Null, stock powerstrugglebuying:984. Successful purchased items **0**, all four attempts primary/ammo ok0, despite PP100. All six initial PS SUMMARY rows confirm PP100/rank1/buys0. That summary is a startup snapshot, not end-of-run PP/capture accounting; total PP spent and completed captures remain UNKNOWN.

**Initial spawn gate fails.** First six creation positions below (teams are explicit pre-registration assignments):

| Bot | Team | Initial XYZ |
| --- | --- | --- |
| 1 | 1 | [1965.939, 1536.152, 100.928] |
| 2 | 2 | [1844.291, 1235.465, 111.545] |
| 3 | 1 | [1841.316, 1245.005, 111.545] |
| 4 | 2 | [2111.25, 1885.875, 103.45] |
| 5 | 1 | [2122.125, 1884.375, 100.2] |
| 6 | 2 | [1963.064, 1313.429, 118.495] |

Bot1/6 start at neutral bunker positions; bot2 team2 starts near NK base, bot5 team1 near the US submarine. These counterexamples disprove 100% own-base spawning irrespective of the precise base boundary. JSON retains all initial/reset creation rows. No `PS spawn group=` lines occurred: wave revives bypass BotInstance::DoRevive. Read-only source finding: C134 changed DoRevive, while initial BotManager::Add still chooses an IA spawn before BotInstance::Spawn assigns a team; BotInstance registers/equips at that position. Claude owns the correction. No workaround/forced grant/substitute build or repeated run.

Raw evidence: `build/c135-Shore-1a7ec84e/`, including arguments, effective flags, logs, readable demo, PS/error extracts, summary/metrics, original/final memory and hashes. Two pre-existing memory files restored with actual SHA256 verification after each phase; own QA server stopped, no client. Foreign Recording hash C08CF236... preserved. C131 equivalence remains pending; no promotion or soak on this failed PS gate.
