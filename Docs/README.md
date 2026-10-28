# WarsBots: documentation entry point

Project: **Crysis 2007 multiplayer through CryMP**, repository `Madd3st/crymp-client`. This index is maintained on `warsbots/research/steelmill-triage`; game code is on `warsbots/feature/claude-sprint`.

## New to the project

- [Wiki home](Wiki/Home.md): choose a reading route.
- [First visit](Wiki/Getting-Started.md): purpose, terms and first demonstration.
- [Run and play](Wiki/Running-and-Playing.md): C150 Steelmill/Shore packages, controls and recovery.
- [Status and roadmap](Wiki/Status-and-Roadmap.md): verified capabilities and remaining acceptance work.
- [Troubleshooting](Wiki/Troubleshooting.md): failed launch or useful bug report.

## Engineering

- [Repository map](Wiki/Repository-Map.md): branch ownership, code paths, tools and local data.
- [Engineering guide](Wiki/Engineering-Guide.md): pinned source links and change workflow.
- [Architecture](Wiki/Bot-Architecture.md): lifecycle, decisions and execution.
- [Navigation](Wiki/Navigation.md): data bake, paths, transitions and water.
- [Commands](Wiki/Console-Commands.md): server/client boundaries and exact-version references.
- [Testing and evidence](Wiki/Testing-and-Evidence.md): what each check proves and the C149 example.
- [CS-bot maturity plan](Research/CS_BOT_MATURITY_PLAN.md): C152/DS096 priorities, concrete code issues, oracles, acceptance targets and work split; [wiki overview](Wiki/CS-Maturity-Plan.md).
- [Demos and logs](Wiki/Demos.md): artifacts, versioning and video correlation.

## Current work and handoff

[Research STATUS](Research/STATUS.md) is the current technical snapshot; [CLAUDE_STATE](Research/Coordination/MAILBOX/CLAUDE_STATE.md) is operational handoff; [PROTOCOL](Research/Coordination/MAILBOX/PROTOCOL.md) defines exact SHA, review and session rules. Tasks and owner decisions live alongside them. QA reports and source tools live on weapon-goals; independent reviews live on deepseek. Read each document on its owning branch. An old QA checkout's game code must never be used to assess current behavior.

## Publishing the wiki

`Docs/Wiki/*.md` is the source of the [GitHub Wiki](https://github.com/Madd3st/crymp-client/wiki). Commit source changes to triage, copy the exact committed blobs into the existing `crymp-client.wiki.git` checkout, inspect its diff, commit and push. Record source SHA and wiki SHA in the completion report. Validate relative wiki links against page filenames and GitHub source links against their stated branch/SHA. No binary build is required for Markdown changes.

Historical experiments remain evidence for their own version and recipe; they do not override current STATUS or mailbox. Existing SDK/component licenses remain in force.
