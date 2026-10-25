# Review: `Tools/RecastBake/recast_bake.cpp` (OC-024 item 1)

Author: OpenCode (read-only). Reviewed file: `warsbots/research/steelmill-triage` `dfb226cc`, 589 lines.
Cross-checked against: OC-011 numbers (`ENGINE_GEOMETRY_EXPORT.md:174-183`), `PLAYER_MOVEMENT.md:67-86`,
Detour **v1.6.0** sources (vendored tree, `ThirdParty/recastnavigation/Detour/...`), and the v2/v3 evidence in
`Docs/Research/SteelmillTriage/RECAST-BAKE-2026-10-24.md`.

Verdict: the method is sound and the case numbers hold up. I found **3 real defects** (one of them matters only
for the in-game step, not for the verdict), **4 config drifts vs OC-011**, and 5 nits. No blocker for C-041/D-006.

---

## 1. Recast config vs OC-011

| Parameter | tool default (`Cfg`, :90-95) | v2/v3 command line | OC-011 (`ENGINE_GEOMETRY_EXPORT.md:174-183`) | Verdict |
| --- | --- | --- | --- | --- |
| cell size `cs` | 0.2 | 0.05 (0.1 for the in-game bake) | - | fine, but see §1.2: `minRegionArea` is in voxels |
| `ch` | 0.1 | 0.1 | - | fine |
| `radius` | 0.4 | **0.35**, v3 also **0.3** | 0.4 (stand capsule) | deliberate deviation, documented in the doc; 0.3 is 10 cm **under** the capsule -> path following must keep 0.4 m clear |
| `height` | 1.8 | 1.8 | **1.9** (headroom) | **drift**: 0.1 m too low, rejects crawl spaces the bot can stand in |
| `climb` | 0.5 | 0.5 | **0.75** (step test) | **drift**: rejects 0.5-0.75 m risers; the doc says "climb 0.75 changes nothing" for the tested maps, so the verdict is safe, but the *tool* should default to 0.75 |
| `slope` | 45 | 45 | 45 (`normalZ > 0.7` ~ 45.6 deg) | matches |
| `walkableRadius` | `ceil(r/cs)` (:107) | at 0.4/0.2 -> 2 cells (exact); at 0.35/0.05 -> 7 cells (0.35 exact) | - | no quantization loss in either run, good |
| `tile` | 128 cells | 6.4 m at cs 0.05 | - | fine |
| `borderSize` | `walkableRadius + 3` (:114) | 10 cells = 0.5 m | - | correct per rcConfig contract |
| `maxEdgeLen` | `12/cs` (:108) | 240 voxels = **12 m** | - | **dead value**: the tile is 6.4 m wide, a contour edge can never exceed it -> clamp to `tile*cs` |
| `detailSampleDist` | `cs*6` (:117) | 0.3 m | - | fine |

### 1.1 Player capsule (source of the `radius`)
`Player.cpp:3759-3769` sets only `pe_player_dimensions.headRadius = 0`; the capsule itself comes from the actor
entity params (Lua/`.ent`), so OC-011's 0.4 m radius / 1.9 m height come from the physics proxy measurements, not
from a constant in C++. Recommendation: cite that in the README next to the defaults, so nobody "fixes" 0.35 -> 0.4
without re-checking the 0.85 m doorway case.

### 1.2 `minRegionArea` / `mergeRegionArea` are voxel areas, not m^2
`:110-111` does `minRegion*minRegion` with `minRegion = 2` -> **4 voxels**. At `cs = 0.05` that is 0.01 m^2, i.e.
"no filtering": the bake keeps every 5-6 m^2 pocket (intended) but also every handrail-width island. The JSON
`regions`/`polys` numbers are therefore **not comparable between runs at different `cs`**. Fix: take the CLI values
in m^2 and divide by `cs*cs`, and print the effective voxel count in the JSON.

---

## 2. Frame conversion - correct

`ToRecast(x,y,z) = (x, z, -y)` (:43) is a rotation (det = +1), so triangle winding survives and CryENGINE's Z-up
becomes Recast's Y-up without a mirror. The inverse (:44) and the JSON bounds (:588) are consistent with it
(`y_crysis = -z_recast`, `z_crysis = y_recast`). No bug.

Nit: nothing checks that the input OBJ really is Z-up. A silently transposed input produces a horizontal mesh and a
plausible-looking "0 / N spawns on mesh" report. Cheap guard: after loading, require the walkable normal axis to be
Z (median |n.z| > 0.9) and fail loudly otherwise.

---

## 3. Off-mesh link placement per tile - correct (verified against Detour)

`:158-170` attaches a link to the tile whose **raw** xz box contains the link start, and sets
`p.bmin = pm->bmin` (the padded poly-mesh box) at :183.

