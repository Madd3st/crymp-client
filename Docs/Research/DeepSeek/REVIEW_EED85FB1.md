# Review eed85fb1 (revised triage baker) — OK

Read-only: no build, no bake, no game. Reviewed the tool diff against `70aae712` and the QA artefacts
(`review-regressions.json`, `final-selftest.log`, `70/final-partial-void.dump`).

## Both blocks are fixed

**Unknown void (was BLOCK 1).** `WaterRectCovered` (`recast_bake.cpp:428-448`) starts from the cell rectangle and
subtracts each accepted triangle by half-plane clipping; the remainder is "uncovered". Any uncovered fragment above
`1e-6 m2` keeps the cell out (`:491`), so a cell straddling the seabed edge, a pier shadow or the crop boundary is
`unknown_cells`, not water. The construction is correct: for a convex triangle the complement is the union of the three
outer half-planes, the local double coordinates avoid float cancellation, and subtracting the same triangle twice is a
no-op (overlaps/duplicates cannot fake coverage). Degenerate triangles are skipped (`cross < 1e-12`), and the 128-fragment
cap fails closed (`:446`). `partial-void.obj` is the right RED case (a 0.4 m hole with all four corners and the centre
on known seabed), and the old/new pair is 1 -> 0 quads over the hole.

**Partial route (was BLOCK 2).** `pathPartial` now requires a clean status and the goal poly:
`DT_PARTIAL_RESULT|DT_BUFFER_TOO_SMALL|DT_OUT_OF_NODES`, `pathPolys==0` or `polys[last]!=b` (`:857`), and a truncated
`findStraightPath` also fails closed with zero length/water (`:869`). That matches the runtime's
`polys[n-1] != goal` reason and is strictly safer than `pathPolys>0`. The 256-poly corridor regression
(409.2 m "found" -> `found=false, partial=true, length 0`) proves the buffer detail is really detected.

**Inputs/JSON.** Non-finite `--water`/`--water-cost`/`--water-cell` and path coordinates are rejected before parsing
(`:666-669`); `unknown_cells` and `path.partial` are reported. No-water mesh code is untouched (all new logic is inside
`AddWaterSurface` and the `havePath*` block).

## Non-blocking notes (fix before using `found` as the Shore connectivity gate)

1. **The straight-path buffer is 64 corners** (`:867-868`). `findStraightPath` sets `DT_BUFFER_TOO_SMALL` once it fills
   64, so any complete corridor with ~64 turns is reported `partial` even though the poly route is full. Size the
   buffer to `pathPolys` (max 256) or split `path_partial` (corridor) from `straight_partial` (measurement), and keep
   `found=true` for a complete corridor. Otherwise a valid long Shore route will read as "no route".
2. **Test 9 can pass vacuously.** It only asserts that no quad hits the hole; a regression that emits no water at all
   would also pass. Add `water.cells > 0` and `unknown_cells >= 1`.
3. **Test 10 conflates truncation with no path.** Add `polys -eq 256` (or a control run with a larger budget) so a
   broken query cannot satisfy `found=false, partial=true`.
4. **`unknown_cells` mixes two meanings**: "no geometry in the cell" and "partial coverage". On a crop the outside-of-
   island cells will inflate it. Two counters (`no_geometry_cells`, `partial_coverage_cells`) make the receipt readable.
5. **Production cost is still unmeasured for this commit.** 38.953 s / 9.76 MB belong to `70aae712`; the coverage pass
   adds per-cell clipping on 189k water cells. Record time and peak RSS for `eed85fb1` on the same crop.
6. `area_m2`/`main_area_m2` remain 2x (pre-existing `PolyArea`), and non-finite `--cs/--ch/--radius/--tile` are still
   accepted (could print `nan/inf` bounds). Both are outside this diff; do not quote area as m2 until fixed.

Verdict: `review eed85fb1: OK` — both DS-068 blockers are closed with tested, fail-closed logic; the accepted Shore
bake can proceed. Note 1 must be settled before the tool's `found` field is used to prove water-to-land connectivity.
