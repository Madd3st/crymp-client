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

// DS-087: query box bounds for one end of a path/reachability query. The default keeps the dry-floor search
// (`up 1 m`); a native swimmer's feet sit 1.5-1.8 m below the baked water surface, so the water-aware caller
// passes a bounded box that reaches the surface without selecting the submerged floor.
struct NavQueryBounds { float side = 1.5f, up = 1.0f, down = 3.0f; };

// DS-088: the bounded water start box. Used only for a native water movement start; the core accepts it only when
// the projected polygon is the baked water area, otherwise it fails closed (no dry/seabed/through-deck fallback).
// DS-089 (C147 actual 1136 rows): the float offset is not a constant 1.5-1.8 m - it ranged 1.18..2.31 m, and a
// fixed up 2.0 lost 10 rows whose feet were 2.05-2.31 m below the native plane (they failed start_off_mesh from the
// *feet* while the same XY at waterZ projected). Size the box to the native plane, bounded by the swimmer body:
// lift + 0.30 m margin, never above 2.8 m, so a wrong/deep plane cannot widen the search arbitrarily.
inline NavQueryBounds WaterQueryBounds(float lift = 0.f)
{
	NavQueryBounds b{1.5f, 2.0f, 0.2f};
	if (lift > 0.f)
	{
		const float needed = lift + 0.30f;
		b.up = needed < 1.2f ? 1.2f : (needed > 2.8f ? 2.8f : needed);
	}
	return b;
}

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
	bool Project(const float* p, float* out, float side = 1.5f, float up = 1.0f, float down = 3.0f, bool waterOnly = false) const;
	// Distance from p (projected onto the mesh) to the nearest mesh edge within maxRadius, and the horizontal unit
	// direction from that edge into the mesh (Crysis frame). The mesh is eroded by the bake radius (0.3 m) while the
	// player capsule is 0.4 m, so paths that hug an edge rub walls; callers push corners inward by the difference.
	// False when p is off the mesh. dist = maxRadius and dir = 0 when no edge is in range.
	// Straight walk a -> b stays on the mesh (Detour raycast along the surface from a's poly). Used before skipping a
	// path corner: a corner reached "within 1.5 m" is only passed when the next one is in straight reach, otherwise
	// the bot cuts the turn into the wall the corner was going around.
	bool RayClear(const float* a, const float* b, const NavQueryBounds* startBounds = nullptr) const;
	bool WallClearance(const float* p, float maxRadius, float& dist, float* dirXY) const;
	// Corner path start -> goal, excluding start (like BotNav::FindPath). False with a reason when either end is off
	// the mesh or the goal is in another connected part (Detour would return a partial path).
	bool FindPath(const float* start, const float* goal, std::vector<float>& xyz, const char** reason = nullptr) const;
	// O(1) after projection: both ends on the mesh and the goal's part reachable from the start's part (directed). `goalDown` lets items lying on
	// crates (up to ~2.2 m above the floor) count as reachable.
	bool Reachable(const float* start, const float* goal, float goalDown = 2.6f, const NavQueryBounds* startBounds = nullptr) const;

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
	// FindPath + the links on the returned corners. `startBounds` is the optional water-aware start box; the core
	// accepts it only when the projected start polygon is the baked water area (kWaterArea). A water start with no
	// water ref (hole) or a non-water ref fails closed: it never falls back to the dry/seabed/stacked floor. The dry
	// start (no bounds) and the goal keep the existing dry behavior.
	bool FindPathLinks(const float* start, const float* goal, std::vector<float>& xyz, std::vector<NavPathLink>& links,
		const char** reason = nullptr, const NavQueryBounds* startBounds = nullptr) const;

private:
	unsigned long long Nearest(const float* p, float* snapped, float side, float up, float down) const;
	bool IsWaterPoly(unsigned long long ref) const;
	unsigned long long StartRef(const float* start, float* snapped, const NavQueryBounds* bounds, bool* notWater = nullptr) const;
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
