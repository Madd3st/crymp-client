# DS-069: C145 candidate bake — capture origins, hold radius, spawns, US submarine

Inputs (read-only, no bake/game): `build/coordinator-c145/Shore.candidate.json`, `.navmesh`
(sha256 `07b0bf4e...`, 9,762,044 B), `Shore.bake-receipt.json` (tool 70aae712, input a943 hash verified),
`Shore.area-shapes.json`, `Shore.area-query.tsv`, `Shore.area-samples.txt`,
`Shore.spawn-classification.json`, `Shore.capture-area-paths.json`, extracted `ps-Shore.xml`.
Bake: 2099 tiles / 46,614 polys / 3803 components / 38.953 s; 124 spawns, 112 on mesh, 100 main.
Note (DS-068a): the tool's `PolyArea` omits the /2, so every `comp_area`/`area_m2` below is 2x the true value;
344/579 components are therefore ~10/~89 m², not 20/178. Relative sizes and the conclusions do not change.

## 1. Correction to DS-064: the 579 origin is not 0.17 m outside its area

DS-064 compared only the y coordinate. In 2D the entity origin is **13.3 m** from `Shape_VF_neutral` (569,
`ps-Shore.xml:2358`): the shape is a narrow diagonal strip x 2209.3..2233.0 / y 1326.0..1358.1, the origin is at
(2236.76, 1325.84). The origin is outside the polygon, not on its edge. 344 is genuinely close to both of its shapes
(0.67 m to 330, 0.38 m to 323), 654/64/316 origins are inside their shapes.

| id | origin (Crysis) | AreaShape | origin inside? | min 2D dist to polygon | origin comp (candidate) | in-area samples on mesh | reachable from NK / US seed |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 64 | 1965, 1387.75, 105 | 39 (h38) | yes | 13 m to edge | 3802 main | 428 / 438 | 408 / 0 |
| 316 | 2075.44, 1285.38, 126.18 | 291 (h38) | yes | 13 m to edge | 3802 main | 388 / 421 | 134 / 0 |
| 654 | 1799.38, 1686.25, 102.75 | 629 (h38) | yes | 13 m to edge | 3802 main | 420 / 424 | **0** / 0 |
| 344 | 2145.20, 1484.64, 110.03 | 330 h4 + 323 h7 | no | 0.67 / 0.38 m | **2880, 20 m²** | 100 / 123 | 0 / 0 |
| 579 | 2236.76, 1325.84, 124.32 | 569 h6 | **no** | **13.33 m** | **1878, 178 m²** | 54 / 55 | 0 / 0 |

## 2. The seeds, not the areas, explain most `0` columns

`area-query.cpp` uses NK `(1842.85, 1251, 111.27)` and US `(2111.88, 1882.5, 103.25)`.
The US point is one of the black submarine spawns, and that spawn is in **component 3795 (266 m²)**, not main
(`Shore.candidate.json:107-124`). Therefore `from_US=0` for all five areas says nothing about the areas; the seed is
inside the isolated submarine interior. `from_NK>0` for 64/316 shows the NK room connects to those two; `from_NK=0`
for 654/344/579 is a real negative *from that seed*, not a map-wide verdict.

Known-good real inside-area floor points (snapped, on mesh): 64 `(1943.75, 1367.25, 105.36)`; 316
`(2054.87, 1297.79, 125.15)` (both NK-reachable); 654 `(1774.88, 1678.0, 101.35)` (NK 0); 344
`(2126.07, 1481.26, 111.11)` (NK 0, inside shape 330/323 z-range 110.75+); 579 `(2212.57, 1335.24, 124.51)`
(NK 0, inside 123.32..129.32). So the floors exist and are inside the native boxes; the open question is routing.

## 3. What the hold radius can and cannot fix

`BotPSCapture.h:66` holds as soon as the 3D distance to the **entity origin** is <=6 m, then stops
(`BotBrain.cpp:630-641`).

