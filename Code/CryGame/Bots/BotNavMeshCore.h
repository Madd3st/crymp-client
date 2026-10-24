// Detour navmesh runtime for WarsBots (sprint C-047 P0, wb_bot_navmesh, default OFF).
// Loads tiles baked offline by Tools/RecastBake (recastnavigation v1.6.0, RecastDemo "MSET" layout) and answers
// path / reachability queries in the Crysis frame (Z up). Recast frame = (x, z, -y), a rotation, as in the baker.
// Engine-free: no CryEngine types, so the same code is unit-tested offline on the real baked meshes
// (Tests/BotNavMeshCoreTests.cpp). The engine adapter lives in BotBrain/BotWeapons (Vec3 <-> float[3]).
#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <cstddef>

class dtNavMesh;
class dtNavMeshQuery;
class dtQueryFilter;

namespace WarsBots {

class NavMeshCore {
public:
	NavMeshCore();
	~NavMeshCore();
	NavMeshCore(const NavMeshCore&) = delete;
	NavMeshCore& operator=(const NavMeshCore&) = delete;

	void Reset();
	bool LoadMemory(const unsigned char* data, size_t size, std::string* error = nullptr);
	bool LoadFile(const char* path, std::string* error = nullptr);
	bool Ready() const { return m_query != nullptr; }
	int Polys() const { return int(m_comp.size()); }
	int Components() const { return m_components; }

	// Nearest walkable point under/around p (Crysis frame): searches +-side m horizontally, `up` m above and
	// `down` m below p. Returns false when nothing is in range.
	bool Project(const float* p, float* out, float side = 1.5f, float up = 1.0f, float down = 3.0f) const;
	// Corner path start -> goal, excluding start (like BotNav::FindPath). False with a reason when either end is off
	// the mesh or the goal is in another connected part (Detour would return a partial path).
	bool FindPath(const float* start, const float* goal, std::vector<float>& xyz, const char** reason = nullptr) const;
	// O(1) after projection: both ends on the mesh and in the same connected part. `goalDown` lets items lying on
	// crates (up to ~2.2 m above the floor) count as reachable.
	bool Reachable(const float* start, const float* goal, float goalDown = 2.6f) const;

private:
	unsigned long long Nearest(const float* p, float* snapped, float side, float up, float down) const;
	void BuildComponents();

	dtNavMesh* m_mesh = nullptr;
	dtNavMeshQuery* m_query = nullptr;
	dtQueryFilter* m_filter = nullptr;
	std::unordered_map<unsigned long long, int> m_comp;   // poly ref -> connected part
	int m_components = 0;
};

} // namespace WarsBots
