# Review of Codex A–D (weapon-goals `f119d2f..32b037f`), 2026-10-24 13:00

Inputs (owner PC, read-only): `WarsBots-Tactics-body-soak-12eaec0f-candidate-r1-n15.bots.log` (A only, "A"),
`WarsBots-Tactics-scc-soak-ca198a42-candidate-r1-n15.bots.log` (A+B+C+D flags on, "B"), same graph export
(fingerprint `cfcf02f0…`, identical to d4b0854c). One unpaired 15-bot 660 s run each, random roster/RNG:
**descriptive, not causal.** Code read on `FETCH_HEAD` (src).

## Verdict per step

| Step | Verdict | Evidence |
| --- | --- | --- |
| A body samples | works; confirms a revive-specific contact defect at 195: groundHeight = −1e10, zero vel/velRequested, bStuck 1 in 3/4 (normal spawns 209 samples bStuck 0); creation at 195 is fine | server (Codex) |
| B reachable goals | **regression, keep OFF.** Lives with ≥1 pickup 81/200 → 32/192; for lives starting on main-component spawns 58/139 (42 %) → 14/125 (11 %); sink spawns unchanged (17/37 → 12/45). Weapon timeouts barely move (296 → 276), so the skip removes good goals and keeps bad ones | server + graph |
| B mechanism | `BotReachability` is correct Kosaraju + condensation bitsets (src). Offline, with fresh bot position (≤ 0.6 s) and weapon position ≈ bot position at an A pickup (±1–2 m): 65 of 142 evaluable skips target a weapon the graph says IS reachable. Suspect goal projection: `NodeAt(goal)` = `Nearest` incl. the 0.9 m physics ray, run on the weapon's own position (often on a crate/table) → picks another layer / island node | src + graph (approximate positions) |
| C sink escape | no measurable effect; self-kills 2 → 5 but every self-kill and every level-4 episode in B is at spawn 195 (5 revives there vs 3 in A) | server |
| D `wb_nav_explain` | works. 2871↔2874 (beside spawn 195) both reject on clearance: static hits at (1245.90, 1318.50, 136.61) and (1248.13, 1318.50, 136.58) — a static object ≥ 2.2 m long at knee height 1.25 m south of spawn 195 (railing/curb? owner screenshot shows a rail along that sidewalk). 2733→2873 rejects rise 2.17 m (legitimate) | server |
| `wb_revive_at` / `wb_teleport` | works headless: forced revive at 195 reproduces F1 at +5 s (z 136.512, invalid groundHeight) | server (Codex) |
| summary pipeline | vendored Triage = `31e9394` byte-identical (steelmill_triage.py, summary.py, navgraph.py, tests) | src |

`summary` (warsbots-run-summary/1): A — 195 frozen 3/3, watchdog 12 episodes / 2 self-kills, pinned 1.3 %, fallbacks 1904;
B — 195 frozen 5/5, 14 / 5, pinned 2.3 %, fallbacks 1510.

## Requests

1. B stays OFF. Change the rule: a goal is unreachable only if **no** node near it (same candidate set as
   `Nearest`: ±2 columns, |dz| ≤ 2.5) is reachable from the bot's node; skip the physics ray for this test;
   fail open when the bot's own node is in a sink/isolated SCC (the sink is often a graph artefact).
   Log `goal=(x,y,z) toNode= toZ= candidates= reachableCandidates=` on `GOAL SKIP`. Re-measure with the
   per-life pickup rate split by spawn component (Claude computes it from the log).
2. Spawn 195: add a test-only cvar for the revive z offset (0 / 0.2 / native `GetSpawnLocation` zoffset) and
   run `wb_revive_at 195` ×10 per value + `wb_revive_at 195 me` (human). Also `GetEntitiesInBox` 2 m around
   (1247.5, 1319.75) with names/classes/proxies. Native Lua also calls `spawn:Spawned(player)` and
   `player:UpdateAreas()`; the bot path does neither.
3. For any comparison: paired runs, same roster/memory, ≥2 repeats per arm.
