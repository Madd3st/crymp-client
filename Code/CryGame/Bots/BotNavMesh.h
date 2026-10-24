// Engine side of the Detour navmesh (sprint C-047 P0): one mesh for the current level, loaded lazily from
// %LOCALAPPDATA%/WarsBots/Navmesh/<Level>.navmesh (baked by Tools/RecastBake). Null when wb_bot_navmesh=0, the
// file is missing or invalid; callers then keep the old BotNav graph path.
#pragma once
namespace WarsBots {
class NavMeshCore;
NavMeshCore* CurrentNavMesh();
}
