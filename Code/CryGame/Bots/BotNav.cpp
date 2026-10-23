#include "PortIncludes.h"
#include "CryGame/Bots/BotNav.h"
#include "CryGame/Game.h"
#include "CryGame/GameRules.h"
#include "CryCommon/Cry3DEngine/I3DEngine.h"
#include <algorithm>
#include <cmath>
#include <queue>

namespace WarsBots
{
namespace
{
const int kGeom = ent_static | ent_terrain;
const unsigned kFlags = rwi_stop_at_pierceable | rwi_colltype_any;
bool RayHit(const Vec3& from, const Vec3& delta, ray_hit& hit)
{
	return gEnv->pPhysicalWorld->RayWorldIntersection(from, delta, kGeom, kFlags, &hit, 1) != 0;
}
bool Clear(const Vec3& from, const Vec3& to)
{
	ray_hit hit;
	return !RayHit(from, to - from, hit);
}
}

void BotNav::Reset()
{
	m_phase = Phase::Idle;
	m_nodes.clear(); m_edges.clear(); m_pending.clear(); m_columns.clear();
	m_cursor = 0; m_buildMs = 0; m_queries = 0;
}

void BotNav::Begin()
{
	if (m_phase != Phase::Idle || !gEnv->pPhysicalWorld || !g_pGame) return;
	CGameRules* rules = g_pGame->GetGameRules();
	const int count = rules ? rules->GetSpawnLocationCount() : 0;
	if (!count) return;
	m_min = Vec3(1e9f, 1e9f, 1e9f); m_max = Vec3(-1e9f, -1e9f, -1e9f);
	for (int i = 0; i < count; ++i)
		if (IEntity* spawn = gEnv->pEntitySystem->GetEntity(rules->GetSpawnLocation(i)))
		{
			const Vec3 p = spawn->GetWorldPos();
			m_min.x = min(m_min.x, p.x); m_min.y = min(m_min.y, p.y); m_min.z = min(m_min.z, p.z);
			m_max.x = max(m_max.x, p.x); m_max.y = max(m_max.y, p.y); m_max.z = max(m_max.z, p.z);
		}
	if (m_min.x > m_max.x) return;
	// The fight happens around the spawns; a margin covers the space between and behind them.
	m_min -= Vec3(40.0f, 40.0f, 15.0f);
	m_max += Vec3(40.0f, 40.0f, 35.0f);
	m_nx = int((m_max.x - m_min.x) / m_cell) + 1;
	m_ny = int((m_max.y - m_min.y) / m_cell) + 1;
	m_nodes.reserve(size_t(m_nx) * size_t(m_ny));
	m_cursor = 0;
	m_phase = Phase::Nodes;
	CryLogAlways("[WarsBots] NAV build started: %dx%d columns, cell %.1f m, z %.0f..%.0f.", m_nx, m_ny, m_cell, m_min.z, m_max.z);
}

void BotNav::ScanColumn(int ix, int iy)
{
	const float x = m_min.x + (float(ix) + 0.5f) * m_cell;
	const float y = m_min.y + (float(iy) + 0.5f) * m_cell;
	float top = m_max.z;
	std::vector<int>& column = m_columns[Key(ix, iy)];
	// Several floors per column: roofs, building interiors, ground.
	for (int layer = 0; layer < 5 && top > m_min.z; ++layer)
	{
		ray_hit hit;
		if (!RayHit(Vec3(x, y, top), Vec3(0, 0, m_min.z - top), hit)) break;
		const Vec3 floor = hit.pt;
		top = floor.z - 0.6f;
		if (hit.n.z < 0.7f) continue;                                    // walls, steep slopes
		if (!Clear(floor + Vec3(0, 0, 0.15f), floor + Vec3(0, 0, 1.9f))) continue; // no headroom
		// Not standing under water: skip nodes whose feet are below the ocean level.
		if (gEnv->p3DEngine && floor.z < gEnv->p3DEngine->GetWaterLevel() - 0.3f) continue;
		Node node;
		node.pos = floor + Vec3(0, 0, 0.05f);
		node.column = int(Key(ix, iy) & 0x7fffffff);
		node.firstEdge = 0; node.edgeCount = 0;
		column.push_back(int(m_nodes.size()));
		m_nodes.push_back(node);
	}
}

bool BotNav::Walkable(const Vec3& a, const Vec3& b) const
{
	const float rise = b.z - a.z;
	if (rise > 1.9f || rise < -3.2f) return false;
	// Body clearance along the segment at knee, chest and head height.
	for (float h : { 0.5f, 1.2f, 1.75f })
		if (!Clear(a + Vec3(0, 0, h), b + Vec3(0, 0, h + max(0.0f, rise)))) return false;
	// Continuous ground: no holes, no single step taller than a stair when going up.
	float last = a.z;
	for (float t : { 0.25f, 0.5f, 0.75f, 1.0f })
	{
		const Vec3 p = a + (b - a) * t;
		ray_hit hit;
		if (!RayHit(Vec3(p.x, p.y, max(a.z, b.z) + 1.0f), Vec3(0, 0, -(fabs_tpl(rise) + 4.5f)), hit)) return false;
		const float step = hit.pt.z - last;
		if (step > 0.75f) return false;              // a wall or ledge too tall to walk up
		if (step < -3.2f) return false;              // a hole in the middle of the edge
		last = hit.pt.z;
	}
	return fabs_tpl(last - b.z) < 0.8f;
}

bool BotNav::Step(float budgetMs)
{
	if (m_phase != Phase::Nodes && m_phase != Phase::Edges) return false;
	const CTimeValue start = gEnv->pTimer->GetAsyncTime();
	auto elapsed = [&]() { return (gEnv->pTimer->GetAsyncTime() - start).GetMilliSeconds(); };
	if (m_phase == Phase::Nodes)
	{
		const int total = m_nx * m_ny;
		while (m_cursor < total && elapsed() < budgetMs)
		{
			ScanColumn(m_cursor % m_nx, m_cursor / m_nx);
			++m_cursor;
		}
		if (m_cursor >= total)
		{
			m_phase = Phase::Edges;
			m_cursor = 0;
			m_pending.assign(m_nodes.size(), {});
			CryLogAlways("[WarsBots] NAV nodes: %d.", int(m_nodes.size()));
		}
	}
	else
	{
		static const int dirs[8][2] = { {1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1} };
		while (m_cursor < int(m_nodes.size()) && elapsed() < budgetMs)
		{
			const Node& node = m_nodes[m_cursor];
			const int ix = int((node.pos.x - m_min.x) / m_cell), iy = int((node.pos.y - m_min.y) / m_cell);
			for (const auto& d : dirs)
			{
				auto it = m_columns.find(Key(ix + d[0], iy + d[1]));
				if (it == m_columns.end()) continue;
				for (int other : it->second)
					if (Walkable(node.pos, m_nodes[other].pos))
						m_pending[m_cursor].push_back({ other, (m_nodes[other].pos - node.pos).GetLength() });
			}
			++m_cursor;
		}
		if (m_cursor >= int(m_nodes.size()))
		{
			for (size_t i = 0; i < m_nodes.size(); ++i)
			{
				m_nodes[i].firstEdge = uint32_t(m_edges.size());
				m_nodes[i].edgeCount = uint16_t(std::min<size_t>(m_pending[i].size(), 0xffff));
				m_edges.insert(m_edges.end(), m_pending[i].begin(), m_pending[i].begin() + m_nodes[i].edgeCount);
			}
			m_pending.clear();
			m_pending.shrink_to_fit();
			m_phase = Phase::Ready;
			CryLogAlways("[WarsBots] NAV ready: %d nodes, %d edges, build %.0f ms of frame budget.",
				int(m_nodes.size()), int(m_edges.size()), m_buildMs + elapsed());
		}
	}
	m_buildMs += elapsed();
	return m_phase != Phase::Ready;
}

void BotNav::Status() const
{
	static const char* names[] = { "idle", "nodes", "edges", "ready" };
	CryLogAlways("[WarsBots] NAV phase=%s nodes=%d edges=%d buildMs=%.0f queries=%u.",
		names[int(m_phase)], NodeCount(), EdgeCount(), m_buildMs, m_queries);
}

int BotNav::Nearest(const Vec3& pos) const
{
	const int cx = int((pos.x - m_min.x) / m_cell), cy = int((pos.y - m_min.y) / m_cell);
	int best = -1;
	float bestScore = 1e9f;
	for (int dx = -2; dx <= 2; ++dx)
		for (int dy = -2; dy <= 2; ++dy)
		{
			auto it = m_columns.find(Key(cx + dx, cy + dy));
			if (it == m_columns.end()) continue;
			for (int id : it->second)
			{
				const Vec3& p = m_nodes[id].pos;
				const float dz = fabs_tpl(p.z - pos.z);
				if (dz > 2.5f) continue;                 // a different floor
				const float score = (p - pos).GetLengthSquared2D() + dz * dz * 4.0f;
				if (score < bestScore && Clear(pos + Vec3(0, 0, 0.9f), p + Vec3(0, 0, 0.9f)))
				{ bestScore = score; best = id; }
			}
		}
	return best;
}

bool BotNav::FindPath(const Vec3& start, const Vec3& goal, std::vector<Vec3>& path) const
{
	path.clear();
	if (m_phase != Phase::Ready) return false;
	++m_queries;
	const int from = Nearest(start), to = Nearest(goal);
	if (from < 0 || to < 0) return false;
	if (from == to) { path.push_back(goal); return true; }
	// A* with a node budget; graph is small (thousands of nodes) so allocations per query are fine.
	const size_t n = m_nodes.size();
	std::vector<float> g(n, 1e30f);
	std::vector<int> parent(n, -1);
	std::vector<uint8_t> closed(n, 0);
	typedef std::pair<float, int> Item;
	std::priority_queue<Item, std::vector<Item>, std::greater<Item>> open;
	g[from] = 0;
	open.push({ (m_nodes[to].pos - m_nodes[from].pos).GetLength(), from });
	int expanded = 0;
	while (!open.empty() && expanded < 6000)
	{
		const int cur = open.top().second;
		open.pop();
		if (closed[cur]) continue;
		closed[cur] = 1;
		++expanded;
		if (cur == to) break;
		const Node& node = m_nodes[cur];
		for (uint32_t e = node.firstEdge; e < node.firstEdge + node.edgeCount; ++e)
		{
			const Edge& edge = m_edges[e];
			const float cost = g[cur] + edge.cost;
			if (cost < g[edge.to])
			{
				g[edge.to] = cost;
				parent[edge.to] = cur;
				open.push({ cost + (m_nodes[to].pos - m_nodes[edge.to].pos).GetLength(), edge.to });
			}
		}
	}
	if (parent[to] < 0) return false;
	for (int cur = to; cur != from; cur = parent[cur]) path.push_back(m_nodes[cur].pos);
	std::reverse(path.begin(), path.end());
	path.push_back(goal);
	return true;
}
}
