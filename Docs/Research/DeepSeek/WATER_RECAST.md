# DS-060: water in the existing Recast/Detour path (Shore prerequisite)

C144 receipt: `Docs/Coordination/C144_SHORE_PREFLIGHT.json` and
`build/coordinator-c144/`. The release Shore gzip hash is verified (`0cbe74f3...`), the current baker rejects
`--water 99.5` with exit 2, and the synthetic two-shores baseline is 2 components / item unreachable. No production
bake or install is claimed.

## Current gaps (exact)

* `Tools/RecastBake/recast_bake.cpp:46-79`: `LoadObj` keeps only triangles inside `zmin..zmax`, so a water mask cannot
  see the seabed when the caller uses `--zmin 100` (the synthetic case).
* `recast_bake.cpp:206`: every triangle is marked with `rcMarkWalkableTriangles` (`RC_WALKABLE_AREA`); there is no
  water area id. `:230` sets poly flags only for `RC_WALKABLE_AREA`, so a custom area would be excluded.
* `recast_bake.cpp:453-473`: no `--water` CLI; unknown arg returns 2.
* `Code/CryGame/Bots/BotNavMeshCore.cpp:59-60`: `dtQueryFilter` is default cost; water cannot be more expensive than
  land, and no mesh currently carries a water area.

## Minimum safe extension (existing baker + existing query only)

1. **Second geometry view, not a global plane.** Extend `LoadObj` with an optional `allTris` output filled before the
   `zmin/zmax` drop (`:79`). Water mask uses `allTris`; the bake still uses the filtered `tris`.
2. **CLI:** `--water <crysis_z>` (99.5 for Shore), stored in `Cfg::waterZ`; no other parameter changes. Steelmill
   no-water output must stay byte-identical (same cs/tile/radius/height/climb).
3. **Water surface generation:** 2 m grid over the full geometry bounds. For each cell centre:
   * at least one `allTris` triangle with all vertices below `waterZ` must cover it (seabed/terrain);
   * no triangle above `waterZ` may cover it (land, roof, bridge deck);
   * a triangle straddling `waterZ` at that cell rejects the cell (shoreline/unknown);
   * otherwise emit two flat triangles at `y=waterZ` (Recast frame), area id `WATER_AREA=4`.
   This deliberately leaves void cells and under-solid cells without water; it is not a global waterplane and does not
   mark the seabed walkable.
4. **Bake:** pass a per-triangle water flag into `BuildTile`; after `rcMarkWalkableTriangles`, override water triangle
   areas to 4; set `pm->flags[i]=1` for any non-`RC_NULL_AREA` (`:230`), so water polygons are walkable surfaces.
   `polyAreas` already reaches Detour via `dtCreateNavMeshParams` (`:247`).
5. **Runtime:** in `BotNavMeshCore`, after creating `m_filter`, call `setAreaCost(4, 4.0f)`; land stays 1.0. Water is
   navigable but a path prefers land; `SetLinkMask(0)` still includes flag 1, so water polygons remain selectable.
   Keep the area id in one comment shared with the baker.
6. **No new subsystem:** no boat behaviour, no new `wb_*` flag, no default change. DS-051 stays held.

## Required C144 cases / tests

| Case | Expected |
| --- | --- |
| `two-shores.obj --water 99.5` | 1 component, item/spawn reachable, `water_polys > 0`, area cost 4 > land 1 |
| same without `--water` | 2 components, item unreachable (baseline unchanged) |
| roof at z=105 over the gap | 0 water polygons under the roof, item remains unreachable; no path "through" the solid |
| island at z=100 inside the gap | 0 water polygons on the island; water only around it |
| no seabed / void gap | 0 water cells (unknown is not a water surface) |
| Steelmill no-water regression | navmesh/poly/area hash identical to current cs0.1/tile256/radius0.3/height1.8/climb0.5 |
| runtime cost test | `findPath` across water succeeds; reported/query cost > the same land route; water nearest-poly is not excluded |

The baker JSON should report `water: {z, cells, tris, polys, area_cost}` and the point summary should count
`two_way`/`reach` after water, so the C144 receipt can distinguish "connected by water" from "connected by a plane".

## Blocker / scope

The safe version needs a **second full-geometry pass** in `LoadObj` (the current z-filter discards the seabed) and
changes in both the baker and `BotNavMeshCore`. That is still existing-component work, but it is larger than the
one-line `--water` parser and cannot be validated without Codex's C144 build/oracle. Do not ship a global plane or
treat the seabed as walkable; either would create paths under solid terrain and silently pass the two-shores case.
No production Shore bake/install until the cases above are green.
