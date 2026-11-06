// Engine side of the Detour navmesh (sprint C-047 P0): one mesh for the current level, loaded lazily from
// %LOCALAPPDATA%/WarsBots/Navmesh/<Level>.navmesh (baked by Tools/RecastBake). Null when wb_bot_navmesh=0, the
// file is missing or invalid; callers then keep the old BotNav graph path.
#pragma once
namespace WarsBots {
class NavMeshCore;
NavMeshCore* CurrentNavMesh();
// SPEC-004 runtime of the current level (wb_bot_unstick): navmesh hash, remembered directed transitions with their file
// %LOCALAPPDATA%/WarsBots/Memory/<Level>.unstick.txt (bound to the navmesh hash), and the trodden map (no file).
class TroddenMap;
const char* CurrentNavMeshHash();
TroddenMap* CurrentTrodden();
// Game-seconds clock of the transition memory: stored with the file, so the half-life spans matches.
float UnstickClock(float now);
// D-042: one FindPathLinks took `ms` wall-clock milliseconds; logs PATHTIME percentiles every 60 s and at level end.
void PathQueryTimed(float ms, float now);
// A lone bot failed (or needed Strength) on from -> to: remember it, save the file, refresh the Detour penalties.
void UnstickRecord(const float* from, const float* to, float now);
class VantageMap;
// Baked sniper spots for the current level (<Level>.spots.txt next to the navmesh), nullptr if missing/disabled.
VantageMap* CurrentVantage();
struct CoverCandidate;
// Cover / low-cover spots (flags 1|2) of the current level with their protected sides; nullptr if none or disabled.
const CoverCandidate* CurrentCover(int& count);
}
