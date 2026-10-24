# Real Recast bake on 4 IA maps (OC-022, done by Claude under D-010) — 2026-10-24 22:40 (v2)

## v3 (00:05) — lifts and ladders from the level's editor scene

The installed levels ship their editor scene (`<Map>.cry` → `level.editor_xml`, read-only copy). `Tools/RecastBake/level_movers.py`
lists Elevator / ElevatorSwitch entities and ladder / stairs brushes (`evidence/recast/movers.json`):
Armada **3 elevators** (two carrier lifts, hangar 110 m ↔ flight deck 121 m, `fFloorHeight` 11; one island lift
`Elevator_WES` 117.75 ↔ 122.5) + 9 switches, 2 ladders (`ladder_h`, `ladder_armada_a`), 39 stair brushes; Steelmill 4
ladders (water tower / chimney), Quarry 4 (tanks), Outpost 3 (scaffolding). `mover_links.py` turns them into two-way
off-mesh links between the mesh levels next to each object (`evidence/recast/Armada.movers.txt`, 7 links; `Elevator_right`
found no platform level yet).

Armada with lift/ladder links (cs 0.05): walk + lifts/ladders **19 → 29 / 40**; + jumps 34 / 40; with **r 0.3: 37 / 40**,
items 151 / 156 (r 0.25: 38 / 40). The last 2–3 spawns sit on 5–6 m² pockets (spawn corners). → Armada is solved by
data; **C-041 is no longer needed** (only the lift *use* — press switch, wait, ride — is new bot behaviour, with C-036).
Detour step: bake at r 0.3 (cs 0.1 or finer on Armada), add mover links.

## v2 result (22:40)

**The first bake (22:10, below) was too coarse.** Cell 0.2 m with radius 0.4 erodes ceil(0.4/0.2) = 2 cells plus
quantization, so ship doorways and factory stairwells (~0.8–1.0 m clear) vanished. The player stand capsule is
r 0.4 (Player.cpp:2604 `size = Vec3(0.4, 0.4, 0.3)`), and a real 0.85 m doorway still lets it through. v2: **cell
0.05 m, radius 0.35**, everything else unchanged. Evidence `evidence/recast/` is v2; v1 moved to `evidence/recast/r04/`.

| Map | Spawns in main: walk → all links | Items two-way: walk → all links | v1 (r 0.4 / cs 0.2) all links |
| --- | --- | --- | --- |
| Steelmill | **36 / 36** → 36 / 36 | 54 → **88 / 109** | 32 / 36, 90 |
| Quarry | 30 → **32 / 32** | 89 → **108 / 108** | 32 / 32, 108 |
| Outpost | **35 / 35** → 35 / 35 | 117 → **121 / 123** | 34 / 35, 115 |
| Armada | 19 → **34 / 40** | 72 → **125 / 156** | 6 / 40, 26 |

Corrections to my own 22:10/22:00 claims: Steelmill upper floors are **not** ladders and Armada is **not** closed
doors — it was the erosion. Removing door-like parts (heuristic, 103 leaves 1.13 × 0.13 × 1.98 m) changed nothing;
radius 0.3 at cs 0.1 gives the same picture as v2 (Steelmill 36/36, Armada 35/40). Owner (22:00): "Armada has one
lift, otherwise every point is reachable on foot"; the 6 Armada spawns still outside (294, 465, 152, 417, 441, 373)
are the candidates for the lift / ladders / steep ship stairs → C-041 narrowed to ladders + lift.

**For the in-game Detour step (C-030 item 9):** bake at cs 0.1, radius 0.3 (Steelmill 9.3k polys, 2.2 MB tiles;
Armada 3.4k polys, 0.7 MB) — same connectivity as v2 at a quarter of the memory; path following must keep the 0.4 m
capsule off walls (corridor + steering), and narrow doors get `wb_link_test`-style probes.
Walk-only items stay low on Steelmill (54/109): most weapons sit on roofs/catwalks reached by jumps — vertical play
is first-class (D-006), the jump executor (C-036) is not optional.

---

## v1 (22:10, superseded)


Tool: `Tools/RecastBake/recast_bake` (recastnavigation **v1.6.0**, MIT, pinned; offline, not game code). Input: the same
Codex C-019/C-028 cropped collision OBJ as the walk-model verdict. Tiled Detour navmesh, cell 0.2 m / 0.1 m, agent
r 0.4 / h 1.8, climb 0.5, slope 45° (same as `NAVMESH-VERDICT-2026-10-24.md`). Bake + link generation: 3–16 s per
map on one core.

