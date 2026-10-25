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
	m_comp.clear(); m_components = 0; m_linkMask = 0; m_links = 0;
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
	// Weakly connected parts (union-find over every passing link, both directions). A DFS over outgoing links made
	// one-way off-mesh drops seed-order dependent (O-026 P1). One-way reachability is checked by FindPath
	// ("no_directed_route"), so Reachable is optimistic only across drops.
	const unsigned short include = m_filter ? m_filter->getIncludeFlags() : 0xffff;
	auto Pass = [include](const dtPoly* p) { return (p->flags & include) != 0; };
	m_comp.clear(); m_components = 0;
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
	std::vector<int> parent(refs.size());
	for (size_t i = 0; i < parent.size(); ++i) parent[i] = int(i);
	auto find = [&](int x) { while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; } return x; };
	for (size_t i = 0; i < refs.size(); ++i)
	{
		const dtMeshTile* tile = nullptr; const dtPoly* poly = nullptr;
		if (dtStatusFailed(mesh->getTileAndPolyByRef(refs[i], &tile, &poly))) continue;
		for (unsigned int l = poly->firstLink; l != DT_NULL_LINK; l = tile->links[l].next)
		{
			auto it = index.find(tile->links[l].ref);
			if (it == index.end()) continue;
			const int x = find(int(i)), y = find(it->second);
			if (x != y) parent[x] = y;
		}
	}
	std::unordered_map<int, int> ids;
	for (size_t i = 0; i < refs.size(); ++i)
	{
		const int root = find(int(i));
		auto it = ids.find(root);
		if (it == ids.end()) it = ids.emplace(root, m_components++).first;
		m_comp[refs[i]] = it->second;
	}
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
	return ia != m_comp.end() && ib != m_comp.end() && ia->second == ib->second;
}

bool NavMeshCore::FindPath(const float* start, const float* goal, std::vector<float>& xyz, const char** reason) const
{
	std::vector<NavPathLink> links;
	return FindPathLinks(start, goal, xyz, links, reason);
}

int NavMeshCore::LoadLinkKinds(const char* text)
{
	if (!m_mesh || !text) return 0;
	std::vector<int> kinds;            // by 1-based line
	std::istringstream in(text);
	std::string line;
	while (std::getline(in, line))
	{
		std::istringstream ls(line);
		float v[6]; int bidir = 0; std::string kind;
		if (!(ls >> v[0] >> v[1] >> v[2] >> v[3] >> v[4] >> v[5] >> bidir >> kind)) { kinds.push_back(-1); continue; }
		int k = -1;
		for (int i = 0; i < NavLinkKinds; ++i) if (kind == NavLinkName(i)) k = i;
		kinds.push_back(k);
	}
	int set = 0;
	for (int ti = 0; ti < m_mesh->getMaxTiles(); ++ti)
	{
		const dtMeshTile* tile = static_cast<const dtNavMesh*>(m_mesh)->getTile(ti);
		if (!tile || !tile->header) continue;
		const dtPolyRef base = m_mesh->getPolyRefBase(tile);
		for (int i = 0; i < tile->header->offMeshConCount; ++i)
		{
			const dtOffMeshConnection& con = tile->offMeshCons[i];
			const unsigned id = con.userId;
			const int k = (id >= 1 && id <= kinds.size()) ? kinds[id - 1] : -1;
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
	if (ia == m_comp.end() || ib == m_comp.end() || ia->second != ib->second) return fail("other_component");
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
