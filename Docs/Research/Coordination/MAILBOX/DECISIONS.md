# Owner decisions (the only file the owner must read)

Answer in chat with Claude or Codex as `D-001: yes` etc. Agents record the answer here / in TO_CLAUDE.md.

| ID | Question | Recommended | Status |
| --- | --- | --- | --- |
| D-001 | Keep the native self-kill (45 s, watchdog level 4) as last-resort trap recovery in IA? It counts as a suicide in score. | yes, until spawn 195 is fixed | open |
| D-002 | May agents promote a flag to default ON by themselves when paired runs show no regression, reporting after the fact? (otherwise each flip waits for you) | yes for diagnostics/logging; no for behaviour changes | open |
| D-003 | 10-minute live session when Codex asks: `wb_revive_at 195 me` (you get revived on spawn 195) + video with `r_displayInfo 1` | yes, when C-002 is ready | open |
| D-004 | Move WarsBots logs out of OneDrive (sync can lock files being written)? | yes | open |
