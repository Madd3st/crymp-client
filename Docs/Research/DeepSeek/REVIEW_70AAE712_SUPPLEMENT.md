# Supplement to `REVIEW_70AAE712.md` (independent second pass) — BLOCK stands

Date 2026-10-28. Read-only: no build, no bake, no game. I re-derived the two blocks from the pinned Detour v1.6.0
source and the C145 artefacts, and add four items the first pass did not state.

## Evidence for BLOCK 2 (partial path) — exact mechanism

* `dtNavMeshQuery::findPath` sets `status |= DT_PARTIAL_RESULT` when `lastBestNode->id != endRef`
  (`Detour/Source/DetourNavMeshQuery.cpp:1156-1159`), i.e. when the goal was **not reached**.
* `getPathToNode` truncates to `maxPath` and returns `DT_SUCCESS | DT_BUFFER_TOO_SMALL`
  (`DetourNavMeshQuery.cpp:1199-1202`).
* `dtStatusSucceed` only tests the `DT_SUCCESS` bit (`Detour/Include/DetourStatus.h:37-40`); both detail bits pass.
  So `recast_bake.cpp:807` sets `pathFound=true` for an unreachable goal or a 256-poly truncation, then counts
  `water_polys`/`length_m` over the stub (`:810-818`). The runtime already rejects this (`BotNavMeshCore.cpp:328`,
  `polys[n-1] != goal`), so tool and runtime disagree.
* Correct gate: `!(status & (DT_PARTIAL_RESULT | DT_BUFFER_TOO_SMALL)) && pathPolys > 0 && polys[pathPolys-1] == b`,
  not `pathPolys < 256` (a legitimate 256-poly path would be mislabelled).

## Extra items

1. **Stale comment contradicts the fix.** `recast_bake.cpp:335-337` says "SAT ... touch counts as overlap
   (conservative)"; the accepted behaviour is the opposite — `:347` rejects a shared border (`hi[0] <= x0`), which is
   exactly why two-shores reconnects. Update the text, or the next editor will "restore" the disconnect.
   (The SAT axes at `:348-359` still treat touch as overlap; the AABB at `:347` runs first and wins, so the effective
   rule is "separated", not "overlap". State that explicitly.)
2. **`area_m2` is 2x the real area, pre-existing.** `PolyArea` (`:581-588`, introduced by `5df0defb`) sums
   `dtTriArea2D`, which returns twice the triangle area, without halving. Check: dry two-shores reports 342 m2 for two
   ~88 m2 eroded 10x10 quads; water two-shores reports 729 m2 vs ~377 m2 expected. Relative comparisons still work,
   but the Shore receipt's `area_m2 2,383,134` / `main_area_m2 2,113,751` must be halved before quoting them as map
   area or as a percentage of the level.
3. **Steep-shore bound is 14.0 deg, not "one climb step" in general.** The crossing cell is skipped when
   `MaxTriYInRect > waterZ + climb` (`:442`), i.e. when the rise inside `waterCell` exceeds `climb`:
   `atan(0.5/2.0)=14.0 deg` at the production `--water-cell 2`, `26.6 deg` at `--water-cell 1`, `45 deg` at `0.5`.
   `shore-gentle.obj` documents this. Consequence for the Shore candidate: 148,679 blocked cells and no proof that a
   single water poly touches a land poly. "Water connects the shores" is a synthetic-only claim until a real-bake
   connectivity point exists (path from a water cell to a land spawn, `water_polys > 0`, full path).
4. **Production cost is partially measured.** 38.953 s is recorded (`Shore.bake-receipt.json`), peak RSS is not.
   `TriGrid::Build` sizes the grid from the mesh bounds (`:292`) with no cap; one stray far vertex multiplies
   `nx*nz` and the per-cell `std::vector` allocations. Add a bounds sanity cap and report RSS in the receipt.

## Verdict

Unchanged: `review 70aae712: BLOCK` (`:438-446` cell coverage, `:807-817` partial path). Items above are either
fix-before-next-bake (1) or acceptance-text corrections (2-4); none of them changes the two blocking defects.
