# DS-093: review of `b8b444d6` - DS090b resolved

Exact `b8b444d6c858f27a1fdde0d93b25a318cafc6937` (parent `def8a65d`), CI 40709459511 queued. Offline review; no
build/game. Cumulative `b066..b8b`: 11 files, +286/-29 (the known water/roam set only, no config/cvar/Lua/default
file, no new `wb_` flag); `def8..b8b`: 4 files, +42/-2 (DS092 patch + two refinements).

## Verdict: **`DS093 b8b444d6: OK`** - the DS090b defect is resolved for this SHA

### The fix is coherent (samples AND committed goal)

* `goalZ = RoamGoalZ(mesh && waterStart, feet.z, m_blackboard.waterZ)` (`BotBrain.cpp:960`), all four samples
  `p.z = goalZ`, committed positive-fraction `goal.z = goalZ` (`:970-975`), `fraction == 0` still holds at `feet`.
* The adapter refinement is correct and needed: my published patch used `waterStart` alone, which lifted the local
  goal even when `mesh == nullptr` (projection failed/hole) and so changed the legacy no-mesh behaviour; `mesh &&
  waterStart` keeps that path byte-identical (`goal.z == feet.z`), as specified.
* Dry callers: `waterStart == false` -> `goalZ = feet.z`; the leg is horizontal, so nothing changes.
* Gate is the existing qualified fact (`WaterStartBounds`: `waterMovement && waterValid && lift > 0.05`); no actor
  position change, no flag/subsystem, no global `Nearest`/`Project` widening, spawn/hotspot/PS-654/upper-jump
  untouched. Heard/sound goals stay dry and are explicitly a separate unknown (no blind lift).

### Real-core evidence (re-read, not taken on trust)

`build/ds092-offline/roam.cpp` runs the actual `RoamGoalZ`/`RoamFallbackFraction` with `WaterQueryBounds(lift)` over
the 1136 C147 swim rows + E223/mask14: `waterStarts=1095`, **`oldLocalMoves=0`**, **`sampleOnlyCommittedRawPaths=0`**
(the one-liner really is insufficient), `correctedMoves=878`, `correctedCommittedPaths=878`, `fullLegRejected=487`,
`holds=217`, `contradictions=0`, exit 0 requires `halfPaths==0 && bad==0`. The 217 holds are cases where even the
1.5 m fraction is in another component/off-mesh (hull/void edges) - the guard's intended fail-closed answer, not a
lift failure; the 487 full-leg rejects pick a shorter reachable fraction.

### DS090b status

Resolved in `b8b444d6`: the local water leg is now sampled **and** committed at the native plane; the earlier false
"unreachable" (water goal at feet z vs the dry goal box) cannot occur for a valid water start. Behaviour for dry,
no-mesh, fraction-0 hold, land spawn/hotspot and the C148 capture-654 route is unchanged.

## Remaining (not blocking C149)

* Heard-gunshot goals from a swimming shooter still use the dry goal box (noted, no lift - separate unknown).
* The 41 water holes and the PSCapture water-start fail-closed behaviour stand as before.
* C149 still needs green CI 40709459511 + artifact hashes + the harness's fresh-review-head checks (before staging and
  after the actorless preflight); this OK is for the exact SHA only, and the C148 def8 run remains physical evidence,
  not a gate-approved run.