Verified against the builder: `classifyOffMeshPoint` returns `0xff` when the point is **inside** `bmin/bmax`
(`DetourNavMeshBuilder.cpp:243-269`), the connection is stored only for `class == 0xff` (":635 Only store connections
which start from this tile", verts at :503), and the end point's side (`:645`) drives the cross-tile link
(`dtNavMesh::connectExtOffMeshLinks`, `DetourNavMesh.cpp:454`). Since the padded poly-mesh box strictly contains the
raw tile box, every generated link start is inside `p.bmin/bmax` -> stored, nothing is dropped at seams. Good.

One real edge case: `DetourNavMeshBuilder.cpp:344-348` silently demotes a link whose start is inside xz but outside
`[hmin,hmax]` (tile height range). For jump links the start is a floor point, so this cannot trigger; for the v3
**mover** links from a sidecar file it can (lift platforms above the tile's walkable band). Recommend: before
`dtCreateNavMeshData`, drop links with `start[1] < pm->bmin[1] || start[1] > pm->bmax[1]` and **count them** in the
JSON (`links_dropped`), so a silent 0-link run is visible.

---

## 4. Jump arc, clearance, landing

Physics constants match the engine: `g = 9.81` (:288) vs `PLAYER_MOVEMENT.md:80`; `v0 = 4.43` (apex 1.0 m) and
`v0 = 9.60` (apex 4.7 m) vs `PLAYER_MOVEMENT.md:72-75` (=433/96 in the 354/768 internal units). `vh = 4.5` (:94)
matches the run speed; the doc correctly notes that sprint+Strength at 6.75 is not covered.

**Defect 1 (real, medium-high): clearance is a single vertical line.**
`:323-324` casts three parallel segments at one (x,z) lane, heights +0.3/+0.9/+1.5.
The capsule is 0.8 m wide, so a 0.5 m window, a door frame or two catwalk rails 0.6 m apart all pass. This is the
same over-approximation that produced v1's "Strength links through walls", only now reduced to the lateral axis.
Fix: cast in three lanes offset by +-`radius` perpendicular to the motion direction (9 rays), all must be clear.
Also scale the heights with `c.height` (`0.15h/0.5h/0.85h`) - at height 1.8 the top 0.3 m is currently unchecked.

**Defect 2 (nit with a real failure mode): `index[r]` default-inserts.**
`:334` (`index[r]`) and `:532` (`index[ref]`) use `std::map::operator[]`; a ref missing from the map silently becomes
poly index 0, i.e. a link (or a spawn) is attributed to an arbitrary component instead of failing. Use `.find()` and
skip + count.

**Nits**
- Landing window `:331-332` accepts the arc point up to 0.15 m **above** the ground and up to 0.25 m laterally
  (`ext[xz] = 0.25`); worst case this is a 0.6 m effective step, still below `walkableClimb` 0.75, so it is safe
  today, but it must be stated (a reader assumes "exact landing").
- `maxFall = 4` (:288) has no cited source. If Crysis fall damage starts below 4 m this invents links a bot cannot
  use safely; if there is no fall damage in MP, say so and drop the limit.
- Take-off samples are only pushed outward (edge normal +-35 deg, :310-313). Jumps *across* a corner or *along* a
  catwalk to a parallel catwalk are not generated. Acceptable for this verdict, but it is why "one link per
  component pair" is enough - do not reuse this generator as the in-game link set.
- Dedup bookkeeping `:337-343` (`where` / `motionOf` / `stats[]`) is correct on re-read, but it is the kind of code
  that breaks silently on the next edit: `motionOf.size() < out.size()` resize + `stats[]` decrement for the old
  motion assume the resize happened. Recommend a 20-line unit test (three candidate motions for one component pair,
  assert one link with the cheapest kind and `stats` summing to 1).

---

## 5. SCC / "main" component - correct

`:493-519` is a correct iterative Kosaraju (finish order on `adj`, reverse pass on `radj`), no recursion -> safe on
300k+ polys. Off-mesh connections are part of the tile poly array (`dtMeshHeader::offMeshBase` .. `polyCount`,
`DetourNavMesh.h:257,268`), so directed jump links are in the graph and "main = SCC with most spawns" is the right
definition for one-way links. `GroundComponents` (`:248-272`) intentionally uses *undirected* components for link
generation - correct there, and documented at `:466-467`.

**Defect 3 (nit): point projection is generous for items.**
`:529` searches +-1 m sideways / 2 m down / 0.5 m up around `p[1]-0.75`. For a spawn on a floor that is fine; for an
item on a 0.8 m catwalk an item 1 m away on the neighbouring catwalk snaps to the wrong balcony, which is exactly the
kind of "item two-way 125/156" number the doc quotes. The JSON already stores `snap_m` - add a hard cap
(`--snap-max 0.5`) and report how many items were dropped.

---

## 6. Outputs

- `--out-obj` (:551-568) writes the **poly** mesh (not the detail mesh) - correct for a nav mesh, and `g main/other`
  plus `# comp N` per face is enough for the PNG renderer.
- MSET binary (:570-583) is versioned (`version = 1`) but the loader must be pinned to Detour **v1.6.0**; record the
  version string in the container header (2 bytes are free) before any in-game prototype reads it.
- JSON: add `input_sha256` and `cs/ch/radius/height/climb` of the *effective* config (it prints `params` already, so
  only the hash is missing) - the navmesh cache key in `DETOUR_INTEGRATION.md` needs both.
- `printf`-only output is locale-safe as long as `setlocale` is never called; keep it that way (a stray
  `std::locale` use would emit `0,35` and break every JSON reader).

---

## 7. Priority list for the Detour step

1. Defect 1 (lateral capsule clearance) - a 0.8 m-wide capsule must not use a 0.5 m gap.
2. `links_dropped` counter for `[hmin,hmax]`-demoted links (§3) - otherwise a 0-link run looks like a pass.
3. Defaults `climb = 0.75`, `height = 1.9` to match OC-011; keep `radius = 0.35` as an explicit, documented deviation.
4. `minRegionArea` in m^2 (§1.2) and `maxEdgeLen = min(12, tile*cs)` (§1).
5. `index[].find()` instead of `operator[]`, and `--snap-max` for items.

Acceptance for the re-bake: same case numbers as v2/v3 on all four maps with the new defaults, plus the two new
counters (`links_dropped`, `points_unsnapped`) in the JSON.
