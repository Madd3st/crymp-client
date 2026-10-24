# Real Recast bake on 4 IA maps (OC-022, done by Claude under D-010) — 2026-10-24 22:10

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
- Evidence: `evidence/recast/<map>.png` (green: main by height, grey: other parts, orange/blue/magenta: drop/jump/
  Strength links, white/red circles: spawns in/out of main, triangles: items), `<map>.json` (per stage: mesh,
  summary, outside spawns; per point at the last stage), `<map>.links.txt` (generated links, Crysis frame:
  `x0 y0 z0 x1 y1 z1 bidir kind`) — candidates for the Detour step; in-game links still need `wb_link_test` (C-036).
- `--out-bin` writes the tiles in RecastDemo's MSET layout for the later in-game Detour load (not committed: binaries).

## Not done / risks

- No ladder links yet (needs C-041 data). No lifts/doors (dynamic entities).
- Strength horizontal speed fixed at 4.5 m/s; sprint+Strength jumps (6.75 m/s) would add links — run with `--vh 6.75`.
- Link heights: one link per component pair is enough for this verdict, not for good in-game paths (more links,
  costs per motion, energy for Strength) — that is the Detour step's job.
