# Offline navmesh verdict on 4 IA maps (D-006 step 1) — 2026-10-24 21:10

> **Correction 22:10:** the real Recast bake (`RECAST-BAKE-2026-10-24.md`) checks jump arcs against the actual
> triangles. The "+Strength" numbers below are optimistic (links through walls); Armada stays at **6 / 40**, Steelmill
> 32 / 36. Walk-only, Recast is much better than this model (Steelmill items 53, not 9). Use the Recast numbers.
> **v2 22:40** (finer cells): Steelmill 36/36 on foot, Armada 34/40 with jumps — see the Recast verdict.

Input: Codex C-019/C-028 collision export (a7553da / af53f5c), cropped to spawns ∪ items + 120 m; Triage `navmesh`
(Recast-style walk model: 0.2 m cells, agent r 0.4 / h 1.8, climb 0.5, slope 45°, drops ≤ 4 m; ballistic jump links:
normal v0 4.43 m/s, Strength 9.60 m/s, arc clearance), level ceiling removed (`--zmax`: Steelmill has collision planes
at 196–198 m), Armada sea bed removed (`--zmin 108`). "Main part" = the component holding most spawns. Evaluation
model, not the in-game navmesh; real Recast bake is OC-022 (OpenCode, after its limit resets).

| Map | Spawns in main: walk → +jump → +Strength | Items reachable: walk → +jump → +Strength | Main area +Strength |
| --- | --- | --- | --- |
| Steelmill | 30 → 31 → **36 / 36** | 9 → 13 → **103 / 109** | 20.9k of 43.2k m² |
| Quarry | 28 → **32 / 32** → 32 | 83 → 100 → **108 / 108** | 30.8k of 34.1k m² |
| Outpost | 32 → **35 / 35** → 35 | 78 → 92 → **118 / 123** | 9.1k of 26.3k m² |
| Armada | 6 → 9 → **39 / 40** | 24 → 37 → **151 / 156** | 16.6k of 16.8k m² |

Old BotNav graph on Steelmill (same day): 13 / 36 spawns outside the two main parts, 25 % of nodes in sinks, 0 edges
between the outer ring and the factory, `no_route` 90 % of fallbacks.

Owner route (Steelmill, 2 MOVE TRACE sessions): 89 % of samples on the walk model; consecutive pairs reproducible
99.8 % walk-only. The 11 % off-mesh are mostly roofs/catwalk edges eroded by the 0.4 m radius and mid-air samples.

**Verdict: the geometry-based navmesh is far better than the ray graph and works on maps we never tuned.** With
jump links every spawn on Steelmill, Quarry and Outpost is in one part and ~95 % of weapons are reachable. Strength
jumps are essential (Steelmill items 13 → 103), confirming D-006's "vertical play first-class".

Open points before the in-game switch:
1. **Armada**: walking alone connects 6 / 40 spawns — the model misses how players move between decks (owner asked:
   stairs, ladders, lifts?). Strength links connect it, but if there are lifts/ladders they need their own link type.
2. **Steelmill walk-only items 9 / 109**: most weapons sit on roofs/catwalks — check with the real Recast bake that
   stairs/catwalks are not lost to erosion (audit risk 3) before trusting the walk-only number.
3. Strength links are generated generously (one per component pair); the in-game links must come from the filtered
   set (ballistic + owner clusters, Triage `jumps`) and be probed with `wb_link_test` (C-036).

Pictures: `evidence/navmesh/<map>.png` (green: two-way with main, orange: drop-off only, red: no way back, grey:
isolated, black: not walkable; white circles: spawns; blue: owner route on Steelmill). Numbers: `evidence/navmesh/<map>.json`.
