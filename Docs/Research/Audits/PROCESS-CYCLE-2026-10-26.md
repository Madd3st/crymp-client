# Process cycle v5 — reference for D-020

Date: 2026-10-26. This is the short reply to D-020; the canonical text stays in
`Docs/Research/Coordination/MAILBOX/PROTOCOL.md` § Task cycle v5. Do not fork a second process document.

## Accepted shape

| Step | Owner | Artifact | Stop / promote rule |
| --- | --- | --- | --- |
| 0 | Claude | `C-`/`DS-` item with what, why, acceptance metric, flag exit criterion | no acceptance metric → not taken |
| 1 | Claude (+DeepSeek review) | oracle: unit / compile / walk_sim / probe that is **red on the current bug** | oracle still green on the bug → no code |
| 2 | Claude | code behind a default-OFF flag | no flag → no merge |
| 3 | Claude | units + `compile_check` + `walk_sim` offline | red → fix; no build/run |
| 4 | DeepSeek | pre-batch diff review: real bugs only, `file:line` + scenario, ≤ 15 lines | real bug → back to step 2 |
| 5 | Codex | one frozen build/run with the exact requested flag set | evidence only; no silent rerun |
| 6 | owner | playtest or explicit verdict | promote only on evidence |

## Rules that came out of the audit

1. **The oracle is part of the task, not optional.** The corners v1/v2 story and X-088 (`navmesh=0` while
   `wb_bot_corners=1`) show that a 15×660 run cannot validate code that the recipe never enabled. The batch header
   must record effective cvars, not desired ones.
2. **One change, one flag, one exit criterion.** `promote` (default ON after evidence), `diagnostic` (keep OFF,
   measure), or `delete`. No “probably better” flags.
3. **Default-off is a testable promise.** Three flags in a batch means the control is the current default build; if
   the flags change `BotProfile` at add time, the header must say when they are set (before `wb_add`/roster).
4. **DeepSeek reviews the diff before Codex spends a run.** Review is read-only, real bugs only; style and “could be
   nicer” are not blockers. A batch with no pre-batch review is not ready.
5. **No idle.** After a push, fetch, read the mailbox, ACK/STARTED, and wait only for a bounded interval; a timeout is
   not an end state. Each push rewrites `NOW:` and the item state.
6. **PS runs in parallel but after IA's foundation.** IA first; T-0 (`IBotGameMode`, teams, team spawn) is the first PS
   code because without it `SetTeam`/spawn/registration remain IA-only. Vehicles are mandatory in PS v1 (D-019), but
   they sit behind `wb_ps_vehicles` so the no-vehicle mode is testable.

## Status vocabulary

`ACK` (≤10 min) → `STARTED` (ETA) → `PROGRESS` (30 min heartbeat or phase change) → `DONE` / `BLOCKED` / `REJECTED`.
The `NOW:` line is the first line under the outbox title and is rewritten on every push. A status-only commit is valid.
One item is `STARTED` at a time per agent unless the item says otherwise.

## What this changes for the next batches

- C-093 must ship the effective cvar list and the pre-batch review hash (DS-018) with the run.
- PS work starts with T-0 (`T0_GAMEMODE.md`), then the vehicle entry/exit probe (`VEHICLES.md`), then the 5-minute
  behaviour (`PS_MIN.md`); no PS feature without a team-spawn/registration oracle.
- The definition of done for PS v1 is the checklist in `PS_T0_SPEC.md`, not “bots spawned on Shore”.
