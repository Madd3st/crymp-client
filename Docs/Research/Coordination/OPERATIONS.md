# Working map and artifact lifecycle

This page is the short operating map. Start with [current state](MAILBOX/CLAUDE_STATE.md), [active Codex tasks](MAILBOX/TO_CODEX.md), [active DeepSeek tasks](MAILBOX/TO_DEEPSEEK.md), and [protocol](MAILBOX/PROTOCOL.md). Read historical mail only when the current task references it.

## Which source answers which question

| Question | Authority | Working copy |
| --- | --- | --- |
| What game code runs? | claude-sprint exact SHA + full CI artifact | QA/build/ds076-code |
| What was actually tested? | weapon-goals X-report + JSON + immutable logs/demo | QA root; build/c154-route-* retained |
| What should happen next? | steelmill-triage current mailbox + owner instruction | QA/build/ds076-triage |
| What did an independent reviewer establish? | research/deepseek report SHA | Temp/opencode/wb-review/deepseek |
| What can a newcomer play? | reviewed versioned package + disclosed limitations | Steelmill classic on E; Shore diagnostic only |

QA root is `C:/Users/Magic8/WarsBotsBuild/crymp-multibot`. Its own Code/ directory is historical; do not inspect it for current behaviour. A proposed patch on DeepSeek is not integrated game code. A green build is not physical or visual acceptance.

## Ready classic demonstration

`E:/WarsBots/Showcases/Steelmill-Classic-abac72a9/START.cmd`: Instant Action Steelmill, 2Beginner/2Normal/2Veteran/2Elite, IDs1–8 in that order. Sourceabac72a9/fullCI40890940918/DS1215c12d27b. CanonicalC119 navigation. F6/F7 select a bot, F1 view. The launcher validates all eight created difficulty profiles before opening the client, snapshots its own Memory/Navmesh namespace, archives logs/demo and restores on normal exit. Keep its PowerShell window open. RESTORE.cmd is recovery, CHECK.cmd does not launch a game. Mixed roster packaged/reviewed, not freshly played for packaging.

Old b8 packages on C remain immutable. The abac Shore preview is diagnostic: top exit stuck, delayed revive is not a fresh ladder presentation after the earlier link ban. Do not promote it as playable Shore or full PS.

## Storage tiers

| Tier | Location | Retention |
| --- | --- | --- |
| Current code/mail/tools | Git, small active checkouts on C | Preserve active/uncommitted changes; one authority per branch |
| New packages and large future evidence | E:/WarsBots/Showcases and E:/WarsBots/Evidence | Versioned, exact SHA, manifest + recovery |
| Closed raw exports/review snapshots | E:/WarsBots/Archive/YYYY-MM-DD | ZIP payload hashes + original path + restore instructions |
| Current nav/memory | Launcher's LOCALAPPDATA/WarsBots | Snapshot/restore only in same process namespace |
| Owner videos and live app data | Original owner/app paths | Never delete as build cache |

Codex MSIX may read the packaged namespace (`Packages/OpenAI.Codex_2p2nqsd0c76g0/LocalCache/Local/WarsBots`), while owner's normal shell reads plain Local/WarsBots. Keep these distinct; a successful restore in one does not prove the other. Do not cross-copy trees to make counts match.

## Safe cleanup and restoration

1. Measure bytes first; identify active dependencies and owners. No broad recursive deletion, no git clean -fdx, no history rewrite.
2. A clean retired worktree is preserved by exact HEAD archive ref. A dirty retired worktree also needs a complete verified ZIP (tracked/untracked/ignored files), staged and unstaged binary patches, status and HEAD bundle. Exclude the .git pointer from ZIP; never drop a live worktree.
3. Recheck unchanged HEAD/status and all source/archive hashes immediately before deleting only the recorded absolute target. Reject reparse points/nested repositories; preserve current foreign edits.
4. Geometry archive: extract the named payload to its original directory only when needed; verify its SHA256 from manifest.json. Crops/sidecars still on C. Historical receipts keep original paths; use archive manifest to resolve relocation.
5. Worktree restore: verify worktrees.bundle; fetch its recorded refs into the repository, add a detached worktree at restore_ref. Apply nonempty staged.patch with git apply --index, then unstaged.patch with git apply. Extract the payload ZIP over that worktree (excluding .git); verify file hashes/status. Do not overwrite an existing checkout.
6. Remove generated binaries/caches only after evidence/provenance or a reproducible source recipe survives. Never delete active C154 attempts just because they failed. Correspondence moves out of active mail into dated archives, preserving exact review SHAs and corrections.

## Session and documentation hygiene

Current state is one concise page; no chronological stack of obsolete NOW blocks. Active mail holds actionable tasks; dated archives preserve old wording, including stale open labels, which are not instructions to rerun. Keep an evidence index linking source/CI/review/runtime result and unknowns. Restart a long-lived agent at 50% or daily after clean push/idle; update wake to the new session. Do not delete app databases/session JSON to imitate archival.

Additional model trial: use a third model for independent video/audio timeline analysis and a bounded source review, with the same branch/task/evidence rules. Keep one integrator and exact runtime gates. Measure useful verified findings before buying a long subscription or adding another writer to the same files.
