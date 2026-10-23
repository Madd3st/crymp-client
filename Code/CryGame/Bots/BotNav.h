// WarsBots: shared navigation graph for the current level.
// Built in time slices from the level's physical geometry (static + terrain): a 3 m grid with
// several floor layers per column, edges verified walkable by rays. A* on demand. This is map
// knowledge every human player has; it never reveals enemies.
#pragma once
#include "CryCommon/CryMath/Cry_Math.h"
#include <cstdint>
#include <unordered_map>
#include <vector>

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

	// Path of world points from start to goal (excluding start). False if no route is known.
	bool FindPath(const Vec3& start, const Vec3& goal, std::vector<Vec3>& path) const;
	int NodeCount() const { return int(m_nodes.size()); }
	int EdgeCount() const { return int(m_edges.size()); }
	unsigned Queries() const { return m_queries; }

private:
	struct Node { Vec3 pos; int column; uint32_t firstEdge; uint16_t edgeCount; };
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
};
}
