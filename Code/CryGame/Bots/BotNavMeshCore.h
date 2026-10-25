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

// Off-mesh link kinds (meshes_links bake, C-036). Poly flags: walk 1, then one bit per kind so the query filter can
// enable only the kinds that have an executor.
enum NavLinkKind { NavLinkDrop = 0, NavLinkJump, NavLinkStrength, NavLinkLift, NavLinkLadder, NavLinkKinds };
inline unsigned short NavLinkFlag(int kind) { return (unsigned short)(2u << kind); }
inline const char* NavLinkName(int kind)
{ static const char* n[] = {"drop", "jump", "strength", "lift", "ladder"}; return kind >= 0 && kind < NavLinkKinds ? n[kind] : "?"; }

// One off-mesh link on a corner path: corner `corner` (index into FindPath's xyz/3) is the take-off point a,
// corner `corner + 1` the landing point b.
struct NavPathLink { int corner = 0; int kind = 0; float a[3] = {0, 0, 0}, b[3] = {0, 0, 0}; unsigned long long ref = 0; };

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
	// O(1) after projection: both ends on the mesh and the goal's part reachable from the start's part (directed). `goalDown` lets items lying on
	// crates (up to ~2.2 m above the floor) count as reachable.
	bool Reachable(const float* start, const float* goal, float goalDown = 2.6f) const;

	// C-036 links. `<map>.links.txt` (x0 y0 z0 x1 y1 z1 bidir kind; Detour user id = 1-based line) gives each
	// off-mesh poly its kind flag; lines without a known kind (lift/ladder movers) are never used. Without the file the
	// caller keeps the mask 0 (walk only).
	int LoadLinkKinds(const char* text);
	// Which link kinds paths may use (bit per NavLinkFlag); 0 = walk only. Rebuilds the connected parts.
	void SetLinkMask(unsigned short mask);
	unsigned short LinkMask() const { return m_linkMask; }
	int Links() const { return m_links; }
	int LinkMismatches() const { return m_linkMismatch; }   // links whose user id line did not match by end points
	// A link failed in the game: never use it again on this level.
	void BanLink(unsigned long long ref);
	// Counts in-game failures of a link; true from the second failure on (then the caller bans it).
	bool NoteLinkFailure(unsigned long long ref) { return ++m_linkFails[ref] >= 2; }
	// FindPath + the links on the returned corners.
	bool FindPathLinks(const float* start, const float* goal, std::vector<float>& xyz, std::vector<NavPathLink>& links,
		const char** reason = nullptr) const;

private:
	unsigned long long Nearest(const float* p, float* snapped, float side, float up, float down) const;
	void BuildComponents();
	bool PartReaches(int from, int to) const;

	dtNavMesh* m_mesh = nullptr;
	dtNavMeshQuery* m_query = nullptr;
	dtQueryFilter* m_filter = nullptr;
	std::unordered_map<unsigned long long, int> m_comp;   // poly ref -> connected part
	int m_components = 0;
	std::vector<unsigned long long> m_reach;              // part x part reachability bits (one-way links)
	int m_words = 0;
	unsigned short m_linkMask = 0;
	int m_links = 0;
	std::unordered_map<unsigned long long, int> m_linkFails;
	int m_linkMismatch = 0;
};

} // namespace WarsBots
