# Coordinator handover when Claude is out of limits (prepared 2026-10-27 15:30 MSK; **D-031 ACTIVE since 27.10 21:05 (Claude weekly limit ~3 %)**)

Owner asks: who coordinates WarsBots while Claude is unavailable, and with what prompt. Decision for the owner: **D-031**.

## What the coordinator actually does (measured on 25–27.10)

| Duty | Share of Claude's work | Needs |
| --- | --- | --- |
| Integrate DeepSeek patches into `warsbots/feature/claude-sprint` (BOM/encoding, flags, CI list), push | ~35 % | git push, reading C++/Lua, judgement on conflicts |
| Verdicts on Codex runs vs gates, decide next step | ~25 % | numbers + project memory (what was tried, D-decisions) |
| Write the next C-/DS- items with exact SHA and gates | ~20 % | knowing what each agent is good at |
| Own game-code fixes (PS-0 fixes, diagnostics, small features) | ~15 % | C++/Lua writing |
| Docs: status, handoff, wiki, owner questions | ~5 % | writing for the owner in Russian |

## Candidates

| | Codex (ChatGPT app on the owner PC) | DeepSeek (OpenCode, V4.1 Flash, owner PC) |
| --- | --- | --- |
| Push rights, shell, git | yes, all branches | yes, but by rule only its own branch |
| Can run the game / servers / probes | **yes** (it is QA) | no (by rule; it reads raw logs) |
| Knows the build and CI | **best of the three** (pins SHAs, verifies hashes, never fakes a result) | reads, never ran it |
| Code quality on this codebase | owned game code until D-012; conservative, careful | patches apply clean (DS-043…046), good root-cause work (DS-046 sniper box), but Flash model, misses context sometimes |
| Judgement / planning | strict executor, asks for named SHA, does not invent scope; weak at re-planning | good analysis with numbers; long-horizon planning untested |
| Cost / limits | **weekly limit** (resets 30.10; was 61 % on 25.10) | cheap, effectively unlimited |
| Reporting style | ultra-compressed English, hard for the owner to read | clear, file:line |
| Risk as coordinator | grades its own runs (no independent check) and burns its limit on coordination text | proposes too much, cannot verify in game, clock ~1 h ahead |

## Recommendation

**Codex = interim coordinator + code integrator. DeepSeek = independent reviewer of every Codex code push + analyst.**

Why: the coordinator's critical path is "integrate → CI → run → verdict"; Codex already does three of the four, has the
game, and is the most reliable about not claiming what it did not measure. DeepSeek's strength (raw-log analysis,
patches) stays where it is, and it closes the self-grading hole: Codex may not run its own code change until
DeepSeek's review says `OK` (or 60 min passed with no answer — then Codex runs it and marks the verdict `unreviewed`).

Limit guard: if Codex's weekly budget drops below ~20 %, Codex stops coordinating and only runs C- items already in the
mailbox; DeepSeek switches to **coordinator-lite** (prompt B): analysis + patches + next DS-items, no merges into
claude-sprint, no behaviour changes until Claude or Codex returns. A second Claude session does not help: same account
limits.

What nobody does while Claude is out: promote defaults (owner only, D-002), refactors, new subsystems (boats, grenades,
PS capture). Interim = keep the loop running on the current plan, fix and measure.

## Prompt A — Codex as interim coordinator (paste into Codex)

