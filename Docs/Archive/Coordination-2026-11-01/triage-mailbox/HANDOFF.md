# Handoff for the next coordinator (updated 2026-10-27 21:20 MSK)

**D-031 is ACTIVE since 27.10 21:05** (Claude weekly limit ~3 %): Codex coordinates and integrates
(`SUCCESSOR.md` prompt A), DeepSeek reviews every Codex code push (prompt C). Live state and the ordered plan:
`MAILBOX/CLAUDE_STATE.md`. When Claude returns it reads that file first.

Repo `Madd3st/crymp-client` (public, fork of `crymp-net/crymp-client`). Read this file, then on
`warsbots/research/steelmill-triage`: `MAILBOX/CLAUDE_STATE.md`, `DECISIONS.md` (D-001…D-033), `PROTOCOL.md` (Sync v4),
`Docs/Research/STATUS.md`, and run `python3 Tools/Mailbox/board.py`. Fetch ALWAYS with
`git fetch origin '+refs/heads/warsbots/*:refs/remotes/origin/warsbots/*'` (a plain fetch hides other agents' work).

## Owner and style

- **The game is Crysis (2007) multiplayer via CryMP, not Crysis Wars.** "WarsBots" is only the project name.
- Owner = Magic8 (madd3st). Russian, humour, mild swearing OK, short chat, details in files. Phone screen: answer first.
- Owner decides (DECISIONS.md) and plays 10-minute "feel" playtests; impressions are signals, not specs. Before every
  playtest give him a short checklist of what changed and what to look at.
- Owner's PC is the only game machine. No runs while he plays; otherwise runs any time, incl. 15-bot (D-029).

## Roles

| Agent | Normally | While D-031 is active |
| --- | --- | --- |
| Claude | game code (`warsbots/feature/claude-sprint`), coordination, verdicts, docs, DeepSeek patch integration | out |
| Codex (ChatGPT app, owner PC) | CI artifacts (no local builds), runs/probes/presets, playtest archives → `weapon-goals:Docs/Coordination/TO_CLAUDE.md` (X-) | + coordinator: C-/DS- items, verdicts, integrates DeepSeek patches into claude-sprint |
| DeepSeek (OpenCode, owner PC) | raw-log analysis, `git apply`-able patches, engine-free modules + tests in `warsbots/research/deepseek` → `DEEPSEEK_TO_CLAUDE.md` | + reviewer of every claude-sprint SHA Codex pushes (OK / BLOCK file:line) |

Integrating DeepSeek patches: `git apply --3way`, strip BOMs, keep LF, `Pak/**/PowerStruggle*.lua` stay Latin-1, every
behaviour change behind a default-OFF `wb_*` flag, new tests into `.github/workflows/warsbots-ci.yml`, never push over
red CI. PS Lua: guard per-client RMIs with `WarsBotsIsBot`, never by channel 0 (DS-045). **A bot must only call server
methods of stock scripts** (e.g. `Door:Open`), never the client use path (`OnUsed` → `self.server:Sv…` = RMI from a
client the bot does not have; C-138). No luac in the cloud: Lua is checked by the first PS server start.

## Sync (D-030)

- Every task is a mailbox item (C-/DS-), never only chat. IDs: `python3 Tools/Mailbox/next_id.py C|DS|D`.
- Wake loop on the owner PC wakes Codex and the newest DeepSeek session on any new item.
- Claude's hourly cloud task (trig_01ABKXcowTTiHUd7wwLt9Lxz) should be OFF while D-031 is active.
- Codex budget (C-137): mailboxes archived to open items, X- reports ≤ 6 lines, one Codex instance at a time.
- Wiki source `Docs/Wiki` on triage; Codex publishes it to the GitHub wiki.

## State (27.10 21:20)

claude-sprint head **ba4d5f4e** (C-139 PS buy alive at spawn) on 96f7f624 (DS-052 doors/vehicles/events, default OFF)
on 815bcb7e (C-136 team spawn + owner buy rule) on 1a7ec84e on 493f2cab (DS-050 fundament) on 4f3a1d72.
- PS: spawns 12/12 own base PASS (X-186); revive waves work (X-166); buys 0 → C-139 running; doors FAIL (client RMI,
  X-190) → DS-054 fix ready; vehicles untested; Shore has no navmesh.
- IA: 15×660 on d6a9a521 287/32/0 PASS; snipers 0/3 parked (X-188).
- Open: Codex C-139, C-126 rest, C-129 soak; DeepSeek DS-051; owner D-032.

## Next (in order)

1. C-139 verdict. 2. Integrate DS-054 (`ds054_doors.patch`, then `ds053_probe_setup.patch`), CI, review, Shore 6x300
`wb_ps_doors 1`. 3. C-126 rest. 4. Shore navmesh bake → DS-051. 5. PS plan stage 1–2 (`Docs/Research/PS_PLAN.md`).
6. If D-032 yes: flip only `wb_bot_first_primary` default 1.

Docs: Bible (Claude Doc) https://claude.ai/artifact/JucfPiM9E5cmV7qoM2du8U; PS plan `Docs/Research/PS_PLAN.md`;
upstream plan `Docs/Research/UPSTREAM_READINESS.md`; architecture review `Docs/Research/Reviews/CLAUDE-ARCH-2026-10-27.md`.
