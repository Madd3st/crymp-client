// WarsBots: shared navigation graph for the current level.
// Built in time slices from the level's physical geometry (static + terrain): a 3 m grid with
// several floor layers per column, edges verified walkable by rays. A* on demand. This is map
// knowledge every human player has; it never reveals enemies.
#pragma once
#include "CryCommon/CryMath/Cry_Math.h"
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <string>
#include "BotReachability.h"

namespace WarsBots
{
class BotNav final
{
public:
	enum class Phase { Idle, Nodes, Edges, Ready };

	void Reset();
	// Starts a build if the level has spawn points; safe to call every frame.
	void Begin();
	// Spends up to budgetMs building; returns true while work remains.
	bool Step(float budgetMs);
	bool Ready() const { return m_phase == Phase::Ready; }
	Phase GetPhase() const { return m_phase; }
	void Status() const;
    bool Export(const char* path=nullptr) const;

	// Path of world points from start to goal (excluding start). False if no route is known.
    struct PathTrace { const char* reason="not_ready"; int from=-1,to=-1,expanded=0; };
	bool FindPath(const Vec3& start, const Vec3& goal, std::vector<Vec3>& path, PathTrace* trace=nullptr) const;
	// --- Map memory ("learning the map"): per-node experience accumulated from play and kept on
	// disk per level (%USER%/WarsBots/<level>.mem). Stuck spots make edges expensive; kill/death
	// activity guides where bots look for fights. Nothing here observes enemies directly.
	void ReportStuck(const Vec3& pos, std::uint32_t actor);
	void ReportKill(const Vec3& pos);
	void ReportDeath(const Vec3& pos);
	// A roaming goal from experience: a fight hotspot (or a less-visited area to explore);
	// false if no memory yet. `seed` makes choices differ per bot.
	bool PickHotspot(unsigned seed, bool preferSafe, Vec3& out, const Vec3* from=nullptr, std::uint32_t actor=0) const;
    bool GoalFilterEnabled() const;
    int NodeAt(const Vec3& p) const { return Ready()?Nearest(p):-1; }
    int Component(int node) const { return m_reachability.Component(node); }
    // Unknown projection/cache is kept; only a proven graph disconnection is rejected.
    bool KnownUnreachable(int from, const Vec3& goal, int& to) const;
    bool InClosedComponent(const Vec3& p) const { return m_reachability.Closed(NodeAt(p)); }
	void Load(const char* level);
	void Save() const;
	void Tick(float dt);
	int NodeCount() const { return int(m_nodes.size()); }
	int EdgeCount() const { return int(m_edges.size()); }
	unsigned Queries() const { return m_queries; }

private:
	struct Node { Vec3 pos; int column; uint32_t firstEdge; uint16_t edgeCount; };
	struct Memory { float stuck = 0, kills = 0, deaths = 0; };
	struct Edge { int to; float cost; };
	int Nearest(const Vec3& pos) const;
	bool Walkable(const Vec3& a, const Vec3& b) const;
	void ScanColumn(int ix, int iy);
	int64_t Key(int ix, int iy) const { return (int64_t(ix) << 32) | uint32_t(iy); }

	Phase m_phase = Phase::Idle;
	float m_cell = 3.0f;
	Vec3 m_min = Vec3(0,0,0), m_max = Vec3(0,0,0);
	int m_nx = 0, m_ny = 0;
	int m_cursor = 0;             // column (Nodes) or node (Edges) being processed
	float m_buildMs = 0;
	std::vector<Node> m_nodes;
	std::vector<Edge> m_edges;
	std::vector<std::vector<Edge>> m_pending; // per-node edges during the Edges phase
	std::unordered_map<int64_t, std::vector<int>> m_columns;
	mutable unsigned m_queries = 0;
	std::vector<Memory> m_memory;
	std::string m_level;
	float m_saveTimer = 0;
	bool m_dirty = false;
    BotReachability m_reachability;
    bool m_componentsAttempted=false;
	unsigned m_events = 0;
};
}
