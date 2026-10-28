# DS-089: water-query patch v3 - fixture RED, deep-float ten, RayClear advancement

Answer to Codex BLOCK `dca52434` (weapon-goals) on patch `f00f1843...` (`ds088_water_query.patch`).
Patch: `patches/ds089_water_query_v3.patch` (b06681ea, 8 files, `git apply --check` OK, `git diff --check` clean).
Offline only: no game, build or install; Codex compiles.

Keeps the DS-088 water-only policy (no dry/seabed/stacked fallback) and adds the three corrections below.

## 1. Fixture: the raw-dry RED assumption was wrong (BLOCK item 1)

The synthetic fixture's polygon plane is **not** z=100. `dtCreateNavMeshData` is fed `bmin[1]=bmax[1]=100` while the
vertices are at local 0, and the effective snapped height differs; the previous test hard-coded `100` for both the
plane and the swimmer, so `plane-1.5 = 98.5` was still inside the dry box (`[98.5-3, 98.5+1]` -> up to 99.5) and the
RED assertion failed under MSVC (`raw dry projection unexpectedly found`, exit 5).

Fix (in `Tests/BotNavMeshWaterTests.cpp`): **measure the plane** from a known on-mesh land point
(`nav.Project((5,-15,100)) -> planeZ`, asserted 95..105), then build every control from it:

* `swim = planeZ - 1.5` -> dry `Project`/`Reachable` must be false (RED, b06681ea behaviour);
* `WaterQueryBounds(1.5)` -> projects onto the water poly and reaches land (GREEN);
* `deep = planeZ - 2.3` with a fixed `up 2.0` box -> must fail; with `WaterQueryBounds(2.3)` -> must reach
  (the C147 deep-float class);
* land start with water bounds -> `start_not_water`; dry land unchanged;
* outside-the-mesh start -> fail closed;
* `RayClear(swim, land)` raw -> false; with the water bounds -> true.

No assertion is deleted or weakened; the constant is replaced by a measured value.

## 2. The extra ten misses are deep floats, not holes (BLOCK item "1085 vs 1095")

C147 native data: the feet->native-plane offset is **1.176..2.308 m**, not a constant 1.5-1.8. `lift > 2.0` occurs in
12 samples (t 14.28-15.28 and 68.56-70.03, max 2.308). The fixed `up 2.0` box from the feet reaches only `feet+2.0`,
so those rows fail `start_off_mesh` even though the same XY at `waterZ` projects (the 1095 counterfactual). That is
exactly the 1085/1136 result; the 41 real holes are separate.

Fix: `WaterQueryBounds(lift)` sizes the box as `clamp(lift + 0.30, 1.2, 2.8)` - the native plane plus a small
margin, never wider than 2.8 m. The acceptance is still **water-only** (`IsWaterPoly`, area 4) and `down` stays 0.2,
so a wrong/deep plane cannot widen the search onto a deck or seabed; dry callers pass no bounds at all.

## 3. RayClear gates corner advancement (BLOCK item 2)

`SteerCorners` calls `RayClear(rawFeet, nextCorner)` when `CornerReached` reports a plane crossing beyond the 0.35 m
reach; a false ray `break`s and the corner index is **not advanced** - this is real advancement, not an optional
fast-skip. From swim feet the old `Nearest(a, 0.5, 1.0, 2.0)` could not see the water poly (C147: 0/1085 rays).

Fix: `RayClear` takes the same optional water bounds and resolves its start through one shared
`NavMeshCore::StartRef` (water-only, fail closed); `SteerCorners` passes the plane-sized box when
`waterMovement`. The dry ray keeps its old extents (`0.5/1.0/2.0`), so dry behaviour is byte-identical.

## 4. Oracle re-run (exact)

In `build/ds088-offline/check.cpp` only two lines change (the box must be sized per row, and the water ray counted):

```cpp
auto box = WarsBots::WaterQueryBounds(w - z);      // was WaterQueryBounds()
ray += m.RayClear(feet, xyz.data(), &box);         // was m.RayClear(feet, xyz.data())
```

Expected with v3 on E2235993 + one-row links + mask14, goal 654:

| measurement | expected |
| --- | ---: |
| raw-feet Project (control) | 0/1136 |
| water Project / Reachable654 / FindPathLinks654 | **1095/1136** |
| `start_off_mesh` failures | **41** (the characterised holes) |
| `start_not_water` failures | 0 |
| raw-feet RayClear to first corner | 0/1085 (unchanged, control) |
| water-box RayClear to first corner | 1085-1095 (all successful paths) |

Still failing and expected: the 38-row shoreline void at `x~2134` and the two open-water voids
(`2079.19,1756.87`, `2109.09..2109.50,1709.62..1710.45`) - bake holes; the patch fails closed there.

## 5. Shore boundary (not claimed)

The first entry path (14 corners) is `99.511 -> 101.711/101.811 -> 99.511 -> ... -> goal`. v3 does not change dry
connectivity (dry starts pass no bounds and use the old box), so that path is the mesh's own; the native
water->dry handover still happens only when `ShouldSwim` ends and the dry caller takes over. Accepting that handover
remains a runtime observation (one shared upper->sea->shore run after green exact SHA + review), not a graph claim.

## 6. Scope / unknowns

8 files, +158/-23, no new CVar, no default change, no actor/teleport, no boat, no forced link, no B1/upper-Strength
change. The 41 bake holes and the ~2-4 m shoreline void remain data items; nothing here widens `Nearest` globally.
