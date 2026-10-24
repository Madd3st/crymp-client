# Handoff for the next Claude coordinator session (2026-10-24 19:55 MSK)

Repo `Madd3st/crymp-client`. Read this, then `../STATUS.md` (current truth) and the mailbox. Do not re-derive
history. The `warsbots-coordinator` skill (if the owner saved it) carries the same loop.

## Roles and rules

- **Owner**: decides and plays. Answers questions in chat; asks Claude directly ("статус", "Codex запушил",
  "я поиграл"). Wants short, funny, Russian replies; details go to files.
- **Codex** (`warsbots/feature/weapon-goals`): all game code, builds, servers, QA. Claude never edits its
  files, never merges, never starts/stops servers or game processes.
- **Claude**: coordinator + offline research, writes only in `warsbots/research/steelmill-triage` (and
  `fps-ai-planning`). `steelmill-lab` is retired (merged here).
- Owner decisions (MAILBOX/DECISIONS.md): D-001 keep 45 s self-kill; D-002 agents may default-ON
  logging/diagnostics only, behaviour flags need the owner; D-003 live sessions yes; D-004 logs move out of
  OneDrive — done: `C:\Users\Magic8\AppData\Local\WarsBots\{Logs,Memory}` (X-014).

## Loop

1. `git fetch` weapon-goals: new commits, `Docs/Coordination/TO_CLAUDE.md` (X- messages), `Docs/Experiments/*`.
2. Stage new logs read-only from the owner PC: since 2026-10-24 14:32 (D-004, X-014) Codex writes to
   `C:\Users\Magic8\AppData\Local\WarsBots\{Logs,Memory}` **as seen by Codex**; Codex is a Store app, so on
   disk this is `C:\Users\Magic8\AppData\Local\Packages\OpenAI.Codex_2p2nqsd0c76g0\LocalCache\Local\WarsBots\`
   (connected as its own folder so staging depth is ≤ 7);
   older logs stay in `C:\Users\Magic8\OneDrive\Документы\My Games\Crysis\`. Run
   `steelmill_triage.py summary <log> --graph <nav.json> --mem <mem>` (+ `analyze`/`graph --replay`).
3. Verdict better / same / broke with numbers and evidence type; compare only same-scenario paired runs.
4. Write C- items in `MAILBOX/TO_CODEX.md`, update `../STATUS.md` and SHAs in `PROJECT_COORDINATION.md`, push.
5. Chat: 3–6 lines. Questions for the owner go straight into chat (AskUserQuestion), not into files only.

## Assets

- Steelmill Nav Atlas (private artifact, owner map with minimap underlay and owner marks):
  https://claude.ai/artifact/KbCBM3YfqFjj2hY7Qa8Y34 — owner marks in ArtifactData collection `marks`.
- Level facts: `steelmill_triage.py level --pak level.pak --level-xml Steelmill.xml --minimap Steelmill.dds`
  from `E:\Steam\steamapps\common\Crysis\Game\Levels\Multiplayer\IA\Steelmill\` (copies; outputs contain game
  assets, never commit them).
- Owner video: OCR `r_displayInfo` CamPos at 1 fps; speech-to-text is not available in this workspace.
- Game folder `E:\Steam\steamapps\common\Crysis\Bin64\_dlss_backup\` holds dgVoodoo/DLSS leftovers moved out
  on 2026-10-24 (client now renders plain DX9).

## Pending for Claude (19:55; queue and risks in ../STATUS.md)

- C-021 sprint verdict (`pace` on the pair). C-028 cropped geometry → `steelmill_triage.py navmesh` on 4 maps vs
  each map's BotNav graph (walk / +jump / +strength), verdict for D-006 switch.
- First demo (C-025) → `steelmill_triage.py demo`; refactor wave 1 (C-027) before/after check.
- OpenCode: OC-018 (demo spec review), OC-019 (refactor move list); poke it if its mail loop stops.
- Rules added today: > 2 h → workaround; one task/push/stop for Codex; no builds during owner play; every flag
  states its exit.

## Older pending items

- C-006 results → name the false-cut tests → graph-repair spec for Codex.
- MOVE TRACE lands → owner repeats the 5-minute route → jump heights, sprint timing, pace targets.
- Step 2 lands → before/after pace report (`movement`, `summary`).
- After step 1 closes → bot-code refactor plan from `SteelmillTriage/REFACTOR_AUDIT.md`.
