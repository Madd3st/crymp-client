# BotNav graph export contract (wbnav/1)

Status: **proposal for Codex to implement**. Consumer: `Tools/SteelmillTriage` (`navgraph.py`
already validates and replays this format; tests use a synthetic graph). No game code
was changed by this proposal.

## Why

`<level>.mem` (WBMEM01) stores only `{stuck, kills, deaths}` per node index. Node
coordinates, columns and edges are rebuilt from physics at every level load and are
never written. Node *i* is "the i-th accepted floor in column-scan order", and the number
of floors per column is not stored, so the memory cannot be placed on the map. Every
claim about graph connectivity, roof links or projection failures is currently a guess.

## Command

`wb_nav_export [path]` — server console, local IA only, same gates as other `wb_*` commands.

- Runs only when `BotNav::Ready()`; otherwise prints `NAV export refused: phase=<phase>`.
- Default path: `%USER%/WarsBots/<safe level>.nav.json` (same directory and name
  sanitising as `MemoryPath`). Never overwrites `.mem`, never changes memory or the graph.
- Writes to `<path>.tmp` then renames, so a reader never sees a partial file.
- Logs one line: `NAV export: <path> nodes=<n> edges=<e> bytes=<b> ms=<t>`.
- Cost: one pass over existing arrays, no rays. Allowed to take one frame (~1 MB JSON).

## Format

UTF-8 JSON, one object. Floats with 2 decimals for positions and 3 for costs.

```json
{
  "schema": "wbnav/1",
  "level": "Multiplayer/IA/Steelmill",
  "build": { "describe": "v26-51-g8f0abd5f", "exported_at_game_seconds": 412.3 },
  "params": { "layers": 5, "headroom": 1.9, "max_rise": 1.9, "max_drop": 3.2,
              "max_step": 0.75, "min_normal_z": 0.7, "clearance_heights": [0.5, 1.2, 1.75],
              "nearest_radius_columns": 2, "nearest_max_dz": 2.5, "astar_budget": 6000,
              "margin_xy": 40, "margin_z_below": 15, "margin_z_above": 35 },
  "grid": { "nx": 96, "ny": 63, "cell": 3.0, "min_x": 1121.25, "min_y": 1245.0,
            "min_z": 121.0, "max_z": 187.0 },
  "water_level": 0.0,
  "counts": { "nodes": 7633, "edges": 41618 },
  "nodes": [[x, y, z, column], ...],
  "edges": { "first": [...], "count": [...], "to": [...], "cost": [...] },
  "spawns": [[entityId, x, y, z], ...],
  "memory": { "file": "Multiplayer_IA_Steelmill.mem", "matched": true }
}
```

Rules the exporter must keep (the Triage validator rejects violations):

1. `nodes[i]` is exactly `m_nodes[i].pos` and `column` is `m_nodes[i].column` — same
   index as the `.mem` record, so memory overlays without remapping.
2. `edges` is the existing CSR: `first[i] == m_nodes[i].firstEdge`, `count[i] ==
   m_nodes[i].edgeCount`, `to/cost` copied from `m_edges`. Directed, as built.
3. `grid` values are the ones `Load()` compares (`nx, ny, cell, min_x, min_y`) plus the
   node count; with a matching `.mem` this is the fingerprint.
4. `spawns` come from `GameRules::GetSpawnLocation` (the same set that sized the grid).

Optional block `diagnostics` (nice to have, not required for v1): per-node rejected-edge
counts by reason from `Walkable()` (`rise`, `clearance`, `hole`, `step`, `endpoint`).
That turns "why is there no edge here" from guesswork into data, but it requires
counters inside `Walkable()`, so it is a separate small change.

## Log fields wanted together with the export (same small patch)

These make existing traces decisive instead of ambiguous. All are additive, level 2.

| Line today | Add | Why |
| --- | --- | --- |
| `#N NAV fallback start=.. goal=..` | `actor=<EntityId> reason=<start_unprojected\|goal_unprojected\|budget\|no_route> from=<node> to=<node> expanded=<n>` | 55% of failed queries target another floor; cannot separate projection from topology now |
| `BOT SUIT`, `JUMP TAKEOFF/LANDING`, `BOT ACTION`, `ACTION REFUSED` | `actor=<EntityId>` | per-bot suit and jump features cannot be joined |
| new: `NAV stuck report actor= node= pos=` in `ReportStuck` | — | memory growth is not traceable to a location in logs |

## Acceptance

- `python Tools/SteelmillTriage/steelmill_triage.py graph <export> --mem <same level .mem>` exits 0.
- Node/edge counts equal the `NAV ready:` line of the same session.
- Re-export twice in one session gives byte-identical `nodes/edges`.
- No change in `PROFILE` frame cost outside the export frame.
