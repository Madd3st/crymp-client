# Claude ↔ Codex mailbox protocol (v1, 2026-10-24)

Goal: agents talk through git, the owner only nudges and decides.

## Where each side writes (never in the other's branch)

| Who | File | Branch |
| --- | --- | --- |
| Claude → Codex | `Docs/Research/Coordination/MAILBOX/TO_CODEX.md` | `warsbots/research/steelmill-triage` |
| Codex → Claude | `Docs/Coordination/TO_CLAUDE.md` | `warsbots/feature/weapon-goals` |
| Both → owner | `Docs/Research/Coordination/MAILBOX/DECISIONS.md` (Claude keeps it; Codex adds `NEEDS OWNER` items to `TO_CLAUDE.md`) | steelmill-triage |

Read the other side with `git fetch origin <branch>` + `git show FETCH_HEAD:<file>`. No merges.

## Message format (append at the top, newest first)

```
### C-007 · 2026-10-24 13:05 · request · open
Refs: weapon-goals 32b037fc, triage 03e6567
Needs owner: no
<body: what, why, acceptance check — short; details in a linked file>
```

* IDs: `C-NNN` from Claude, `X-NNN` from Codex. Never reuse or renumber.
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

**Claude**, when nudged ("проверь ящик" / "Codex запушил") or on its schedule:
1. fetch `weapon-goals`, read new commits + `TO_CLAUDE.md`;
2. copy new logs/summaries from the owner PC read-only, run Triage;
3. write verdicts/requests as `C-` items, update `DECISIONS.md`, push; tell the owner 3 lines max.

## What still needs the owner

Only: decisions in `DECISIONS.md` (default ON, self-kill policy, merges, server/client sessions, anything
irreversible), live play/video, and the nudge if an agent is idle. Not copying messages.

## Rules carried over

Codex owns game code, builds, servers, QA. Claude edits only its research branches, never merges, never
stops servers. Default OFF until a paired soak shows no regression.
