# Navigation findings (Steelmill, real logs)

Measured with `Tools/SteelmillLab` on copies of real logs (see STEELMILL_DATA_SOURCES.md).
**Confirmed** = logged by the bot/engine. **Measured** = engine values of one telemetry sample.
**Inferred** = derived across samples (2 s resolution) or joined by time. Without the graph
export, no statement here is about specific nodes or edges.

## 1. BotNav as implemented (`Code/CryGame/Bots/BotNav.cpp`, weapon-goals `53023d52`)

1. **Geometry**: `RayWorldIntersection` against `ent_static | ent_terrain` only (`kGeom`, :21). Dynamic
   entities (doors, rigid bodies, ropes, vehicles) are invisible to the build.
2. **Bounds**: spawn bbox −(40, 40, 15) / +(40, 40, 35) m (:59-60). Steelmill: 96×63 columns, cell 3 m,
   origin (1121.25, 1245.0), z 121..187 — reproduced exactly from the 36 mission spawns.
3. **Nodes** (`ScanColumn`, :69-93): one vertical ray per column centre; up to 5 hits top-down; a hit
   becomes a node if `n.z ≥ 0.7` (≤ ~45°), 1.75 m headroom ray is clear, not below water − 0.3 m.
   Stored in `m_nodes` (pos = hit + 5 cm) and `m_columns[(ix,iy)]` (top-down order = "layer").
   `Node::column` stores `Key & 0x7fffffff` = only `iy` (ix lost); it is never read, so harmless.
4. **Edges** (`Step`/`Walkable`, :95-172): to every node of the 8 neighbouring columns (all layers)
   if rise ∈ [−3.2, 1.9] m, three clearance rays (0.5/1.2/1.75 m) are clear and 4 ground samples
   along the segment have no step > 0.75 m and no drop > 3.2 m, ending within 0.8 m of the target.
   Directed, cost = Euclidean length. Diagonals are tested with the same rays (corner cutting
   only as wide as the rays). Max 65535 edges per node (uint16).
5. **Vertical transitions**: only what `Walkable` accepts between neighbouring columns — stairs/ramps
   with ≤ 0.75 m steps, drops ≤ 3.2 m. No ladders, jumps, Strength jumps or explicit links.
   One 3 m sample per column misses stairs narrower than the sampling.
6. **A\*** (`FindPath`, :207-253): `Nearest()` for start and goal searches the 5×5 neighbouring
   columns (±7.5 m), rejects |dz| > 2.5 m and requires a clear ray at +0.9 m; then A* with Euclidean
   heuristic, edge cost × (1 + 0.6·min(stuck, 6)) of the target node, **at most 6000 expansions**
   (79 % of the 7633-node graph). Failure reasons are not distinguished or logged.
7. **Memory** (`.mem`): per node `{stuck ≤ 20, kills, deaths}` (floats), saved every 120 s when dirty.
   Load accepts a file if nx, ny, cell, min_x, min_y and count match — **no geometry fingerprint**:
   a changed map with the same node count would silently misalign memory.
   `PickHotspot` samples only even node indices.
