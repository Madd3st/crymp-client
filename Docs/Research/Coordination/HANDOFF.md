# Handoff for the next Claude coordinator session (updated 2026-10-26 11:35 MSK)

Repo `Madd3st/crymp-client` (public). Read this file, then `Docs/Research/STATUS.md`, `MAILBOX/DECISIONS.md`
(D-001…D-021), `MAILBOX/PROTOCOL.md` (v4 states + task cycle v5), and run `python3 Tools/Mailbox/board.py`.
Do not re-derive history; everything older is in git and in the journals.

## Owner and style

- Owner = Magic8 (madd3st). Russian, humour, mild swearing OK, short chat, details in files.
- Status format: who does what now / done / next / priorities / roadmap stage.
- Owner decides (DECISIONS.md) and plays "feel" playtests; never regression hunting (D-020).
- Owner's PC is the only game machine. Computer use is possible but hides his windows — avoid when he is working.

## Roles (D-013/D-014, still valid)

| Agent | Does | Writes to |
| --- | --- | --- |
| Claude (you) | all game code (`warsbots/feature/claude-sprint`), coordination, verdicts, docs, tools | triage branch (mailbox/docs), claude-sprint |
| Codex (ChatGPT app on owner PC) | MSVC builds, runs/batches/probes, presets, merges to weapon-goals, wiki publish | `warsbots/feature/weapon-goals` → `Docs/Coordination/TO_CLAUDE.md` (X-xxx) |
| DeepSeek (OpenCode app, V4.1 Flash) | read-only review of every diff before a batch, specs, log analysis | `warsbots/research/deepseek` → `Docs/Coordination/DEEPSEEK_TO_CLAUDE.md` |

Mailbox: triage `Docs/Research/Coordination/MAILBOX/TO_CODEX.md` (C-xxx), `TO_DEEPSEEK.md` (DS-xxx), `DECISIONS.md`.
IDs from `python3 Tools/Mailbox/next_id.py C|DS|D`; `--check` before every push (duplicates happened 26.10).
Run items carry a ```recipe block (`Tools/Mailbox/recipe.py`). Always `git fetch origin '+refs/heads/warsbots/*:refs/remotes/origin/warsbots/*'` —
a plain fetch left stale refs and hid Codex's work for an hour on 26.10.

## Rules that bite

- **One 15-bot run per day** (D-020) unless the owner allows more; probes/short runs do not count.
- Oracle before code, default-OFF flag, offline green (`Tools/OfflineCheck/run_units.sh`, `compile_check.sh`), DeepSeek
  review, one build per batch. CI: `.github/workflows/warsbots-ci.yml` builds every push to `warsbots/feature/**`
  (units + client/server); a newer push cancels the older run.
- Keep CRLF/LF of each file (python `open(..., newline='')`).
- A run recipe must include the owner preset feature set (navmesh etc. are still default 0 in code) — the
  "defaults-only" C-087 run was invalid.
- Agents go idle: Codex heartbeat automation (30 min, `~/.codex/automations/warsbots-claude/automation.toml`, short
  static prompt — do not let Codex put state in it) missed C-097 for 8 h on 26.10 night. C-101 asks Codex to fix the
  Windows wake task. If an agent is silent, ask the owner to paste a one-line nudge.

## State at handoff (26.10 11:35)

Code (claude-sprint head **b1142b93**):
- D-016 corners v2.1 + jump sanity **default ON** (5b90bfb6), confirmed C-088 (278 kills / 18 stalls / 0 traps).
- `wb_bot_difficulty` ladder (BotDifficulty.h): reaction/turn/aim/head/fire gate/range/senses (1e0f4072, 7179ec36) —
  **C-093 PASS**: kills 276, stalls 19, traps 0, Elite 70 vs Beginner 24, 13 hits > 40 m; Elite headshots only 7.8 %.
  Part 2–3 (c6f36c28, 5f1a6c88, b1142b93): zoom/burst by skill, fight strafe/jump by skill, tactical reload,
  Elite cloak approach — **not run yet** (C-103 queued, DS-024 review queued).
- `wb_bot_finish` (target switches 641 → 398), `wb_bot_spectator_aim` (camera = tracer), `wb_bot_path_progress`
  (+ 1dc87ba3 fix), `wb_bot_threat_scan` (glance fix c6f36c28, logs `#n GLANCE`) — C-098 FAIL: no bot ever holds
  DSG1/Gauss (12 unreachable), stalls 39 vs 19, glances 0 (bug fixed since). DS-023 analysing why.
- QoL console: `wb_mark` (F8 for the owner), `wb_fill`, `wb_skill_all`, `wb_kick_bots` (40bd9f60). Probe hook
  `wb_probe_item` + `Tools/Probes/*.json` (ceb6e5b2); Codex builds the runner (C-100).
- Owner client MOVE TRACE at `wb_move_trace_hz` + stance (bb172f34).

Waiting on agents:
- Codex: C-102 owner playtest preset on 40bd9f60 (difficulty+finish+spectator_aim) → C-101 (wake task + one-click
  launcher with F8 mark + auto report) → C-100 probe runner → C-103 batch b1142b93 (owner must allow today or tomorrow).
- DeepSeek: DS-022 (what a mark card shows), DS-023 (snipers + stalls in C-098), DS-024 (review of b1142b93).

Power Struggle (parallel track, D-018/D-019): specs done by DeepSeek (`PS_T0_SPEC.md`, `T0_GAMEMODE.md`,
`VEHICLES.md`, `PS_MIN.md` on deepseek branch). Shore/Plantation navmesh baked from a943e070 crops (Claude's
workspace only: `/home/claude/ps/shore2`, `plantation2`; Shore has a cut-off base around (2100,1900): 24 spawns +
4 items outside the main mesh). Shore 3D v4 artifact: https://claude.ai/artifact/QcDCZtqHXh3NFPY3xBRCKY.
Teams 3–4 bots, scaling; vehicles enter/exit mandatory in PS v1.

## Next for Claude (in order)

1. Verdict C-103 when it lands; if green, preset for the next playtest with ladder part 2–3.
2. Snipers: act on DS-023 (why DSG1/Gauss are dropped) — likely Strength-link execution on the tower/stair routes.
3. Grenades (bots never throw; target behind cover → frag at last known position, self-safety check). D-023: teammates may get hurt by accident, but no throw when a teammate is near the blast point. D-024: if Elite feels unfair, add a first-shot pause, keep accuracy. D-022: C-103 runs today.
4. "Flags die" cleanup from `DeepSeek/FLAGS.md` (41 flags): promote accepted ones, delete dead paths.
5. Owner playtest marks (F8) → playtest report cards (DS-022 spec) in `Tools/PlaytestReport`.
6. T-1 (one writer per control channel, DeepSeek `T1_WRITERS.md`), then T-3 network (incl. spectator HUD line), then
   T-0 IBotGameMode, then PS navmesh into the repo (big files as release assets, D-021 #8).
7. Wave 2 QoL: browser demo viewer on the 3D map; one-command map pipeline.

Tools for the owner: Telegram notifier `Tools/Notify/` (owner has not installed it yet). Wiki is human-readable now
(`Docs/Wiki/Status-and-Roadmap.md`, published by Codex).
