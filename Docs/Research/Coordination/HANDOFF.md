# Handoff for the next Claude coordinator session (2026-10-24 04:50 MSK)

Repo `Madd3st/crymp-client`. Read this file, then `PROJECT_COORDINATION.md` next to it. Do not
re-derive the history; everything below is verified unless marked otherwise.

## Roles

- **Owner**: decides, plays, runs live sessions, carries messages between agents.
- **Codex** (`warsbots/feature/weapon-goals`): owns all game code (C++, GameRules, network,
  builds, servers, QA runs). Claude never edits these files, never merges, never stops servers.
- **Claude (you)**: coordinator + offline research. Own branches only:
  `warsbots/research/steelmill-triage` (tools, findings, this file),
  `warsbots/research/fps-ai-planning` (FPS AI study, decision-layer prototype).
- One Claude session at a time. A parallel session produced `warsbots/research/steelmill-lab`
  (`ad34d283`): its BotNav export contract is the official one; its tools overlap Triage.

## Where things are

| Topic | File |
| --- | --- |
| Status, SHAs, risks, next steps | `Docs/Research/Coordination/PROJECT_COORDINATION.md` |
| Steelmill findings F1–F5 (spawn pin, traps, routes, memory, dataset) | `Docs/Research/SteelmillTriage/FINDINGS-2026-10-24.md` |
| Movement pace, owner vs bots, `MOVE TRACE` spec | `Docs/Research/SteelmillTriage/MOVEMENT_BASELINE.md` |
| Log-field addendum for Codex | `Docs/Research/SteelmillTriage/NAV_EXPORT_CONTRACT.md` |
| Graph export contract (official) | `Docs/Research/SteelmillLab/BOTNAV_EXPORT_CONTRACT.md` on `steelmill-lab` |
| Offline tools | `Tools/SteelmillTriage/steelmill_triage.py` (`mem`, `analyze`, `movement`, `graph`) |
| FPS AI study, gaps, planner design, A/B contract, ML options | `Docs/Research/FPSAILab/*` on `fps-ai-planning` |
| CryMP native audit | `Docs/Research/CryMP/*` on `crymp-native-audit` (`47ccfacf`) |
| Codex's own reports | `Docs/Experiments/*`, `Docs/STATUS.md`, `Docs/AGENT_HANDOFF.md` on weapon-goals |

Owner's logs and map memory: `C:\Users\Magic8\OneDrive\Документы\My Games\Crysis\` (`*.bots.log`,
`WarsBots\Multiplayer_IA_Steelmill.mem`). Copy read-only; a live session may be writing.

## Agreed order of work for Codex (each step separate, gated, soaked ≥10 min before default ON)

1. Spawn 195 + traps: diagnose `grounded=0` after revive at (1247.5, 1319.75, 136.45); displacement
   watchdog with escalation (repath → side step → jump back → last-resort respawn, owner to approve).
2. Movement pace: sprint far more often (energy gates in `TravelGait`/chase), keep the 2.5 m/s
   non-sprint cap until the human baseline exists. Target: >4 m/s share while moving 35–55%.
3. Logging patch: `MOVE TRACE` (humans + bots, 4 Hz, cvar) and NAV fallback `reason=`/`actor=`.
4. Graph export per the Lab contract.
5. Upgrade-branch commitment (`CHASING↔MOVING_TO_WEAPON` flips) with the A/B in
   `TACTICAL_PLANNER_DESIGN.md`; `PickHotspot` `i += 2` fix.
6. Later: decision layer (`wb_decision`), cover candidates, verified jump links, then PS.

## Claude's loop

Codex pushes → owner plays or Codex runs QA → you copy the new logs → run Triage → write a short
verdict (improved / no change / regression, with numbers and evidence type: src / server / client)
→ draft the next message for Codex. Keep replies short; put details in files.

## Open questions for the owner

- Can a human spawned at (1247.5, 1319.75) walk? (geometry vs bot code)
- Is a native self-kill acceptable as the last trap recovery in IA?
- After `MOVE TRACE` exists: one 10-minute owner session on Steelmill with 6 bots for the baseline.

## Pending Claude tasks

- Done 2026-10-24: Triage `navgraph.py` reads `warsbots/botnav-graph/1` (validator, fingerprint,
  memory link, BotNav-exact offline A*). Waits for a real `wb_nav_export` file.

- After step 2 lands: before/after pace report with `steelmill_triage.py movement`.
- After the owner's trace session: human baseline and concrete threshold targets.
