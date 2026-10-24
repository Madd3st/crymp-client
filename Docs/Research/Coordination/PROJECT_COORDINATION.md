# WarsBots coordination (maintained by Claude)

Updated 2026-10-24 04:25 MSK. Source of truth for code is GitHub; this file records what
was verified and by which kind of evidence: **src** = source reading, **unit** = offline
tests, **server** = server logs, **client** = visual check in a real client.

## Branches and verified SHAs

| Branch | SHA | Owner | State |
| --- | --- | --- | --- |
| `warsbots/feature/weapon-goals` | `0f338b12` | Codex | active; spectator + P1 regression report pushed 04:13 |
| frozen P0 build | `8f0abd5f` | Codex | 4 headless 120 s runs (6/15 bots): no crash, no duplicate shots, 1 round per shot |
| spectator build | `a6ee9e9e` | Codex | BUILD PASS, C++ + 60 s headless QA PASS; live client scenario NOT run |
| `warsbots/main` | `caf1a815` | integrator | far behind weapon-goals |
| `warsbots/research/crymp-native-audit` | `47ccfacf` | Claude | done; docs + Tools/CryMPResearch |
| `warsbots/research/steelmill-lab` | `332a3c8b` | Claude (session 01WT…) | Steelmill AI Lab tools (viewer, mem/log/graph, ML validator); started in parallel, no findings doc yet |
| `warsbots/research/steelmill-triage` | this branch | Claude (coordinator) | findings from real logs, export contract, triage detectors, this file |
| `warsbots/research/fps-ai-planning` | planned | Claude | FPS AI research + engine-free planner prototype |

Codex report: `Docs/Experiments/regression-spectator-2026-10-24.md`. Spectator reuses the
native observer flow (`GetNextSpectatorTarget` → `GetMatchParticipants` on local IA, native F1,
renderer-local FP view from replicated aim), as recommended in the CryMP audit. Independent
Triage pass over the same four regression logs agrees: no new movement regression versus older
builds; the spawn-pin defect (Triage F1) is present in `8f0abd5f` too and predates it.

## Current tasks

- **Codex**: regression + Spectator done server-side; waiting for the owner's live client run
  (manual scenario in Codex's report).
- **Claude**: two sessions ran Steelmill work in parallel by accident (see branches). Next for the
  coordinator: FPS AI research → planner prototype. Proposed: Lab session absorbs the triage
  detectors (pinned/trap/motion-join) into `Tools/SteelmillLab`, then `SteelmillTriage` is retired.
  No game files, no servers, no merges.

## Component state

| Component | State | Evidence |
| --- | --- | --- |
| Fire control / NetShoot replay fix | fixed for owned bots, holds at 15 bots | unit + server (Codex); client not checked |
| Spectator (`wb_watch`, F1) | implemented natively | unit + headless server; client not checked |
| Aim / head zone | plausible in duels | server, small sample |
| BotNav build | 7633 nodes / 41618 edges, stable | server |
| BotNav routing | 15–18 failed queries per bot-minute, 42–62% cross-floor | server (Triage F3) |
| Movement stuck handling | blind to "accepted but not moving" | server + src (Triage F1) |
| Trap recovery | none after 1.5 s detours fail | server + src (Triage F2) |
| Map memory | half ignored by hotspots, no decay | src + real .mem (Triage F4) |
| ML dataset | not bandit-ready | server + src (Triage F5) |
| Power Struggle | design only | docs |

## Known risks

1. Short QA runs (120 s) under-measure the two worst movement defects; long play shows 4–7%
   pinned and up to 23% trapped live time. Regression gates need a ≥10 min soak.
2. Stuck memory only grows; a bad session permanently biases routes on the owner's file.
3. Logs lack actor ids on suit/jump lines and failure reasons on route lines.

## Proposed next steps (small, independently testable)

For **Codex**, after the current regression, in this order. Each is a separate commit,
default ON only after a soak shows no regression:

1. *Progress watchdog* (F1, F2): if a live bot requested movement ≥ 8 s and moved < 0.5 m,
   escalate: repath → reverse/side step → jump out if `JumpSpace` allows → after 45 s, native
   self-kill through GameRules so it respawns (log `TRAP RECOVERY level=`). Uses only own
   position, no enemy knowledge.
2. *Spawn 195 check* (F1): during the live spectator run, follow a bot revived at (1247.50, 1319.75, 136.45) in FP/TP; also
   spawn a human there. If geometry: exclude that spawn for bots or nudge revive position.
   If bot-only: trace `CMovementRequest`/`canMove` there.
3. *Logging patch* per `NAV_EXPORT_CONTRACT.md` (reason on NAV fallback, actor on suit/jump).
4. *`wb_nav_export`* per the contract. Unblocks the graph layer in both Lab and Triage tools and offline replay.
5. *PickHotspot stride bug* (`i += 2`): iterate all nodes; measure frame cost (it is 10 Hz).

Deferred until 1–4 are in: stuck decay / edge-level penalties, roof links, any planner.

## Open questions for the owner

- Can you spawn as a human at (1247.5, 1319.75) and walk? That separates geometry from bot code.
- Is a native self-kill acceptable as the last-resort trap recovery in IA (it changes score)?
