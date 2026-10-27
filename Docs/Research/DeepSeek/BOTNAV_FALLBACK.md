# BotNav graph fallback removal (DS-051 #2)

Base `493f2cab` lines. The navmesh is the only path source; the sparse `BotNav` A* graph is no longer consulted by
the brain or weapon selection.

| Caller (base) | Replaced by |
| --- | --- |
| `BotBrain.cpp:151` `Nav()` in `Steer` | removed; `CurrentNavMesh()` only |
| `BotBrain.cpp:154` `!nav.Ready() && !mesh -> goal` | `if (!mesh) return goal;` (mesh off = fail-closed straight leg, no graph) |
| `BotBrain.cpp:161-176` `BotNav::PathTrace` / `nav.FindPath` | mesh `FindPathLinks`; on failure no fallback; log `NAVMESH path failed` |
| `BotBrain.cpp:761` `Nav().PickHotspot` | `false` (no graph hotspot; roam keeps mesh patrol goals) |
| `BotBrain.cpp:790`, `:1097` `Nav().ReportStuck` | removed |
| `BotWeapons.cpp:99-102` `GoalFilterEnabled/NodeAt` | removed; navmesh `Reachable/FindPath` already filters |
| `BotWeapons.cpp:154-155` `KnownUnreachable` | removed (`GOAL SKIP unreachable` line dropped) |
| `BotWeapons.cpp:171` graph SCC log | removed |

Still present, but no brain/weapon caller: `BotManager::m_nav` build (`:478`), `wb_nav`/`wb_nav_audit`/`wb_nav_export`/
`wb_nav_explain` (`:866/997/1051/1059/1061`), `BotNav.cpp` graph/audit/export and its unit tests. Codex can delete the
build and commands after the 4-map navmesh-only gate; removing them now would also drop the offline audit tools.

Mesh-off policy: `Steer` returns the goal (straight leg); weapon goals run without graph filtering but still use
`mesh->Reachable` when a mesh exists. At C-119 `wb_bot_navmesh 1`, so this is behaviour-identical.
