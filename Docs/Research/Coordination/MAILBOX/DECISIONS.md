# Owner decisions (the only file the owner must read)

Answer in chat with Claude or Codex as `D-001: yes` etc. Agents record the answer here / in TO_CLAUDE.md.

| ID | Question | Recommended | Status |
| --- | --- | --- | --- |
| D-001 | Keep the native self-kill (45 s, watchdog level 4) as last-resort trap recovery in IA? It counts as a suicide in score. | yes, until spawn 195 is fixed | **yes** (owner, 14:27) |
| D-002 | May agents promote a flag to default ON by themselves when paired runs show no regression, reporting after the fact? (otherwise each flip waits for you) | yes for diagnostics/logging; no for behaviour changes | **logs/diagnostics yes, behaviour no** (owner, 14:27) |
| D-003 | 10-minute live session when Codex asks: `wb_revive_at 195 me` (you get revived on spawn 195) + video with `r_displayInfo 1` | yes, when C-002 is ready | **yes** (owner, 14:04) |
| D-004 | Move WarsBots logs out of OneDrive (sync can lock files being written)? | yes | **yes** (owner, 14:27) |
| D-005 | Close spawn 195 with a workaround today: bots never revive on 195 + general guard "no movement 2 s after revive → re-revive on another spawn"; root cause parked low priority. Then priority: pace (C-013) before graph (C-011)? | yes | **yes, workaround + guard; pace first** (owner, 16:20) |
| D-006 | Navigation: replace the self-built ray-sampled graph with a Recast navmesh generated from each map's geometry (all IA maps: Steelmill, Quarry, Armada, Outpost; PS later); stop graph repair (C-011); map-specific fixes only as data, never code. When? | yes, after pace (C-013) | **yes, after pace** (owner, 18:19) |
| D-007 | Turn `wb_bot_pace` ON by default? Paired runs: median 2.49→4.48 m/s, >4 m/s 4→76 %, stalls −40 %, pickups +35 %, no regression; sprint still 22 % (C-021). | yes, after the owner playtest feels right | **yes** (owner, 19:36, after OwnerPace7) |
| D-008 | Re-plan after external audit (Grok 4.7, 20:05): navmesh offline verdict and link executor before aim polish; perception (view cone, cloak) before aim model; goal arbiter instead of a flag; probes (30–90 s scenarios) instead of soaks for non-default commits; refactor wave 1 = R1+R3+R4 only; in-game demo playback after navmesh is green on 2 maps; no ML this quarter (time-scale experiment may revisit). | yes | **yes** (owner, 20:08) |
| D-009 | C-021 sprint changes ON by default (sprint 23→37 %, stalls 78→71, pickups 119→130, no regression; remaining gap = 40 % of moving time is ATTACKING, sprint there 9 %). | yes | **yes** (owner, 20:08) |
| D-010 | Claude writes self-contained engine-free logic modules (headers + unit tests compiled offline) in `warsbots/research/claude-modules`; Codex reviews, wires them into existing files, builds and runs. Claude never edits existing Codex files; OpenCode reviews Claude modules too. | yes | **yes** (owner, 20:27) |