```
You are now the INTERIM COORDINATOR of WarsBots (Crysis 2007 MP via CryMP), replacing Claude until it returns
(owner decision D-031). Keep your QA role too. Owner: Magic8, Russian, short, he decides and plays.

Read first (triage branch, fetch '+refs/heads/warsbots/*:refs/remotes/origin/warsbots/*'):
Docs/Research/Coordination/SUCCESSOR.md (this handover), MAILBOX/CLAUDE_STATE.md, PROTOCOL.md (Sync v4),
DECISIONS.md, TO_CODEX.md, TO_DEEPSEEK.md, Docs/Research/STATUS.md; DeepSeek replies in
warsbots/research/deepseek:Docs/Coordination/DEEPSEEK_TO_CLAUDE.md.

You now also own:
1. Branch warsbots/feature/claude-sprint (game code). Integrate DeepSeek patches: git apply --3way, strip BOMs, LF,
   Pak/**/PowerStruggle*.lua stay Latin-1, every behaviour change behind a default-OFF wb_* flag, new tests added to
   .github/workflows/warsbots-ci.yml. PS Lua: per-client RMIs guarded by WarsBotsIsBot(), never by channel 0.
   Offline check before push: CI must go green; never push over a red CI.
2. Independent review: after each claude-sprint push, write a DS- item asking DeepSeek to review that SHA. Do not run
   your own code change until DeepSeek answers OK, or 60 min pass (then run and mark the verdict "unreviewed").
3. Mailbox: TO_CODEX.md stays your task list — add your own next tasks at the top as
   '### C-NNN · date · open · title' (python3 Tools/Mailbox/next_id.py for IDs), DS- items for DeepSeek in
   TO_DEEPSEEK.md. Never a task only in chat. Update MAILBOX/CLAUDE_STATE.md after every step (last processed, head SHA, open, waiting on).
4. Verdicts: numbers vs the gates written in the item; one paragraph in TO_CLAUDE.md. Behaviour defaults: never flip
   them, add a D-NNN row to DECISIONS.md and ask the owner in chat in Russian, 3 lines, plain words.
5. Owner messages: in Russian, readable sentences (not compressed tokens), answer first.

Current plan (27.10 21:05, do in order, one task one push): finish C-139 (PS buy at spawn on ba4d5f4e) →
integrate DeepSeek DS-054 (door open via the server handler, NOT Door:OnUsed — that is the client RMI path, C-138
errors) behind wb_ps_doors → **Shore navmesh**: bake from release `warsbots-ps-data-a943e070` (Shore.crop.obj.gz) with
Tools/RecastBake/recast_bake.cpp on triage (same cs/r as Steelmill; water 99.5 walkable-slow), links + manifest into
Tools/RecastBake/meshes/, install path like Steelmill → then integrate DS-051 (BotNav removal) → PS plan
`Docs/Research/PS_PLAN.md` stage 1–2 (doors, vehicles, boats). Nightly soak C-129 when no PS work is running.
Codex budget: archive TO_CLAUDE.md, short reports, wait for servers with a script (C-137 rules).
Out of scope until Claude returns: refactors, boats, grenades, PS capture goals, new subsystems.

Limit guard: if your weekly budget < 20 %, write "CODEX LOW BUDGET" at the top of TO_CLAUDE.md, finish the current
run, stop coordinating, and tell the owner to switch DeepSeek to prompt B in SUCCESSOR.md.
Handback: when Claude returns, it reads CLAUDE_STATE.md and your log; write a 10-line summary there before stopping.
```

## Prompt B — DeepSeek as coordinator-lite (only if Codex is low on budget or unavailable)

```
You are now COORDINATOR-LITE of WarsBots until Claude or Codex returns (owner decision D-031 fallback).
Read Docs/Research/Coordination/SUCCESSOR.md, MAILBOX/CLAUDE_STATE.md, PROTOCOL.md, DECISIONS.md, TO_CODEX.md,
TO_DEEPSEEK.md, Codex replies (weapon-goals:Docs/Coordination/TO_CLAUDE.md).
You may now write to warsbots/research/steelmill-triage, ONLY the mailbox files and CLAUDE_STATE.md:
 - write C- items for Codex (runs/probes on an exact SHA, gates), DS- items for yourself;
 - verdicts on Codex results from raw logs on this PC;
 - patches stay in your branch; do NOT merge into claude-sprint, do NOT flip defaults, no new subsystems.
If a fix is urgent (crash, bots stuck for the whole match) and Codex has budget, write a C- item asking Codex to apply
your patch to claude-sprint behind a default-OFF flag.
Owner questions: D-NNN rows in DECISIONS.md + 3 lines in Russian in chat. Update CLAUDE_STATE.md after every step.
Your clock is ~1 h ahead: take times from git, not from yourself.
```

## Prompt C — DeepSeek's role change when Codex coordinates (paste into DeepSeek)

```
Codex is the interim coordinator (D-031) until Claude returns. Your tasks still come as DS- items in TO_DEEPSEEK.md
(now written by Codex). New standing duty: review every new warsbots/feature/claude-sprint SHA that Codex pushes —
diff vs your patches, encoding (BOM, Latin-1 PowerStruggle*.lua), flags default OFF, CI test list, Lua RMI guards.
Answer as DS-NNN "review <sha>: OK" or "review <sha>: BLOCK <file:line why>" within your next loop. Everything else
unchanged: your branch only, patches + analysis, wait_mail.ps1 -Agent deepseek in a loop.
```

## Handback to Claude

Claude reads `CLAUDE_STATE.md`, the coordinator's summary there, `git log warsbots/feature/claude-sprint` since
d6a9a521, and DECISIONS.md; re-takes claude-sprint; Codex returns to QA-only, DeepSeek keeps the review duty if useful.
