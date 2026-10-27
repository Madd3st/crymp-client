# Review 70aae712 (triage baker, parent ac70b894) — BLOCK

Read-only review of the exact commit; no build, no bake, no game. The commit is DS-062 + DS-067a/b with Codex's
boundary fix and edge-intersection clipping; the eight-case selftest and the cost oracle are as reported.

## Accepted

* Boundary-only contact: `TriOverlapsRect` now uses strict comparisons at `recast_bake.cpp:347` (AABB) and `:358`
  (edge axes), so the two-shores shore edge at x=10 no longer blocks the x=10..12 water cell. That is the correct
  narrow fix for the C145 RED; a positive-area overlap still passes (tiny island 0.16 m^2).
* Exact height: `MaxTriYInRect` clips against all four cell planes (`:363-385`) and includes edge intersections; an
  empty clip returns `1e30` (`:373/:385`), i.e. fail-closed, so a crossing sliver is blocked rather than sampled away.
* The mask still rejects above-water solids, cliffs/quays (`TriSlopeOk`) and slopes rising more than `climb` inside
  the cell; water stays out from under roofs/decks.
* JSON path escaping (`objJson`, `:861-863`) fixes the Windows backslash failure; `input.triangles` stays the
  pre-water count; `blocked_cells`/`cell` are reported.
* The path oracle applies the same `kWaterArea`/`waterCost` pair as `BotNavMeshCore.cpp:60` (default 4), so cost 4 vs
  1 is measured with Detour, not printed text.
* No-water path: the mask is fully guarded by `c.water`; the `pm->flags` predicate change is the DS-062 one and is
  equivalent for the only non-null area (63) in a dry bake. Codex measured a binary-identical synthetic dry bake.

## BLOCK 1 — a cell may be water outside known seabed (`recast_bake.cpp:438,443,446`)

`below` is set when *any* submerged (or shallow-shore) triangle overlaps the cell by a positive area (`:438/:443`),
and the whole cell quad is then emitted (`:446` onward). There is no coverage test for the rest of the cell. A cell
straddling the seabed boundary (or a hole in the seabed, a pier shadow, the map edge) therefore gets up to 4 m^2 of
walkable water over unknown void. That contradicts the fail-closed rule in `WATER_RECAST.md:47` ("no seabed / void
gap -> 0 water cells") and the owner's "never mark unknown navigable". The current `void.obj` passes only because it
has no seabed at all.

Narrow fix: after the loop require the cell to be *covered* by known below/shore geometry, e.g. all four corners and
the centre inside at least one submerged or accepted-shore clipped triangle (or a union-area test >= cell area - eps);
otherwise skip the cell and count it as `unknown_cells`. Add `partial-void.obj` (seabed covering half a cell, void on
the other half) expecting no water quad on the void side; keep the eight existing cases.

## BLOCK 2 — a partial path is reported as found (`recast_bake.cpp:807-817`)

`pathFound = true` whenever `findPath` succeeds with `pathPolys > 0` (`:807-809`). Detour explicitly allows a partial
result when the buffer fills (`DetourNavMeshQuery.h:305`: "filled to capacity with a partial path", status detail
`DT_PARTIAL_RESULT`), and `dtStatusSucceed` accepts detail bits. The tool then counts `water_polys` and `length_m`
over the truncated path (`:810-818`) and prints `"found": true`. The runtime already rejects this case
(`BotNavMeshCore.cpp:328`: `polys[npolys-1] != b` -> `no_directed_route`), so the tool and the runtime disagree. On
Shore's long routes a 256-poly path is realistic, so the C/D acceptance numbers could be taken from a partial route.

Narrow fix: require `pathPolys > 0 && pathPolys < 256 && polys[pathPolys-1] == b`; otherwise print
`"found": false, "partial": true` and do not report a water-poly/length result as a route. Mirror the runtime reason
strings (`other_component`, `no_directed_route`).

## Non-blocking notes

* `--water-cost nan|inf` / `--path-*` with non-finite values prints `nan/inf`, which is invalid JSON. Validate
  `std::isfinite` on inputs and outputs; the same applies to `length_m`.
* A fully degenerate (collinear) triangle can pass strict SAT and clip to a line; above-water stays blocked,
  straddling is benign because other geometry decides. A zero-area clip counter would make this observable.
* `MaxTriYInRect` allocates `std::vector` per triangle/cell; the 38.953 s candidate bake is acceptable, but peak RSS
  was not reported. Record it for the production bake (7.76 M triangles, two grids).
* No-water Steelmill byte identity is not verified here (source input unavailable); synthetic identical is not the
  same claim. Keep it out of acceptance text until the real input is available.
* Shoreline clearance: the `climb` guard prevents water under >0.5 m of local terrain (Recast then drops a span with
  <1.8 m ceiling), which is the intended tunnel guard; shores steeper than `atan(climb/cell)` still get a one-cell
  gap. That is a documented limitation, not a block.

Verdict: `review 70aae712: BLOCK` at `recast_bake.cpp:438-446` (partial cell coverage -> water over unknown void) and
`recast_bake.cpp:807-817` (partial path reported as found). Everything else in the commit is acceptable for the next
candidate; re-review after the two narrow fixes.
