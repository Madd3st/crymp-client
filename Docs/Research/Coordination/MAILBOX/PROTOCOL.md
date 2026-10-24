# WarsBots agent mailbox protocol (v2, 2026-10-24: Claude, Codex, OpenCode)

Goal: agents talk through git, the owner only nudges and decides.

## Where each side writes (never in the other's branch)

| Who | File | Branch |
| --- | --- | --- |
| Claude → Codex | `Docs/Research/Coordination/MAILBOX/TO_CODEX.md` | `warsbots/research/steelmill-triage` |
| Codex → Claude | `Docs/Coordination/TO_CLAUDE.md` | `warsbots/feature/weapon-goals` |
| Claude → OpenCode | `Docs/Research/Coordination/MAILBOX/TO_OPENCODE.md` | `warsbots/research/steelmill-triage` |
| OpenCode → Claude | `Docs/Coordination/OPENCODE_TO_CLAUDE.md` | `warsbots/research/movement-params` (OpenCode's own branch) |
| Claude modules (D-010) | new self-contained files only, `Code/CryGame/Bots/Bot*.h` + `Tests/*Tests.cpp` | `warsbots/research/claude-modules` (branched from weapon-goals; Codex cherry-picks/merges into weapon-goals) |
| All → owner | `Docs/Research/Coordination/MAILBOX/DECISIONS.md` (Claude keeps it; others add `Needs owner: yes` items to their outbox) | steelmill-triage |

Codex and OpenCode do not write to each other directly; Claude coordinates. OpenCode never edits game code,
`warsbots/feature/*`, `warsbots/main` or other agents' branches, never runs the game or servers, never merges.

Read the other side with `git fetch origin <branch>` + `git show FETCH_HEAD:<file>`. No merges.

## Message format (append at the top, newest first)

```
### C-007 · 2026-10-24 13:05 · request · open
Refs: weapon-goals 32b037fc, triage 03e6567
Needs owner: no
<body: what, why, acceptance check — short; details in a linked file>
```

* IDs: `C-NNN` from Claude to Codex, `OC-NNN` from Claude to OpenCode, `X-NNN` from Codex, `O-NNN` from OpenCode.
  Never reuse or renumber.
* Types: `request`, `report`, `question`, `answer`, `decision-needed`.
* Status lives on the original message and is updated by its **receiver** in the receiver's own file as a
  reply line: `Re C-007: acked | done <sha> | rejected <why> | blocked <on what>`.
* One message = one topic. Numbers carry evidence type: **src / unit / server / client / graph**.
* `Needs owner: yes` → also listed in `DECISIONS.md`. Agents do not act on those until the owner answers.

## Loop

**Codex**, at the start of every step and after every push:
1. fetch `steelmill-triage`, read `TO_CODEX.md` for new/open `C-` items;
2. ack or answer them in `TO_CLAUDE.md`, push with the code commit;
3. QA runs write `summary.json` (warsbots-run-summary/1) as already wired.

**OpenCode**, at the start of every session and after every push: fetch `steelmill-triage`, read
`TO_OPENCODE.md` for new/open `OC-` items, reply `Re OC-NNN: acked | done <sha> | rejected | blocked` in its
outbox, commit deliverables only into its own branch.

**Claude**, when nudged ("проверь ящик" / "Codex запушил") or on its schedule:
1. fetch `weapon-goals`, read new commits + `TO_CLAUDE.md`;
2. copy new logs/summaries from the owner PC read-only, run Triage;
3. write verdicts/requests as `C-` items, update `DECISIONS.md`, push; tell the owner 3 lines max.

## Mail loop (v3, 2026-10-24 17:10) — agents never sit idle waiting for the owner

* After finishing a task (pushed + replied), an agent **does not end its turn**: it runs
  `Tools\Mailbox\wait_mail.ps1 -Agent <codex|opencode> -Repo <its clone>` and repeats it on timeout (exit 1).
  On exit 0 it acts on the printed items. It stops the loop only on an owner message or a blocker it reports.
* At the start of every turn (including owner messages about something else) run `wait_mail.ps1 -Peek` once.
* Safety net: scheduled task "WarsBots Mailbox Wake" (`Tools/Mailbox/register_wake_task.ps1`) restarts a stopped
  agent through its CLI when it has an open item and its clone is idle 20 min. See `Tools/Mailbox/README.md`.
* Claude checks the other outboxes on its own schedule (send_later check-ins) and on owner nudges.

## What still needs the owner

Only: decisions in `DECISIONS.md` (default ON, self-kill policy, merges, server/client sessions, anything
irreversible), live play/video, and the nudge if an agent is idle. Not copying messages.

## Rules carried over

Codex owns game code, builds, servers, QA. Claude edits only its research branches, never merges, never
stops servers. Default OFF until a paired soak shows no regression.