Jump links are generated **on the baked mesh, with real geometry clearance**: take-off every 0.8 m along open polygon
edges, three headings, arc integrated at 50 Hz (normal jump v0 4.43, Strength 9.60, drop v0 0, horizontal 4.5 m/s),
capsule rays at +0.3/+0.9/+1.5 m against the collision triangles, landing on a polygon of another component, fall ≤ 4 m.
One link per (source component → target component), cheapest motion wins. "Main" = the strongly connected component
holding most spawns (links are one-way).

| Map | Spawns in main: walk → +drop → +jump → +Strength | Items two-way: walk → … → +Strength | Walk model (21:10) for comparison |
| --- | --- | --- | --- |
| Steelmill | 30 → 30 → 31 → **32 / 36** | 53 → 53 → 66 → **90 / 109** | spawns 30 → 36, items 9 → 103 |
| Quarry | 27 → 28 → 31 → **32 / 32** | 77 → 81 → 97 → **108 / 108** | spawns 28 → 32, items 83 → 108 |
| Outpost | 32 → 32 → 33 → **34 / 35** | 100 → 100 → 108 → **115 / 123** | spawns 32 → 35, items 78 → 118 |
| Armada | 6 → 6 → 6 → **6 / 40** | 25 → 25 → 25 → **26 / 156** | spawns 6 → 39, items 24 → 151 |

> **Update 22:00 (owner):** Armada has one lift; otherwise every point is reachable on foot. So Armada's split is an
> export problem, not the map: the hangar north wall has no opening ≥ 0.9 m in the collision while rooms behind it
> hold spawns → doors exported closed (and/or stair/ladder entities). C-041 now asks for a per-part sidecar
> (class, physics type, surface) to open doors and list ladders + the lift.

## What this changes

1. **Walk-only: the walk model was too pessimistic on catwalks/stairs** (audit risk 3 confirmed): Steelmill items on
   foot 9 → **53**, Outpost 78 → 100. Recast's span merging keeps stairs and catwalks that our 0.4 m erosion on a
   point-sampled grid lost. For in-game navigation, take Recast, not the Python model.
2. **The walk model's jump links were too generous**: it checked the arc only against its own span grid, so Strength
   links went through walls/windows. With ray clearance against the real triangles Steelmill stays at 32/36 and
   **Armada does not connect at all (6/40)**. The 21:10 claim "Armada 39/40 with Strength" was optimistic — corrected.
3. **The remaining gaps are ladders, not jumps.** Steelmill's 4 outside spawns (104, 136, 268 in the north-east
   factory hall, 297 in the small building at 1306,1368) stand on upper floors at 146–152 m; the collision there has a
   floor at 138 m and the next at 146 m with **nothing in between** — no stairs, no ramp (checked: up-facing area
   histogram, sloped area 6 m²). Armada: hangar deck 110 m and flight/upper decks 117–121 m, steep surfaces don't help
   (slope 60–75° / climb 0.75 change nothing). In CryENGINE 2 a ladder is static geometry with surface type
   `mat_ladder`, used through `CPlayer::IsLadderUsable` (Player.cpp:6972: look-at ray ≤ 2 m, part AABB, vertical
   ≥ 0.85) — it is in the collision export but we cannot tell it apart. Bots today stop moving on a ladder
   (`BotBlackboard.cpp:20` `canMove = … !isOnLadder`).
   → **C-041 (Codex): export ladders** (per-part surface == `mat_ladder`: world AABB, top, bottom, orientation —
   exactly what `IsLadderUsable` computes), then ladder links in the bake, then a ladder executor (C-036 family).
   This answers the owner's Armada question as far as offline data can: the decks connect by ladders (and possibly
   stairs that are not collision) — owner confirmation still welcome.

## Files

- Tool: `Tools/RecastBake/` (`recast_bake.cpp`, `CMakeLists.txt` fetches v1.6.0, `render_mesh.py`, README).
- Evidence (v1 now in `evidence/recast/r04/`): `evidence/recast/<map>.png` (green: main by height, grey: other parts, orange/blue/magenta: drop/jump/
  Strength links, white/red circles: spawns in/out of main, triangles: items), `<map>.json` (per stage: mesh,
  summary, outside spawns; per point at the last stage), `<map>.links.txt` (generated links, Crysis frame:
  `x0 y0 z0 x1 y1 z1 bidir kind`) — candidates for the Detour step; in-game links still need `wb_link_test` (C-036).
- `--out-bin` writes the tiles in RecastDemo's MSET layout for the later in-game Detour load (not committed: binaries).

## Not done / risks

- No ladder links yet (needs C-041 data). No lifts/doors (dynamic entities).
- Strength horizontal speed fixed at 4.5 m/s; sprint+Strength jumps (6.75 m/s) would add links — run with `--vh 6.75`.
- Link heights: one link per component pair is enough for this verdict, not for good in-game paths (more links,
  costs per motion, energy for Strength) — that is the Detour step's job.
