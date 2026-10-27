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

## Sync v4 (2026-10-27, D-030) — nobody waits for the owner

* **Every task goes into the mailbox** as a C-/DS- item, even when the owner also pastes it into chat.
* Wake loop on the owner PC (`register_wake_task.ps1 -CodexRepo .. -DeepSeekRepo ..`, every 10 min): wakes an idle
  agent for any item newer than its last wake. Never starts anything while the owner's game client is open.
* Claude wakes on a cloud schedule (hourly, active since 27.10 15:19): reads `TO_CLAUDE.md` + `DEEPSEEK_TO_CLAUDE.md`, writes verdicts,
  integrates patches, pushes claude-sprint, writes the next C-/DS- items. Last processed IDs live in
  `Docs/Research/Coordination/MAILBOX/CLAUDE_STATE.md` on triage.
* Agents finish with `wait_mail.ps1` in a loop, as in v3. The owner only decides (DECISIONS.md) and plays.

## Which branch has the code (27.10 — DeepSeek DS-S7 graded the wrong branch)

* **Game code = `warsbots/feature/claude-sprint` only.** CI builds it; Codex runs exact claude-sprint SHAs.
* `warsbots/feature/weapon-goals` = Codex's **mail and reports only** (TO_CLAUDE.md, X- reports, JSON). Its code is
  stale since D-012 (25.10) and must never be read, graded or built.
* `warsbots/research/deepseek` = DeepSeek's patches/docs; `warsbots/research/steelmill-triage` = mailbox, docs, wiki source.
* Before upstreaming: rename/retire `weapon-goals` code so the layout cannot mislead (UPSTREAM_READINESS).

## Session hygiene (27.10, owner)

* A long-lived agent session is restarted when its context passes **50 %** or once a day, whichever comes first
  (DeepSeek session from 25.10 reached 67 % / 672k tokens). Nothing is lost: memory = mailbox + docs, not the chat.
* Before the restart the agent pushes everything; the new session starts from PROTOCOL.md + its mailbox file.
* The wake loop must target the current session (Codex updates the DeepSeek session id after each restart).

## Rules added 27.10 (DeepSeek audit, owner)

* **Done = SHA.** A status line "integrated / fixed / done" counts only with the commit SHA on the same line.
  Plans are written as plans ("next: …"), never as states.
* **Net-zero flags.** A batch that adds a `wb_*` flag also promotes or deletes one, named in the same C-/DS- item.
  Exception: diagnostics-only flags, which must name their exit date.

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

## Task states (v4, 2026-10-26 00:40, D-017) — Claude must always know who started what

Every `C-`/`DS-` item moves through explicit states, each written by the **receiver** in its own outbox and pushed
**immediately** (a status-only commit is fine and expected):

| State | When | Line to write (top of outbox, own entry) |
| --- | --- | --- |
| `ACK` | within 10 min of seeing the item | `### X-077 · <time> · ACK C-082 · queued after C-081` |
| `STARTED` | the moment work begins | `### X-078 · <time> · STARTED C-082 · ETA 40 min · build a943e070` |
| `PROGRESS` | every 30 min while running (heartbeat), or on a phase change | `### X-079 · <time> · PROGRESS C-082 · Shore done, Plantation exporting` |
| `DONE` / `BLOCKED` / `REJECTED` | at the end | normal report, header names the state and the item |

Plus a **`NOW:` line** as the very first line under the outbox title, rewritten on every push:
`NOW: C-082 STARTED 00:05 (ETA 00:45) · next C-083, C-084, C-085 · last push 00:20` or `NOW: idle, mailbox empty`.

Rules: no silent work longer than 30 min; "idle, waiting" is a valid NOW and must be written; one item STARTED at a
time per agent unless the item says otherwise. Claude reads the board with `python3 Tools/Mailbox/board.py`
(fetches all branches, prints every open item with its last state and age, flags >30 min silence).

## Task cycle v5 (2026-10-26, D-020) — catch cheap, promote only on evidence

| Step | Who | Cost | Stop rule |
| --- | --- | --- | --- |
| 0. `C-`/`DS-` item: what, why, acceptance metric, flag exit criterion (promote / diagnostic / delete) | Claude | cheap | no acceptance → not taken |
| 1. **Oracle before code**: unit / compile / walk_sim / probe that catches exactly this bug | Claude (+DeepSeek review) | cheap | oracle must be red on the current bug |
| 2. Code behind a default-OFF flag | Claude | medium | — |
| 3. Offline green (units + compile_check + walk_sim) | Claude | seconds | red → fix, no build |
| 4. DeepSeek pre-batch review of the diff | DeepSeek | cheap | real bug → back to 2 |
| 5. Build — one per batch, several C- items together | Codex | expensive | compile error → trivial fix + report |
| 6. Probe 1–3 min (scenario) | Codex | medium | FAIL → stop, no soak |
| 7. 15-bot pair — acceptance / default change only, **max one per day** | Codex | expensive | recipe (feature preset + effective cvars) printed and checked before start |
| 8. Verdict by `REPORT_SCHEMA` numbers, not by eye | Claude | cheap | — |
| 9. Merge green → weapon-goals → main every 2–3 days; STATUS + one outbox line | Codex / Claude | cheap | — |

Owner: decisions (`DECISIONS.md`) and feel playtests only — never regression hunting. A "defaults-only" run without the
feature preset is not a valid acceptance run.
