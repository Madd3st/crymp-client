# BotNav graph export contract — `warsbots/botnav-graph/1`

Status: **specification only**. Not implemented; `BotNav.cpp` was not touched. Consumers already
exist in this branch: `Tools/SteelmillLab/navgraph.py` (validator/analysis), `wbmem.py --graph`
(memory linking) and the viewer. The fixture `Tests/SteelmillLab/fixtures/fixture_graph.json`
(flagged `"fixture": true`) follows this contract.

## 1. Goal

Write the in-memory graph exactly as `BotNav` holds it after `Phase::Ready`, so that offline tools
can draw nodes/edges, place `.mem` counters on positions and replay failed queries. The export is
a read-only view: it must not change build, A*, memory or timing.

## 2. File

* Path: `%USER%/WarsBots/<safe level name>.graph.json` (same `MemoryPath` sanitising as `.mem`),
  or an explicit path argument. Write to `<path>.tmp`, then rename.
* UTF-8 JSON, one object, no comments. Numbers: positions with 3 decimals, costs with 4.

```json
{
  "schema": "warsbots/botnav-graph/1",
  "fixture": false,
  "level": "Multiplayer/IA/Steelmill",
  "source": "BotNav::Export",
  "build_version": "v26-51-g8f0abd5f",
  "created_utc": "2026-10-24T03:00:00Z",
  "build": {
    "nx": 96, "ny": 63, "cell": 3.0,
    "min_x": 1121.25, "min_y": 1245.0, "min_z": 121.125, "max_z": 187.0,
    "max_layers": 5,
    "water_level": 0.0,
    "params": {"min_normal_z": 0.7, "headroom": [0.15, 1.9], "water_margin": 0.3,
               "rise_max": 1.9, "drop_max": 3.2, "step_max": 0.75, "end_tolerance": 0.8,
               "clearance_heights": [0.5, 1.2, 1.75], "ground_samples": [0.25, 0.5, 0.75, 1.0],
               "geometry": "ent_static|ent_terrain", "astar_expansion_cap": 6000,
               "nearest_radius_columns": 2, "nearest_max_dz": 2.5}
  },
  "counts": {"nodes": 7633, "edges": 41618, "truncated_edge_lists": 0},
  "node_columns": ["id", "x", "y", "z", "ix", "iy", "layer"],
  "nodes": [[0, 1122.75, 1246.5, 136.1, 0, 0, 0]],
  "edge_columns": ["from", "to", "cost"],
  "edges": [[0, 1, 3.0]],
  "memory_compat": {"nx": 96, "ny": 63, "cell": 3.0, "min_x": 1121.25, "min_y": 1245.0, "count": 7633},
  "fingerprint": "sha256 of the canonical node+edge text, see §5"
}
```

## 3. Fields

| Field | Exact meaning |
|---|---|
| `nodes[i][0]` id | index `i` in `m_nodes` — **must equal the array index** (this is the `.mem` record order) |
| x, y, z | `Node::pos` (floor hit + 0.05 m), world metres |
| ix, iy | column indices (as used by `Key(ix, iy)`), not `Node::column` (that field keeps only iy) |
| layer | position of the node in `m_columns[Key(ix,iy)]`: 0 = topmost accepted floor of the column |
| `edges` | flattened `m_edges` in node order: node *n* owns `edges[firstEdge..firstEdge+edgeCount)`; directed `from → to`, `cost` = stored `Edge::cost` (Euclidean, **without** the stuck penalty) |
| `counts.truncated_edge_lists` | nodes whose pending list exceeded 65535 (should be 0) |
| `build.*` | values used by this build (`m_nx`, `m_ny`, `m_cell`, `m_min`, `m_max.z`), water level read once |
| `memory_compat` | the exact tuple `BotNav::Load` compares; `count = m_memory.size()` |

## 4. Validation (implemented in `navgraph.py validate`)

ids == indices; finite positions; `0 ≤ ix < nx`, `0 ≤ iy < ny`; x/y equal the column centre within
1 cm; `min_z−1 ≤ z ≤ max_z+1`; `0 ≤ layer < max_layers`; unique (ix, iy, layer); edge endpoints in
range, no self loops, only between columns at Chebyshev distance 1; cost > 0 and equal to the
3D distance within 5 cm; `counts` equal array lengths. Analysis adds weak/strong components,
dead ends, degree, vertical edge classes and an offline replay of logged failed queries.

## 5. Matching saved memory

`wbmem.py MEM --graph GRAPH` links only when nx, ny, cell, min_x, min_y and count match and node ids
follow the array order. Because count equality is weak, the export carries `fingerprint` =
SHA-256 of lines `"%d %.3f %.3f %.3f %d %d %d\n"` for every node followed by `"%d %d %.4f\n"` for
every edge. Proposal (separate, later): a `WBMEM02` header storing this fingerprint so `Load`
can reject memory from a different graph. Not required for the export itself.

## 6. Size and performance

Steelmill: 7633 nodes × ~40 B + 41618 edges × ~20 B ≈ 1.2 MB JSON. Hard limits: refuse to export
above 2,000,000 nodes / 20,000,000 edges or 64 MiB output (the validator applies the same caps).
Serialisation is O(nodes + edges); one synchronous write of ~1 MB costs a few ms up to tens of ms on
the server thread, which is acceptable for an explicit console command, never per frame. No
allocation of graph-sized temporaries beyond one output buffer (or stream with `fprintf`).

## 7. Safe operation

* Console command, server only: `wb_nav_export [path]`. Refuse unless `Phase::Ready` (log
  `NAV export refused: phase=<p>`). Never export automatically or during the build.
* Read-only: no change to `m_nodes`, `m_edges`, `m_columns`, `m_memory`, timers, `m_dirty`.
* Log one line: `[WarsBots] NAV export: <path> nodes=<n> edges=<e> bytes=<b> fingerprint=<hex16>`.
* Works with the game running; does not need clients, bots or a QA runner.

## 8. Minimal task for Codex (copy as is)

> Add a read-only graph export to BotNav without changing navigation logic.
> 1. `BotNav.h`: `bool Export(const char* path) const;` (public). Nothing else in the class changes.
> 2. `BotNav.cpp`: implement `Export` per `Docs/Research/SteelmillLab/BOTNAV_EXPORT_CONTRACT.md`
>    §2–§3, §5 fingerprint, §6 limits; write `<path>.tmp` and rename; return false and log on error.
>    Derive `ix, iy` from `m_columns` keys (iterate the map, `ix = key >> 32`, `iy = (int32)key`)
>    and `layer` from the position in each column vector.
> 3. Register console command `wb_nav_export [path]` in `BotManager.cpp` next to `wb_nav`
>    (`kCommands[]` at :59 for unregistering, `AddCommand` at :102);
>    default path = `MemoryPath(level)` with extension `.graph.json`. Server only, refuse unless Ready.
> 4. Verify on Steelmill: `python Tools/SteelmillLab/navgraph.py validate <file>` → VALID, counts
>    7633/41618 (or whatever `NAV ready` printed), then
>    `python Tools/SteelmillLab/wbmem.py <copy of .mem> --graph <file>` → `graph link: OK`.
> Optional, separate commits: `reason=` on the `NAV fallback` line; success/failure counters in
> `NAV phase=` status; a log line for the roam-blocked `ReportStuck` (BotBrain.cpp:418).

## 9. Known limits of the export

It describes BotNav's sampled graph, not the real walkable space: narrow passages, ladders and
jump links are absent by construction; dynamic objects are not in the build. A missing edge is
therefore not proof that a human cannot pass.