8. **Steer** (`BotBrain.cpp:87-116`): on failure, the bot walks a straight line to the goal ("NAV
   fallback"), retry after 2 s; success retries every 5 s.

## 2. Findings from real data

### F1. Spawn point 107 (EntityId 195) traps bots — confirmed by measurement
Position (1247.5, 1319.75, 136.25). Across 9 logs, **16 of 17 revives at spawn 195** left the bot
within 1.5 m of the spawn for the next 30 s; for every other spawn 1 of 936.
In the 48-min Play11Elite match, three bots stayed there for 8–11 minutes each
(484 s, 611 s, 676 s) with `requested=2.5`, `actual=0`, **`blocked=0`** in every sample — the case
"engine says not blocked, the bot does not move". They still shot and killed passers-by (14 kills
by one), the weapon goal (Shotgun 174, 10 m away) failed A* ~100 times and timed out 55 times.
Cause unknown (inferred candidates: spawn inside/under static geometry, a collision with a
dynamic entity at the spawn, or movement accepted by SafeStep but cancelled by physics).
Next step: in-game look at the spawn; log `pe_status_living`/ground contact for one revive there.

### F2. Permanent pits: bots blocked for the rest of the match — confirmed by measurement
`blocked=1` in every sample, no death, no recovery, until the log ended:

| Log | Bot | Position | Duration | A* failures |
|---|---|---|---|---|
| Play11Elite | 196520 (#1 universal/elite) | (1291.28, 1342.70, 139.82) | 1878 s (31 min) | 1112 |
| Play6 | 131012 | (1361.24, 1349.17, 142.56) | 1957 s (33 min) | 1172 |
| Play6 | 131037 | (1317.20, 1348.19, 149.35) | 1287 s (21 min) | 764 |

The 196520 case dropped 2.1 m (z 141.92 → 139.82) and from then on every goal failed — the
viewer shows its fallback patrol goals as three concentric rings (6/8/10 m, `BotBrain.cpp:406-407`)
around the spot. Strong indication that the bot's position is off-graph (`Nearest` = −1), not that
all goals are unreachable; needs the graph export to confirm. There is no self-recovery path for a
bot that is alive, not in combat and permanently blocked.

### F3. A* failures are frequent and mostly target weapons and spawns — confirmed
`NAV fallback` events: Play11Elite 8348 in 48 min, Play6 4445, 15-bot QA 330–414 in 300 s.
Play11Elite goals: 38 % exactly a mission weapon/ammo position, 20 % exactly a spawn point
(all 36 spawns are failed goals at some time), 42 % other (enemy last position, patrol rings).
Start→goal straight distance p10/p50/p90 = 6 / 21 / 75 m; 572 failures under 6 m.
Because successful queries are not logged, **no failure rate can be computed**.
Hypotheses (need the export): H1 goal off-graph (map weapons lie ~1 m above nearby floor, e.g. SMG 199
z 137.20 vs ground spawns 136.1–136.25; Nearest needs LOS at +0.9 m), H2 start off-graph (pits, mid-air, ledges), H3 separate
components (roofs, z 145–152 spawns), H4 the 6000-expansion cap with stuck penalties.
`navgraph.py analyze --movement` will classify every logged failure into these reasons.

### F4. Weapons bots never reach — confirmed
`weapon … unreachable` (10 s upgrade timeout) 1437 times in 9 logs, 548 for map-placed items.
SMG 199 at (1203.51, 1330.38, 137.20): 116 timeouts, **0 pickups** in all runs. Also SCAR 161
(49/1 pickup), SCAR 287 (47/1), Shotgun 273 (43/7). The 90 s skip list resets per life, so the
same unreachable item is chosen again after respawn (bot+goal pairs repeated up to 48 times).

### F5. "Wants to move, not blocked, not moving" — measured
Share of moving samples with `requested > 0`, `actual < 0.15 m/s`, `blocked = 0`:
Play11Elite 7.2 %, Play6 4.2 %, QA runs 0–3.3 %. In Play11Elite most of it is F1. Unblocked stall
windows (6 s, path < 0.8 m): 517 in Play11Elite, 189 in Play6.

### F6. Stuck learning is partially invisible — confirmed from source + logs
`ReportStuck` has three call sites; only `chase abandoned` (verbose) and `PROGRESS ESCAPE`
(experiment OFF by default) log. The roam-blocked report (`BotBrain.cpp:418`) is silent, so
`.mem` stuck totals (231 now, 5 nodes at the cap of 20) cannot be traced back to positions or times.
Map memory: kills 1105 / deaths 1110 on 543 / 558 nodes; one node holds 73 kills.

### F7. Map memory cannot be placed on the map yet — by design
`.mem` has no positions; node order depends on every column's layer count. `wbmem.py --graph`
links it only to an export with identical nx/ny/cell/min_x/min_y/count and id order.

## 3. Suggested next steps (for Codex; not done here)

1. Graph export (BOTNAV_EXPORT_CONTRACT.md) — unlocks F2/F3 diagnosis and memory placement.
2. Add `reason=` (start_off_graph / goal_off_graph / exhausted / cap) to the existing NAV fallback
   line and periodic success/fail counters to `NAV phase=…` status. Additive logging only.
3. Log the silent `ReportStuck` (BotBrain.cpp:418) with actor and position.
4. Investigate spawn 195 in game (F1) and add a recovery for permanently blocked idle bots (F2).
