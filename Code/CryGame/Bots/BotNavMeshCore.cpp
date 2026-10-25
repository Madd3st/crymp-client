#include "CryGame/Bots/BotNavMeshCore.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"
#include "DetourCommon.h"
#include "DetourAlloc.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <string>

namespace WarsBots {

namespace {
// Crysis (x, y, z) <-> Recast (x, z, -y)
inline void ToRecast(const float* c, float* r) { r[0] = c[0]; r[1] = c[2]; r[2] = -c[1]; }
inline void ToCrysis(const float* r, float* c) { c[0] = r[0]; c[1] = -r[2]; c[2] = r[1]; }

struct MsetHeader { int magic; int version; int numTiles; dtNavMeshParams params; };
struct TileHeader { dtTileRef tileRef; int dataSize; };
const int kMsetMagic = 'M' << 24 | 'S' << 16 | 'E' << 8 | 'T';
}

NavMeshCore::NavMeshCore() {}
NavMeshCore::~NavMeshCore() { Reset(); }

void NavMeshCore::Reset()
{
	if (m_query) { dtFreeNavMeshQuery(m_query); m_query = nullptr; }
	if (m_mesh) { dtFreeNavMesh(m_mesh); m_mesh = nullptr; }
	delete m_filter; m_filter = nullptr;
	m_comp.clear(); m_components = 0; m_linkMask = 0; m_links = 0; m_reach.clear(); m_words = 0; m_linkFails.clear(); m_linkMismatch = 0;
}

bool NavMeshCore::LoadMemory(const unsigned char* data, size_t size, std::string* error)
{
	Reset();
	auto fail = [&](const char* why) { if (error) *error = why; Reset(); return false; };
	if (!data || size < sizeof(MsetHeader)) return fail("short file");
	MsetHeader h; std::memcpy(&h, data, sizeof h);
	if (h.magic != kMsetMagic || h.version != 1 || h.numTiles <= 0) return fail("not an MSET v1 navmesh");
	m_mesh = dtAllocNavMesh();
	if (!m_mesh || dtStatusFailed(m_mesh->init(&h.params))) return fail("navmesh init failed");
	size_t o = sizeof h;
	for (int i = 0; i < h.numTiles; ++i)
	{
		if (o + sizeof(TileHeader) > size) return fail("truncated tile header");
		TileHeader t; std::memcpy(&t, data + o, sizeof t); o += sizeof t;
		if (t.dataSize <= 0 || o + size_t(t.dataSize) > size) return fail("truncated tile data");
		unsigned char* tile = static_cast<unsigned char*>(dtAlloc(t.dataSize, DT_ALLOC_PERM));
		if (!tile) return fail("out of memory");
		std::memcpy(tile, data + o, t.dataSize); o += t.dataSize;
		if (dtStatusFailed(m_mesh->addTile(tile, t.dataSize, DT_TILE_FREE_DATA, 0, nullptr)))
		{ dtFree(tile); return fail("addTile failed (Detour version/layout mismatch?)"); }
	}
	m_query = dtAllocNavMeshQuery();
	if (!m_query || dtStatusFailed(m_query->init(m_mesh, 4096))) return fail("query init failed");
	m_filter = new dtQueryFilter();
	m_filter->setExcludeFlags(0);
	for (int ti = 0; ti < m_mesh->getMaxTiles(); ++ti)
	{
		const dtMeshTile* tile = static_cast<const dtNavMesh*>(m_mesh)->getTile(ti);
		if (tile && tile->header) m_links += tile->header->offMeshConCount;
	}
	SetLinkMask(0);    // walk only until an executor enables kinds
	return true;
}

bool NavMeshCore::LoadFile(const char* path, std::string* error)
{
	FILE* f = std::fopen(path, "rb");
	if (!f) { if (error) *error = "cannot open file"; return false; }
	std::vector<unsigned char> buf;
	unsigned char chunk[1 << 16]; size_t n;
	while ((n = std::fread(chunk, 1, sizeof chunk, f)) > 0) buf.insert(buf.end(), chunk, chunk + n);
	std::fclose(f);
	return LoadMemory(buf.data(), buf.size(), error);
}

void NavMeshCore::BuildComponents()
{
	// Directed reachability (O-026 P1 + c062): strongly connected parts (iterative Tarjan over passing links), then which
	// parts each part can reach through one-way links (drops). Reachable/FindPath answer "can I get there" exactly; a DFS
	// over outgoing links was seed-order dependent, weak parts were optimistic across drops (nav fallbacks 115 -> 689).
	const unsigned short include = m_filter ? m_filter->getIncludeFlags() : 0xffff;
	auto Pass = [include](const dtPoly* p) { return (p->flags & include) != 0; };
	m_comp.clear(); m_components = 0; m_reach.clear();
	const dtNavMesh* mesh = m_mesh;
	std::vector<dtPolyRef> refs;
	std::unordered_map<unsigned long long, int> index;
	for (int ti = 0; ti < mesh->getMaxTiles(); ++ti)
	{
		const dtMeshTile* tile = mesh->getTile(ti);
		if (!tile || !tile->header) continue;
		const dtPolyRef base = mesh->getPolyRefBase(tile);
		for (int i = 0; i < tile->header->polyCount; ++i)
			if (Pass(&tile->polys[i])) { index[base | dtPolyRef(i)] = int(refs.size()); refs.push_back(base | dtPolyRef(i)); }
	}
	const int n = int(refs.size());
	std::vector<std::vector<int>> adj(n);
	for (int i = 0; i < n; ++i)
	{
		const dtMeshTile* tile = nullptr; const dtPoly* poly = nullptr;
		if (dtStatusFailed(mesh->getTileAndPolyByRef(refs[i], &tile, &poly))) continue;
		for (unsigned int l = poly->firstLink; l != DT_NULL_LINK; l = tile->links[l].next)
		{
			auto it = index.find(tile->links[l].ref);
			if (it != index.end()) adj[i].push_back(it->second);
		}
	}
	// iterative Tarjan; SCCs come out in reverse topological order (sinks first)
	std::vector<int> num(n, -1), low(n, 0), comp(n, -1), st, edge(n, 0);
	std::vector<char> on(n, 0);
	int counter = 0;
	for (int s0 = 0; s0 < n; ++s0)
	{
		if (num[s0] >= 0) continue;
		std::vector<int> call(1, s0);
		num[s0] = low[s0] = counter++; st.push_back(s0); on[s0] = 1;
		while (!call.empty())
		{
			const int v = call.back();
			if (edge[v] < int(adj[v].size()))
			{
				const int w = adj[v][edge[v]++];
				if (num[w] < 0) { num[w] = low[w] = counter++; st.push_back(w); on[w] = 1; call.push_back(w); }
				else if (on[w] && num[w] < low[v]) low[v] = num[w];
				continue;
			}
			call.pop_back();
			if (!call.empty() && low[v] < low[call.back()]) low[call.back()] = low[v];
			if (low[v] == num[v])
			{
				const int id = m_components++;
				int w;
				do { w = st.back(); st.pop_back(); on[w] = 0; comp[w] = id; } while (w != v);
			}
		}
	}
	// reach[c] = parts reachable from part c (itself included); successors have smaller ids (already complete)
	const int words = (m_components + 63) / 64;
	m_reach.assign(size_t(m_components) * words, 0ull);
	std::vector<std::vector<int>> members(m_components);
	for (int i = 0; i < n; ++i) members[comp[i]].push_back(i);
	for (int c = 0; c < m_components; ++c)
	{
		unsigned long long* r = &m_reach[size_t(c) * words];
		r[c / 64] |= 1ull << (c % 64);
		for (int v : members[c]) for (int w : adj[v])
		{
			const int d = comp[w];
			if (d == c) continue;
			const unsigned long long* q = &m_reach[size_t(d) * words];
			for (int k = 0; k < words; ++k) r[k] |= q[k];
		}
	}
	m_words = words;
	for (int i = 0; i < n; ++i) m_comp[refs[i]] = comp[i];
}

bool NavMeshCore::PartReaches(int from, int to) const
{
	if (from < 0 || to < 0 || from >= m_components || to >= m_components) return false;
	return (m_reach[size_t(from) * m_words + to / 64] >> (to % 64)) & 1ull;
}

unsigned long long NavMeshCore::Nearest(const float* p, float* snapped, float side, float up, float down) const
{
	if (!m_query) return 0;
	float c[3]; ToRecast(p, c);
	// Detour searches a box around the centre: shift the centre so the box spans [p - down, p + up] vertically.
	const float centre[3] = {c[0], c[1] + (up - down) * 0.5f, c[2]};
	const float ext[3] = {side, (up + down) * 0.5f, side};
	dtPolyRef ref = 0; float nearest[3];
	if (dtStatusFailed(m_query->findNearestPoly(centre, ext, m_filter, &ref, nearest)) || !ref) return 0;
	if (snapped) ToCrysis(nearest, snapped);
	return ref;
}

bool NavMeshCore::Project(const float* p, float* out, float side, float up, float down) const
{
	return Nearest(p, out, side, up, down) != 0;
}

bool NavMeshCore::Reachable(const float* start, const float* goal, float goalDown) const
{
	const unsigned long long a = Nearest(start, nullptr, 1.5f, 1.0f, 3.0f);
	const unsigned long long b = Nearest(goal, nullptr, 1.5f, 1.0f, goalDown);
	if (!a || !b) return false;
	auto ia = m_comp.find(a), ib = m_comp.find(b);
	return ia != m_comp.end() && ib != m_comp.end() && PartReaches(ia->second, ib->second);
}

bool NavMeshCore::FindPath(const float* start, const float* goal, std::vector<float>& xyz, const char** reason) const
{
	std::vector<NavPathLink> links;
	return FindPathLinks(start, goal, xyz, links, reason);
}

int NavMeshCore::LoadLinkKinds(const char* text)
{
	// T-6 (DeepSeek review): the Detour user id is the 1-based line of links.txt, which silently breaks if a re-bake
	// reorders the file. Each link's kind is therefore matched by its END POINTS (within 0.6 m): the line from the user
	// id first, any line otherwise; links with no matching line stay unknown (never used).
	if (!m_mesh || !text) return 0;
	struct L { float a[3], b[3]; int kind; };
	std::vector<L> lines;
	std::istringstream in(text);
	std::string line;
	while (std::getline(in, line))
	{
		std::istringstream ls(line);
		L l; int bidir = 0; std::string kind;
		if (!(ls >> l.a[0] >> l.a[1] >> l.a[2] >> l.b[0] >> l.b[1] >> l.b[2] >> bidir >> kind)) { l.kind = -2; lines.push_back(l); continue; }
		l.kind = -1;
		for (int i = 0; i < NavLinkKinds; ++i) if (kind == NavLinkName(i)) l.kind = i;
		lines.push_back(l);
	}
	auto close = [](const float* p, const float* q) { return (p[0] - q[0]) * (p[0] - q[0]) + (p[1] - q[1]) * (p[1] - q[1]) + (p[2] - q[2]) * (p[2] - q[2]) < 0.36f; };
	int set = 0; m_linkMismatch = 0;
	for (int ti = 0; ti < m_mesh->getMaxTiles(); ++ti)
	{
		const dtMeshTile* tile = static_cast<const dtNavMesh*>(m_mesh)->getTile(ti);
		if (!tile || !tile->header) continue;
		const dtPolyRef base = m_mesh->getPolyRefBase(tile);
		for (int i = 0; i < tile->header->offMeshConCount; ++i)
		{
			const dtOffMeshConnection& con = tile->offMeshCons[i];
			float a[3], b[3]; ToCrysis(&con.pos[0], a); ToCrysis(&con.pos[3], b);
			int k = -1;
			const unsigned id = con.userId;
			if (id >= 1 && id <= lines.size() && lines[id - 1].kind > -2 && close(lines[id - 1].a, a) && close(lines[id - 1].b, b))
				k = lines[id - 1].kind;
			else
			{
				++m_linkMismatch;
				for (const L& l : lines) if (l.kind > -2 && close(l.a, a) && close(l.b, b)) { k = l.kind; break; }
			}
			// links without a known kind (Armada lift/ladder movers from the scene) get a flag no mask ever includes
			m_mesh->setPolyFlags(base | dtPolyRef(con.poly), k < 0 ? (unsigned short)0x4000 : NavLinkFlag(k));
			if (k >= 0) ++set;
		}
	}
	SetLinkMask(m_linkMask);
	return set;
}

void NavMeshCore::SetLinkMask(unsigned short mask)
{
	if (!m_filter) return;
	m_linkMask = mask;
	m_filter->setIncludeFlags((unsigned short)(1u | mask));
	BuildComponents();
}

void NavMeshCore::BanLink(unsigned long long ref)
{
	if (!m_mesh || !ref) return;
	m_mesh->setPolyFlags(dtPolyRef(ref), 0);
	BuildComponents();
}

bool NavMeshCore::FindPathLinks(const float* start, const float* goal, std::vector<float>& xyz, std::vector<NavPathLink>& links,
	const char** reason) const
{
	xyz.clear(); links.clear();
	auto fail = [&](const char* why) { if (reason) *reason = why; return false; };
	if (!m_query) return fail("navmesh_not_loaded");
	float s[3], g[3];
	const dtPolyRef a = Nearest(start, s, 1.5f, 1.0f, 3.0f);
	if (!a) return fail("start_off_mesh");
	const dtPolyRef b = Nearest(goal, g, 1.5f, 1.0f, 2.6f);
	if (!b) return fail("goal_off_mesh");
	auto ia = m_comp.find(a), ib = m_comp.find(b);
	if (ia == m_comp.end() || ib == m_comp.end() || !PartReaches(ia->second, ib->second)) return fail("other_component");
	float rs[3], rg[3]; ToRecast(s, rs); ToRecast(g, rg);
	dtPolyRef polys[256]; int npolys = 0;
	if (dtStatusFailed(m_query->findPath(a, b, rs, rg, m_filter, polys, &npolys, 256)) || npolys == 0) return fail("findpath_failed");
	float end[3]; dtVcopy(end, rg);
	if (polys[npolys - 1] != b && npolys < 256) return fail("no_directed_route"); // one-way drops: same part, no way back
	if (polys[npolys - 1] != b)
	{
		// Path buffer exhausted on a very long route: walk toward the closest point of the last polygon.
		bool over = false;
		m_query->closestPointOnPoly(polys[npolys - 1], rg, end, &over);
	}
	float corners[64 * 3]; unsigned char flags[64]; dtPolyRef cpolys[64]; int ncorners = 0;
	if (dtStatusFailed(m_query->findStraightPath(rs, end, polys, npolys, corners, flags, cpolys, &ncorners, 64)) || ncorners == 0)
		return fail("straightpath_failed");
	for (int i = 1; i < ncorners; ++i)    // corner 0 is the start
	{
		float c[3]; ToCrysis(&corners[i * 3], c);
		xyz.push_back(c[0]); xyz.push_back(c[1]); xyz.push_back(c[2]);
		if ((flags[i] & DT_STRAIGHTPATH_OFFMESH_CONNECTION) && i + 1 < ncorners)
		{
			NavPathLink L; L.corner = i - 1; L.ref = cpolys[i];
			const dtMeshTile* tile = nullptr; const dtPoly* poly = nullptr;
			unsigned short f = 2;
			if (dtStatusSucceed(m_mesh->getTileAndPolyByRef(cpolys[i], &tile, &poly))) f = poly->flags;
			L.kind = NavLinkDrop;
			for (int k = 0; k < NavLinkKinds; ++k) if (f == NavLinkFlag(k)) L.kind = k;
			L.a[0] = c[0]; L.a[1] = c[1]; L.a[2] = c[2];
			ToCrysis(&corners[(i + 1) * 3], L.b);
			links.push_back(L);
		}
	}
	if (xyz.empty()) { xyz.push_back(g[0]); xyz.push_back(g[1]); xyz.push_back(g[2]); }
	if (reason) *reason = "ok";
	return true;
}

} // namespace WarsBots
