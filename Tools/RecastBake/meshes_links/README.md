Detour tiles WITH off-mesh links (for C-036 jump/lift executors). Same bake as `claude-sprint:Tools/RecastBake/meshes`
(cs 0.1, tile 256, r 0.3) plus `--jumps` (drop <= 4 m, jump v0 4.43, Strength v0 9.60, horizontal 4.5 m/s, arc checked
with capsule rays against collision, one link per connected-part pair, cheapest motion) and, on Armada, 7 lift/ladder
links from the level `.cry` scene (`evidence/recast/Armada.movers.txt`).

Re-baked 25.10 12:10 after the OpenCode review (OC-024): jump arcs are now checked with **3 lateral lanes** (centre,
+-radius) x 3 heights scaled to the agent, and unknown poly refs are never mapped to poly 0. Stricter, honest numbers:

| map | spawns in main | items two-way | links drop / jump / Strength |
| --- | --- | --- | --- |
| Steelmill | 36/36 | 78/109 (walk-only 38) | 408 / 354 / 371 |
| Quarry | 31/32 | 102/108 (89) | 581 / 639 / 650 |
| Outpost | 35/35 | 118/123 (115) | 254 / 260 / 203 |
| Armada | 29/40 | 111/156 (70) | 176 / 286 / 170 (+7 lift/ladder) |

`<map>.links.txt`: `x0 y0 z0 x1 y1 z1 bidir kind` (Crysis frame), the same links as in the tiles; Detour user id =
line number (1-based) for the generated ones. Off-mesh polys carry flag 2 (walk polys flag 1): exclude flag 2 in the
query filter until the executor exists, include it after; `findStraightPath` marks link corners with
`DT_STRAIGHTPATH_OFFMESH_CONNECTION` — that is where `BotJumpLink`/`BotMoverLink` take over.
