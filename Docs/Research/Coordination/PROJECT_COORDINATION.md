# WarsBots coordination (maintained by Claude)

Updated 2026-10-24 04:20 MSK. Source of truth for code is GitHub; this file records what
was verified and by which kind of evidence: **src** = source reading, **unit** = offline
tests, **server** = server logs, **client** = visual check in a real client.

## Branches and verified SHAs

| Branch | SHA | Owner | State |
| --- | --- | --- | --- |
| `warsbots/feature/weapon-goals` | `53023d52` | Codex | active; no push since 53023d52 at 04:00 |
| last confirmed game build | `8f0abd5f` | Codex | BUILD PASS + server duels; client replication unverified |
| `warsbots/main` | `caf1a815` | integrator | far behind weapon-goals |
| `warsbots/research/crymp-native-audit` | `47ccfacf` | Claude | done; docs + Tools/CryMPResearch |
| `warsbots/research/steelmill-lab` | this branch | Claude | Steelmill AI Lab + findings |
| `warsbots/research/fps-ai-planning` | planned | Claude | FPS AI research + engine-free planner prototype |

Codex P1 regression logs for `8f0abd5f` (6 and 15 bots, 2 runs each, ~120 s) exist on the
owner's machine but are not committed yet. Spectator Mode: nothing published yet.

## Current tasks

- **Codex**: regression of `8f0abd5f` (shots, ammo, duplicates, perf) → Spectator Mode.
- **Claude**: Steelmill AI Lab (done in this branch) → FPS AI research → planner prototype.
  No game files, no servers, no merges.

## Component state

| Component | State | Evidence |
| --- | --- | --- |
| Fire control / NetShoot replay fix | fixed for owned bots | unit + server (Codex) |
| Aim / head zone | plausible in duels | server, small sample |
| BotNav build | 7633 nodes / 41618 edges, stable | server |
| BotNav routing | 15–18 failed queries per bot-minute, 42–62% cross-floor | server (Lab F3) |
| Movement stuck handling | blind to "accepted but not moving" | server + src (Lab F1) |
| Trap recovery | none after 1.5 s detours fail | server + src (Lab F2) |
| Map memory | half ignored by hotspots, no decay | src + real .mem (Lab F4) |
| ML dataset | not bandit-ready | server + src (Lab F5) |
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
2. *Spawn 195 check* (F1): with Spectator Mode, look at (1247.50, 1319.75, 136.45); also
   spawn a human there. If geometry: exclude that spawn for bots or nudge revive position.
   If bot-only: trace `CMovementRequest`/`canMove` there.
3. *Logging patch* per `NAV_EXPORT_CONTRACT.md` (reason on NAV fallback, actor on suit/jump).
4. *`wb_nav_export`* per the contract. Unblocks the Lab graph layer and offline replay.
5. *PickHotspot stride bug* (`i += 2`): iterate all nodes; measure frame cost (it is 10 Hz).

Deferred until 1–4 are in: stuck decay / edge-level penalties, roof links, any planner.

## Open questions for the owner

- Can you spawn as a human at (1247.5, 1319.75) and walk? That separates geometry from bot code.
- Is a native self-kill acceptable as the last-resort trap recovery in IA (it changes score)?