* 64/316/654: origin inside the shape, nearest edge ~13 m away -> a 6 m hold is inside; radius is not the blocker.
* 344: origin 0.4-0.7 m outside; a 6 m stop is inside only if the approach comes from the area side. Ambiguous, a
  2 m hold near an inside point is safer.
* 579: origin 13.3 m outside; **no hold radius can ever put the bot inside the area** while aiming at the origin.
  This is a proven destination bug, independent of the navmesh.

Reachability (components/SCC) is a separate problem: 344/579 origins are on small isolated surfaces (20/178 m²), and
the candidate shows their in-area floors on the mesh but not reachable from the NK seed. A hold fix cannot make an
isolated area reachable; do not generate links to the pivot.

## 4. Spawns and the US submarine

From `Shore.spawn-classification.json` + `ps-Shore.xml`:

* tan `SpawnGroup2` (120, Base_NK): 20/20 on mesh, **all main**.
* neutral `SpawnGroup1` (96 high point, 211 high point, 243 east dock, 689 west dock): ~80 spawns, all main.
* black `SpawnGroup_US5` (537, Submarine_US): 23 spawns; **11 on mesh in comp 3795 (interior deck at z=103.25,
  266 m²), 12 off-mesh at z=90.38** (inside the hull, below the bake `--zmin 99.5`); **zero in main**.
* The 12 off-mesh and the 11 non-main spawns are therefore all own-base (black/sub), not neutral. No neutral spawn is
  off-mesh.

US shore connection: **none in this candidate**. The interior deck (103.25) is 3.75 m above the water plane (99.5),
the hull spawns are below it, and there is no navmesh opening from comp 3795 to main. The water mesh does not bridge
an enclosed deck. A hatch/exit or a spawn-policy change is a separate item; do not claim a US land route and do not
force links through the hull.

## 5. Narrow proposal (existing components, no subsystem)

1. **Destination = capture area, not entity pivot.** In `PSCaptureSnapshot`/`PsCapturePoint`, carry the position of
   the `captureAreaId` `AreaShape` entity (its `Pos`, e.g. 569 (2224.74, 1350.58, 123.32)) or, if available, the
   nearest inside-area point, as the goal; keep the capturable id for ownership/events. This alone makes 579 aim at
   the strip instead of 13 m away.
2. **Hold on the native area signal, not a guessed radius.** Hold when the bot is inside the area (the stock
   `PowerStruggleUtils` `inside` membership / `PS EVENT capture_start`) and within ~2 m of the destination. Keep 6 m
   only as the outer approach radius. Native event + 2 m replaces the 6 m pivot sphere.
3. **Skip unreachable points.** Before selecting, require the destination (or a sample inside the area) to be in the
   bot's reachable component; otherwise log `capture_unreachable` and pick the next point. Never teleport, never add
   a jump link to the pivot. This keeps 344/579 out of the goal list until their floor is reachable.
4. **Regression (offline, map data, no hardcode):** for all five, assert the destination is inside the AreaShape
   polygon (point-in-polygon) and within `bottom..bottom+height`, projects on the candidate mesh, and is
   `main`/reachable from at least one neutral seed. Extend `area-query` with a neutral seed
   `(1965.86, 1537.62, 101.10)` (west dock spawn 669) and a component/reach column; do not use the US submarine seed
   as the only US test.

## 6. Missing observations (cheapest next, no game)

1. Re-run the area query with the neutral seed and print `Project` + component + directed reach per sample. This
   separates "654 floor one-way/isolated" from "NK route only".
2. Bake `Shore.area-samples.txt` as `--points` once (the existing `--points` path) and report per-sample `main`,
   `comp`, `reach`; this is the authoritative component evidence.
3. If 344/579 floors are main but isolated by geometry, stop and decide: map/asset fix (stairs/ramp) vs leaving those
   two points uncapturable. No link generation to the pivot.

No game, no install, no accepted mesh; this document only diagnoses the candidate.
