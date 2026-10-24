// recast_bake — offline Recast/Detour bake of a WarsBots collision export (OC-022, done by Claude under D-010).
// Not game code: reads the cropped OBJ(.gz) from Codex C-019/C-028 (Crysis frame, Z up), bakes a tiled
// Detour navmesh with the player-capsule parameters, and reports which spawns / items sit on the main
// connected part. Optional off-mesh links (jumps) from a text file. Output: JSON on stdout, polygon mesh as
// OBJ (Crysis frame) and an optional binary .navmesh (Detour tiles) for later in-game loading experiments.
//
// Frame: Crysis (x, y, z) -> Recast (x, z, -y). This is a rotation, so triangle winding (normals up) is kept.
//
// Usage: recast_bake --obj map.crop.obj.gz [--points pts.txt] [--links links.txt] [--out-obj mesh.obj]
//                    [--out-bin map.navmesh] [--zmin z] [--zmax z] [--cs 0.2] [--ch 0.1] [--radius 0.4]
//                    [--height 1.8] [--climb 0.5] [--slope 45] [--tile 128] [--min-region 2] [--monotone]
// pts.txt: lines "kind id x y z" (kind = spawn | item | probe), Crysis frame.
// links.txt: lines "x0 y0 z0 x1 y1 z1 bidir kind" (Crysis frame), kind is a free label (jump, strength...).
#include "Recast.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshBuilder.h"
#include "DetourNavMeshQuery.h"
#include "DetourCommon.h"
#include <zlib.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <numeric>

struct Pt { std::string kind; int id; float p[3]; };     // p in Recast frame
struct Link { float a[3], b[3]; int bidir; std::string kind; };

static bool ReadAll(const std::string& path, std::string& out)
{
	gzFile g = gzopen(path.c_str(), "rb");
	if (!g) return false;
	char buf[1 << 16]; int n;
	while ((n = gzread(g, buf, sizeof buf)) > 0) out.append(buf, n);
	gzclose(g);
	return true;
}

static void ToRecast(float x, float y, float z, float* o) { o[0] = x; o[1] = z; o[2] = -y; }
static void ToCrysis(const float* r, float* o) { o[0] = r[0]; o[1] = -r[2]; o[2] = r[1]; }

static bool LoadObj(const std::string& path, float zmin, float zmax, std::vector<float>& verts, std::vector<int>& tris,
	int& dropped)
{
	std::string s;
	if (!ReadAll(path, s)) return false;
	std::vector<float> v;  // crysis frame
	const char* p = s.c_str(); const char* end = p + s.size();
	std::vector<int> idx;
	dropped = 0;
	while (p < end)
	{
		const char* nl = (const char*)memchr(p, '\n', end - p); if (!nl) nl = end;
		if (p[0] == 'v' && p[1] == ' ')
		{
			char* q; float x = strtof(p + 2, &q); float y = strtof(q, &q); float z = strtof(q, &q);
			v.push_back(x); v.push_back(y); v.push_back(z);
		}
		else if (p[0] == 'f' && p[1] == ' ')
		{
			idx.clear(); const char* q = p + 2;
			while (q < nl)
			{
				while (q < nl && (*q == ' ' || *q == '\t' || *q == '\r')) ++q;
				if (q >= nl) break;
				char* e; long i = strtol(q, &e, 10); if (e == q) break;
				idx.push_back(i > 0 ? int(i - 1) : int(v.size() / 3 + i));
				q = e; while (q < nl && *q != ' ' && *q != '\t') ++q;
			}
			for (size_t k = 1; k + 1 < idx.size(); ++k)
			{
				const int a = idx[0], b = idx[k], c = idx[k + 1];
				const float za = v[a * 3 + 2], zb = v[b * 3 + 2], zc = v[c * 3 + 2];
				const float lo = std::min(za, std::min(zb, zc)), hi = std::max(za, std::max(zb, zc));
				if (lo > zmax || hi < zmin) { ++dropped; continue; }
				tris.push_back(a); tris.push_back(b); tris.push_back(c);
			}
		}
		p = nl + 1;
	}
	verts.resize(v.size());
	for (size_t i = 0; i < v.size() / 3; ++i) ToRecast(v[i * 3], v[i * 3 + 1], v[i * 3 + 2], &verts[i * 3]);
	return true;
}

struct Cfg {
	float cs = 0.2f, ch = 0.1f, radius = 0.4f, height = 1.8f, climb = 0.5f, slope = 45.f;
	int tile = 128, minRegion = 2, mergeRegion = 20; bool monotone = false;
	int motions = 3;      // generated link motions: 1 drop, 2 +jump, 3 +Strength
	float vh = 4.5f;      // horizontal take-off speed for generated links (run 4.5, sprint 6.75 m/s)
};

// Off-mesh links for one tile: those whose start lies in the tile's xz box.
static unsigned char* BuildTile(rcContext& ctx, const Cfg& c, const std::vector<float>& verts, const std::vector<int>& tris,
	const std::vector<int>& triIdx, const float* bmin, const float* bmax, int tx, int ty,
	const std::vector<Link>& links, int& dataSize, int& polys)
{
	rcConfig cfg; memset(&cfg, 0, sizeof cfg);
	cfg.cs = c.cs; cfg.ch = c.ch;
	cfg.walkableSlopeAngle = c.slope;
	cfg.walkableHeight = (int)ceilf(c.height / c.ch);
	cfg.walkableClimb = (int)floorf(c.climb / c.ch);
	cfg.walkableRadius = (int)ceilf(c.radius / c.cs);
	cfg.maxEdgeLen = (int)(12.f / c.cs);
	cfg.maxSimplificationError = 1.3f;
	cfg.minRegionArea = c.minRegion * c.minRegion;
	cfg.mergeRegionArea = c.mergeRegion * c.mergeRegion;
	cfg.maxVertsPerPoly = 6;
	cfg.tileSize = c.tile;
	cfg.borderSize = cfg.walkableRadius + 3;
	cfg.width = cfg.tileSize + cfg.borderSize * 2;
	cfg.height = cfg.tileSize + cfg.borderSize * 2;
	cfg.detailSampleDist = c.cs * 6.f;
	cfg.detailSampleMaxError = c.ch * 1.f;
	const float tw = c.tile * c.cs;
	cfg.bmin[0] = bmin[0] + tx * tw; cfg.bmin[1] = bmin[1]; cfg.bmin[2] = bmin[2] + ty * tw;
	cfg.bmax[0] = bmin[0] + (tx + 1) * tw; cfg.bmax[1] = bmax[1]; cfg.bmax[2] = bmin[2] + (ty + 1) * tw;
	cfg.bmin[0] -= cfg.borderSize * cfg.cs; cfg.bmin[2] -= cfg.borderSize * cfg.cs;
	cfg.bmax[0] += cfg.borderSize * cfg.cs; cfg.bmax[2] += cfg.borderSize * cfg.cs;

	dataSize = 0; polys = 0;
	if (triIdx.empty()) return nullptr;
	rcHeightfield* hf = rcAllocHeightfield();
	if (!rcCreateHeightfield(&ctx, *hf, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch)) return nullptr;
	std::vector<int> t; t.reserve(triIdx.size() * 3);
	for (int i : triIdx) { t.push_back(tris[i * 3]); t.push_back(tris[i * 3 + 1]); t.push_back(tris[i * 3 + 2]); }
	const int nt = (int)triIdx.size();
	std::vector<unsigned char> areas(nt, 0);
	rcMarkWalkableTriangles(&ctx, cfg.walkableSlopeAngle, verts.data(), (int)verts.size() / 3, t.data(), nt, areas.data());
	rcRasterizeTriangles(&ctx, verts.data(), (int)verts.size() / 3, t.data(), areas.data(), nt, *hf, cfg.walkableClimb);
	rcFilterLowHangingWalkableObstacles(&ctx, cfg.walkableClimb, *hf);
	rcFilterLedgeSpans(&ctx, cfg.walkableHeight, cfg.walkableClimb, *hf);
	rcFilterWalkableLowHeightSpans(&ctx, cfg.walkableHeight, *hf);
	rcCompactHeightfield* chf = rcAllocCompactHeightfield();
	const bool okc = rcBuildCompactHeightfield(&ctx, cfg.walkableHeight, cfg.walkableClimb, *hf, *chf);
	rcFreeHeightField(hf);
	if (!okc) { rcFreeCompactHeightfield(chf); return nullptr; }
	rcErodeWalkableArea(&ctx, cfg.walkableRadius, *chf);
	bool okr;
	if (c.monotone) okr = rcBuildRegionsMonotone(&ctx, *chf, cfg.borderSize, cfg.minRegionArea, cfg.mergeRegionArea);
	else okr = rcBuildDistanceField(&ctx, *chf) && rcBuildRegions(&ctx, *chf, cfg.borderSize, cfg.minRegionArea, cfg.mergeRegionArea);
	if (!okr) { rcFreeCompactHeightfield(chf); return nullptr; }
	rcContourSet* cset = rcAllocContourSet();
	if (!rcBuildContours(&ctx, *chf, cfg.maxSimplificationError, cfg.maxEdgeLen, *cset) || cset->nconts == 0)
	{ rcFreeContourSet(cset); rcFreeCompactHeightfield(chf); return nullptr; }
	rcPolyMesh* pm = rcAllocPolyMesh();
	rcPolyMeshDetail* dm = rcAllocPolyMeshDetail();
	const bool okp = rcBuildPolyMesh(&ctx, *cset, cfg.maxVertsPerPoly, *pm) &&
		rcBuildPolyMeshDetail(&ctx, *pm, *chf, cfg.detailSampleDist, cfg.detailSampleMaxError, *dm);
	rcFreeContourSet(cset); rcFreeCompactHeightfield(chf);
	if (!okp || pm->npolys == 0) { rcFreePolyMesh(pm); rcFreePolyMeshDetail(dm); return nullptr; }
	for (int i = 0; i < pm->npolys; ++i) { if (pm->areas[i] == RC_WALKABLE_AREA) pm->flags[i] = 1; }

	// off-mesh links starting in this tile (xz without border)
	std::vector<float> omv; std::vector<float> omr; std::vector<unsigned char> omdir, omarea; std::vector<unsigned short> omflags;
	std::vector<unsigned int> omid;
	const float x0 = bmin[0] + tx * tw, x1 = x0 + tw, z0 = bmin[2] + ty * tw, z1 = z0 + tw;
	for (size_t i = 0; i < links.size(); ++i)
	{
		const Link& L = links[i];
		if (L.a[0] < x0 || L.a[0] >= x1 || L.a[2] < z0 || L.a[2] >= z1) continue;
		for (int k = 0; k < 3; ++k) omv.push_back(L.a[k]);
		for (int k = 0; k < 3; ++k) omv.push_back(L.b[k]);
		omr.push_back(c.radius); omdir.push_back(L.bidir ? DT_OFFMESH_CON_BIDIR : 0);
		omarea.push_back(RC_WALKABLE_AREA); omflags.push_back(2); omid.push_back((unsigned)(i + 1));
	}

	dtNavMeshCreateParams p; memset(&p, 0, sizeof p);
	p.verts = pm->verts; p.vertCount = pm->nverts; p.polys = pm->polys; p.polyAreas = pm->areas; p.polyFlags = pm->flags;
	p.polyCount = pm->npolys; p.nvp = pm->nvp;
	p.detailMeshes = dm->meshes; p.detailVerts = dm->verts; p.detailVertsCount = dm->nverts;
	p.detailTris = dm->tris; p.detailTriCount = dm->ntris;
	p.offMeshConVerts = omv.empty() ? nullptr : omv.data(); p.offMeshConRad = omr.empty() ? nullptr : omr.data();
	p.offMeshConDir = omdir.empty() ? nullptr : omdir.data(); p.offMeshConAreas = omarea.empty() ? nullptr : omarea.data();
	p.offMeshConFlags = omflags.empty() ? nullptr : omflags.data(); p.offMeshConUserID = omid.empty() ? nullptr : omid.data();
	p.offMeshConCount = (int)omr.size();
	p.walkableHeight = c.height; p.walkableRadius = c.radius; p.walkableClimb = c.climb;
	p.tileX = tx; p.tileY = ty; p.tileLayer = 0;
	rcVcopy(p.bmin, pm->bmin); rcVcopy(p.bmax, pm->bmax);
	p.cs = cfg.cs; p.ch = cfg.ch; p.buildBvTree = true;
	unsigned char* data = nullptr;
	polys = pm->npolys;
	if (!dtCreateNavMeshData(&p, &data, &dataSize)) { data = nullptr; dataSize = 0; }
	rcFreePolyMesh(pm); rcFreePolyMeshDetail(dm);
	return data;
}


// ---------------------------------------------------------------------------------------------------------------
// Jump / drop link generation on the baked mesh. Take-off samples every 0.8 m along open polygon edges (edges with
// no neighbour), three headings (outward normal, ±35°), three motions: drop (v0 0), jump (v0 4.43 m/s, apex 1.0 m)
// and Strength jump (v0 9.60 m/s, apex 4.7 m) — PlayerMovement.cpp:1134,1150-1170 — with horizontal speed vh.
// The arc is integrated at 50 Hz; the capsule (rays at +0.3 / +0.9 / +1.5 m) must not hit collision geometry;
// landing = first descending step that is within 0.2 m above a polygon of another ground component, fall ≤ 4 m.
// One link per (source component, target component, motion) is kept: the cheapest motion that works.
struct TriGrid {
	float cell = 2.f, ox = 0, oz = 0; int nx = 0, nz = 0;
	std::vector<std::vector<int>> cells; const std::vector<float>* v = nullptr; const std::vector<int>* t = nullptr;
	std::vector<int> stamp; int cur = 0;
	void Build(const std::vector<float>& V, const std::vector<int>& T, const float* bmin, const float* bmax)
	{
		v = &V; t = &T; ox = bmin[0]; oz = bmin[2];
		nx = (int)((bmax[0] - bmin[0]) / cell) + 1; nz = (int)((bmax[2] - bmin[2]) / cell) + 1;
		cells.assign((size_t)nx * nz, {}); stamp.assign(T.size() / 3, 0);
		for (int i = 0; i < (int)T.size() / 3; ++i)
		{
			float lo[2] = {1e30f, 1e30f}, hi[2] = {-1e30f, -1e30f};
			for (int k = 0; k < 3; ++k) { const float* p = &V[T[i * 3 + k] * 3];
				lo[0] = std::min(lo[0], p[0]); hi[0] = std::max(hi[0], p[0]); lo[1] = std::min(lo[1], p[2]); hi[1] = std::max(hi[1], p[2]); }
			const int ax = std::max(0, (int)((lo[0] - ox) / cell)), bx = std::min(nx - 1, (int)((hi[0] - ox) / cell));
			const int az = std::max(0, (int)((lo[1] - oz) / cell)), bz = std::min(nz - 1, (int)((hi[1] - oz) / cell));
			for (int z = az; z <= bz; ++z) for (int x = ax; x <= bx; ++x) cells[(size_t)z * nx + x].push_back(i);
		}
	}
	static bool SegTri(const float* o, const float* d, const float* a, const float* b, const float* c)
	{
		float e1[3], e2[3], pv[3], tv[3], qv[3];
		dtVsub(e1, b, a); dtVsub(e2, c, a); dtVcross(pv, d, e2);
		const float det = dtVdot(e1, pv);
		if (fabsf(det) < 1e-9f) return false;
		const float inv = 1.f / det; dtVsub(tv, o, a);
		const float u = dtVdot(tv, pv) * inv; if (u < 0.f || u > 1.f) return false;
		dtVcross(qv, tv, e1);
		const float w = dtVdot(d, qv) * inv; if (w < 0.f || u + w > 1.f) return false;
		const float tt = dtVdot(e2, qv) * inv;
		return tt >= 0.f && tt <= 1.f;
	}
	bool Hit(const float* p0, const float* p1)
	{
		++cur; float d[3]; dtVsub(d, p1, p0);
		const int ax = std::max(0, (int)((std::min(p0[0], p1[0]) - ox) / cell)), bx = std::min(nx - 1, (int)((std::max(p0[0], p1[0]) - ox) / cell));
		const int az = std::max(0, (int)((std::min(p0[2], p1[2]) - oz) / cell)), bz = std::min(nz - 1, (int)((std::max(p0[2], p1[2]) - oz) / cell));
		for (int z = az; z <= bz; ++z) for (int x = ax; x <= bx; ++x)
			for (int i : cells[(size_t)z * nx + x])
			{
				if (stamp[i] == cur) continue; stamp[i] = cur;
				const std::vector<float>& V = *v; const std::vector<int>& T = *t;
				if (SegTri(p0, d, &V[T[i * 3] * 3], &V[T[i * 3 + 1] * 3], &V[T[i * 3 + 2] * 3])) return true;
			}
		return false;
	}
};

static std::vector<int> GroundComponents(const dtNavMesh* nav, std::map<dtPolyRef, int>& index, std::vector<dtPolyRef>& refs)
{
	index.clear(); refs.clear();
	for (int ti = 0; ti < nav->getMaxTiles(); ++ti)
	{
		const dtMeshTile* t = nav->getTile(ti); if (!t || !t->header) continue;
		const dtPolyRef base = nav->getPolyRefBase(t);
		for (int i = 0; i < t->header->polyCount; ++i) { index[base | (dtPolyRef)i] = (int)refs.size(); refs.push_back(base | (dtPolyRef)i); }
	}
	std::vector<int> comp(refs.size(), -1); int nc = 0;
	for (size_t s = 0; s < refs.size(); ++s)
	{
		if (comp[s] >= 0) continue;
		std::vector<int> st{(int)s}; comp[s] = nc;
		while (!st.empty())
		{
			int u = st.back(); st.pop_back();
			const dtMeshTile* t; const dtPoly* p; nav->getTileAndPolyByRefUnsafe(refs[u], &t, &p);
			for (unsigned int l = p->firstLink; l != DT_NULL_LINK; l = t->links[l].next)
			{ auto it = index.find(t->links[l].ref); if (it != index.end() && comp[it->second] < 0) { comp[it->second] = nc; st.push_back(it->second); } }
		}
		++nc;
	}
	return comp;
}

static std::vector<Link> GenerateJumpLinks(const dtNavMesh* nav, const std::vector<float>& verts, const std::vector<int>& tris,
	const Cfg& c, int* stats)
{
	std::map<dtPolyRef, int> index; std::vector<dtPolyRef> refs;
	std::vector<int> comp = GroundComponents(nav, index, refs);
	float bmin[3] = {1e30f, 1e30f, 1e30f}, bmax[3] = {-1e30f, -1e30f, -1e30f};
	for (size_t i = 0; i < verts.size() / 3; ++i) for (int k = 0; k < 3; ++k) { bmin[k] = std::min(bmin[k], verts[i * 3 + k]); bmax[k] = std::max(bmax[k], verts[i * 3 + k]); }
	TriGrid grid; grid.Build(verts, tris, bmin, bmax);
	dtNavMeshQuery* q = dtAllocNavMeshQuery(); q->init(nav, 2048);
	dtQueryFilter filter;
	struct Motion { const char* name; float v0; } motions[3] = {{"drop", 0.f}, {"jump", 4.43f}, {"strength", 9.60f}};
	std::vector<int> motionOf;
	std::map<long long, size_t> where;
	std::vector<Link> out;
	const float g = 9.81f, maxFall = 4.f, dt = 0.02f;
	for (size_t n = 0; n < refs.size(); ++n)
	{
		const dtMeshTile* t; const dtPoly* p; nav->getTileAndPolyByRefUnsafe(refs[n], &t, &p);
		if (p->getType() != DT_POLYTYPE_GROUND) continue;
		float cen[3] = {0, 0, 0};
		for (int k = 0; k < p->vertCount; ++k) dtVadd(cen, cen, &t->verts[p->verts[k] * 3]);
		dtVscale(cen, cen, 1.f / p->vertCount);
		for (int e = 0; e < p->vertCount; ++e)
		{
			if (p->neis[e] != 0) continue;   // internal or tile-portal edge
			const float* va = &t->verts[p->verts[e] * 3]; const float* vb = &t->verts[p->verts[(e + 1) % p->vertCount] * 3];
			const float ex = vb[0] - va[0], ez = vb[2] - va[2], len = sqrtf(ex * ex + ez * ez);
			if (len < 0.3f) continue;
			float nxo = ez / len, nzo = -ex / len;
			const float mx = (va[0] + vb[0]) * 0.5f - cen[0], mz = (va[2] + vb[2]) * 0.5f - cen[2];
			if (nxo * mx + nzo * mz < 0) { nxo = -nxo; nzo = -nzo; }
			const int ns = std::max(1, (int)(len / 0.8f));
			for (int si = 0; si < ns; ++si)
			{
				const float f = (si + 0.5f) / ns;
				float s0[3] = {va[0] + ex * f - nxo * 0.1f, va[1] + (vb[1] - va[1]) * f, va[2] + ez * f - nzo * 0.1f};
				for (int hd = -1; hd <= 1; ++hd)
				{
					const float ang = hd * 35.f * 3.14159265f / 180.f, ca = cosf(ang), sa = sinf(ang);
					const float dx = nxo * ca - nzo * sa, dz = nxo * sa + nzo * ca;
					for (int mi = 0; mi < c.motions; ++mi)
					{
						const float v0 = motions[mi].v0;
						float prev[3]; rcVcopy(prev, s0);
						for (float tt = dt; tt < 3.f; tt += dt)
						{
							float cur[3] = {s0[0] + dx * c.vh * tt, s0[1] + v0 * tt - 0.5f * g * tt * tt, s0[2] + dz * c.vh * tt};
							if (cur[1] < s0[1] - maxFall - 0.5f) break;
							bool blocked = false;
							for (float hgt : {0.3f, 0.9f, 1.5f})
							{ float a[3] = {prev[0], prev[1] + hgt, prev[2]}, b[3] = {cur[0], cur[1] + hgt, cur[2]}; if (grid.Hit(a, b)) { blocked = true; break; } }
							if (blocked) break;
							const float vy = v0 - g * tt;
							if (vy < 0.f && tt > 0.1f)
							{
								const float ext[3] = {0.25f, 0.35f, 0.25f}; const float qc[3] = {cur[0], cur[1] - 0.15f, cur[2]};
								dtPolyRef r = 0; float nearest[3];
								q->findNearestPoly(qc, ext, &filter, &r, nearest);
								if (r && cur[1] - nearest[1] <= 0.2f && cur[1] - nearest[1] >= -0.15f)
								{
									const int dn = index[r];
									if (comp[dn] != comp[n] && nearest[1] >= s0[1] - maxFall)
									{
										const long long key = ((long long)comp[n] << 32) | (unsigned)comp[dn];
										auto w = where.find(key);
										Link L; rcVcopy(L.a, s0); rcVcopy(L.b, nearest); L.bidir = 0; L.kind = motions[mi].name;
										if (w == where.end()) { where[key] = out.size(); out.push_back(L); stats[mi]++; }
										else if (mi < motionOf[w->second]) { stats[motionOf[w->second]]--; stats[mi]++; out[w->second] = L; }
										if (motionOf.size() < out.size()) motionOf.resize(out.size(), 9);
										if (w == where.end() || mi < motionOf[where[key]]) motionOf[where[key]] = mi;
									}
									break;   // landed (same component or not)
								}
							}
							rcVcopy(prev, cur);
						}
					}
				}
			}
		}
	}
	dtFreeNavMeshQuery(q);
	return out;
}

static float PolyArea(const dtMeshTile* t, const dtPoly* poly)
{
	float a = 0.f;
	const float* v0 = &t->verts[poly->verts[0] * 3];
	for (int i = 2; i < poly->vertCount; ++i)
		a += dtTriArea2D(v0, &t->verts[poly->verts[i - 1] * 3], &t->verts[poly->verts[i] * 3]);
	return fabsf(a);
}

int main(int argc, char** argv)
{
	std::string obj, ptsPath, linksPath, outObj, outBin, linksOut; bool genJumps = false;
	float zmin = -1e9f, zmax = 1e9f;
	Cfg c;
	for (int i = 1; i < argc; ++i)
	{
		std::string a = argv[i];
		auto nx = [&]() { return std::string(i + 1 < argc ? argv[++i] : ""); };
		if (a == "--obj") obj = nx(); else if (a == "--points") ptsPath = nx(); else if (a == "--links") linksPath = nx();
		else if (a == "--out-obj") outObj = nx(); else if (a == "--out-bin") outBin = nx();
		else if (a == "--zmin") zmin = std::stof(nx()); else if (a == "--zmax") zmax = std::stof(nx());
		else if (a == "--cs") c.cs = std::stof(nx()); else if (a == "--ch") c.ch = std::stof(nx());
		else if (a == "--radius") c.radius = std::stof(nx()); else if (a == "--height") c.height = std::stof(nx());
		else if (a == "--climb") c.climb = std::stof(nx()); else if (a == "--slope") c.slope = std::stof(nx());
		else if (a == "--tile") c.tile = std::stoi(nx()); else if (a == "--min-region") c.minRegion = std::stoi(nx());
		else if (a == "--monotone") c.monotone = true;
		else if (a == "--jumps") genJumps = true; else if (a == "--links-out") linksOut = nx();
		else if (a == "--vh") c.vh = std::stof(nx()); else if (a == "--motions") c.motions = std::stoi(nx());
		else { fprintf(stderr, "unknown arg %s\n", a.c_str()); return 2; }
	}
	if (obj.empty()) { fprintf(stderr, "usage: recast_bake --obj file.obj[.gz] ...\n"); return 2; }
	std::vector<float> verts; std::vector<int> tris; int dropped = 0;
	if (!LoadObj(obj, zmin, zmax, verts, tris, dropped)) { fprintf(stderr, "cannot read %s\n", obj.c_str()); return 1; }
	const int nt = (int)tris.size() / 3;
	float bmin[3], bmax[3];
	rcCalcBounds(verts.data(), (int)verts.size() / 3, bmin, bmax);
	// bounds from used triangles only
	bmin[0] = bmin[1] = bmin[2] = 1e30f; bmax[0] = bmax[1] = bmax[2] = -1e30f;
	for (int i : tris) for (int k = 0; k < 3; ++k) { bmin[k] = std::min(bmin[k], verts[i * 3 + k]); bmax[k] = std::max(bmax[k], verts[i * 3 + k]); }

	std::vector<Pt> pts;
	if (!ptsPath.empty())
	{
		FILE* f = fopen(ptsPath.c_str(), "r"); char kind[32]; int id; float x, y, z;
		while (f && fscanf(f, "%31s %d %f %f %f", kind, &id, &x, &y, &z) == 5) { Pt p; p.kind = kind; p.id = id; ToRecast(x, y, z, p.p); pts.push_back(p); }
		if (f) fclose(f);
	}
	std::vector<Link> links;
	if (!linksPath.empty())
	{
		FILE* f = fopen(linksPath.c_str(), "r"); float a[6]; int bd; char kind[32];
		while (f && fscanf(f, "%f %f %f %f %f %f %d %31s", a, a + 1, a + 2, a + 3, a + 4, a + 5, &bd, kind) == 8)
		{ Link L; ToRecast(a[0], a[1], a[2], L.a); ToRecast(a[3], a[4], a[5], L.b); L.bidir = bd; L.kind = kind; links.push_back(L); }
		if (f) fclose(f);
	}

	const float tw = c.tile * c.cs;
	const int tilesX = (int)ceilf((bmax[0] - bmin[0]) / tw), tilesY = (int)ceilf((bmax[2] - bmin[2]) / tw);
	const float border = (ceilf(c.radius / c.cs) + 3) * c.cs;
	std::vector<std::vector<int>> bucket(tilesX * tilesY);
	for (int i = 0; i < nt; ++i)
	{
		float lo[2] = {1e30f, 1e30f}, hi[2] = {-1e30f, -1e30f};
		for (int k = 0; k < 3; ++k) { const float* v = &verts[tris[i * 3 + k] * 3];
			lo[0] = std::min(lo[0], v[0]); hi[0] = std::max(hi[0], v[0]); lo[1] = std::min(lo[1], v[2]); hi[1] = std::max(hi[1], v[2]); }
		const int ax = std::max(0, (int)floorf((lo[0] - border - bmin[0]) / tw)), bx = std::min(tilesX - 1, (int)floorf((hi[0] + border - bmin[0]) / tw));
		const int ay = std::max(0, (int)floorf((lo[1] - border - bmin[2]) / tw)), by = std::min(tilesY - 1, (int)floorf((hi[1] + border - bmin[2]) / tw));
		for (int y = ay; y <= by; ++y) for (int x = ax; x <= bx; ++x) bucket[y * tilesX + x].push_back(i);
	}
	int totalPolys = 0, builtTiles = 0, maxTilePolys = 0;
	auto bake = [&](const std::vector<Link>& L) -> dtNavMesh* {
		dtNavMesh* nm = dtAllocNavMesh();
		dtNavMeshParams np; memset(&np, 0, sizeof np);
		rcVcopy(np.orig, bmin); np.tileWidth = tw; np.tileHeight = tw;
		int tileBits = 1; while ((1 << tileBits) < tilesX * tilesY) ++tileBits;
		if (tileBits > 14) tileBits = 14;
		np.maxTiles = 1 << tileBits; np.maxPolys = 1 << (22 - tileBits);
		if (dtStatusFailed(nm->init(&np))) { fprintf(stderr, "navmesh init failed\n"); exit(1); }
		rcContext ctx(false);
		totalPolys = 0; builtTiles = 0; maxTilePolys = 0;
		for (int y = 0; y < tilesY; ++y) for (int x = 0; x < tilesX; ++x)
		{
			int ds = 0, np_ = 0;
			unsigned char* data = BuildTile(ctx, c, verts, tris, bucket[y * tilesX + x], bmin, bmax, x, y, L, ds, np_);
			if (!data) continue;
			if (dtStatusFailed(nm->addTile(data, ds, DT_TILE_FREE_DATA, 0, nullptr))) { dtFree(data); continue; }
			totalPolys += np_; ++builtTiles; maxTilePolys = std::max(maxTilePolys, np_);
		}
		return nm;
	};
	dtNavMesh* nav = bake(links);
	int genStats[3] = {0, 0, 0};
	if (genJumps)
	{
		std::vector<Link> gen = GenerateJumpLinks(nav, verts, tris, c, genStats);
		if (!linksOut.empty())
		{
			FILE* f = fopen(linksOut.c_str(), "w");
			for (const Link& L : gen) { float a[3], b[3]; ToCrysis(L.a, a); ToCrysis(L.b, b);
				fprintf(f, "%.2f %.2f %.2f %.2f %.2f %.2f %d %s\n", a[0], a[1], a[2], b[0], b[1], b[2], L.bidir, L.kind.c_str()); }
			fclose(f);
		}
		links.insert(links.end(), gen.begin(), gen.end());
		dtFreeNavMesh(nav);
		nav = bake(links);
	}

	// connected components over polygon links (Detour links are two-way for ground polys; off-mesh one-way
	// links are treated as directed only for the "main" reach: we report undirected components plus directed reach)
	std::map<dtPolyRef, int> index; std::vector<dtPolyRef> refs; std::vector<float> area;
	const dtNavMesh* cnav = nav;
	for (int ti = 0; ti < cnav->getMaxTiles(); ++ti)
	{
		const dtMeshTile* t = cnav->getTile(ti);
		if (!t || !t->header) continue;
		const dtPolyRef base = cnav->getPolyRefBase(t);
		for (int i = 0; i < t->header->polyCount; ++i)
		{
			index[base | (dtPolyRef)i] = (int)refs.size(); refs.push_back(base | (dtPolyRef)i);
			const dtPoly* p = &t->polys[i];
			area.push_back(p->getType() == DT_POLYTYPE_GROUND ? PolyArea(t, p) : 0.f);
		}
	}
	const int N = (int)refs.size();
	std::vector<std::vector<int>> adj(N);
	for (int n = 0; n < N; ++n)
	{
		const dtMeshTile* t; const dtPoly* p; cnav->getTileAndPolyByRefUnsafe(refs[n], &t, &p);
		for (unsigned int l = p->firstLink; l != DT_NULL_LINK; l = t->links[l].next)
		{
			auto it = index.find(t->links[l].ref);
			if (it != index.end()) adj[n].push_back(it->second);
		}
	}
	// components = strongly connected components of the polygon graph (ground links are two-way)
	std::vector<int> comp(N, -1); int ncomp = 0; std::vector<float> compArea;
	std::vector<std::vector<int>> radj(N);
	for (int n = 0; n < N; ++n) for (int m : adj[n]) radj[m].push_back(n);
	{
		// Kosaraju SCC (iterative): off-mesh drop/jump links are one-way, so "main" must be strongly connected
		std::vector<int> order; order.reserve(N); std::vector<char> vis(N, 0);
		for (int s = 0; s < N; ++s)
		{
			if (vis[s]) continue;
			std::vector<std::pair<int, size_t>> st{{s, 0}}; vis[s] = 1;
			while (!st.empty())
			{
				auto& top = st.back();
				if (top.second < adj[top.first].size()) { int v = adj[top.first][top.second++]; if (!vis[v]) { vis[v] = 1; st.push_back({v, 0}); } }
				else { order.push_back(top.first); st.pop_back(); }
			}
		}
		for (int i = N - 1; i >= 0; --i)
		{
			const int s = order[i]; if (comp[s] >= 0) continue;
			std::vector<int> st{s}; comp[s] = ncomp; float a = 0;
			while (!st.empty()) { int u = st.back(); st.pop_back(); a += area[u];
				for (int v : radj[u]) if (comp[v] < 0) { comp[v] = ncomp; st.push_back(v); } }
			compArea.push_back(a); ++ncomp;
		}
	}

	dtNavMeshQuery* q = dtAllocNavMeshQuery(); q->init(nav, 4096);
	dtQueryFilter filter; filter.setIncludeFlags(0xffff);
	struct PR { int poly = -1; float d = -1; float snapped[3]; };
	std::vector<PR> pr(pts.size());
	std::map<int, int> spawnsPerComp;
	for (size_t i = 0; i < pts.size(); ++i)
	{
		// 1 m sideways, 2 m down, 0.5 m up (same as the Triage walk model projection)
		const float c0[3] = {pts[i].p[0], pts[i].p[1] - 0.75f, pts[i].p[2]}, ext[3] = {1.f, 1.25f, 1.f};
		dtPolyRef ref = 0; float nearest[3];
		q->findNearestPoly(c0, ext, &filter, &ref, nearest);
		if (ref) { pr[i].poly = index[ref]; pr[i].d = dtVdist(nearest, pts[i].p); rcVcopy(pr[i].snapped, nearest);
			if (pts[i].kind == "spawn") spawnsPerComp[comp[pr[i].poly]]++; }
	}
	int mainComp = -1, best = -1;
	for (auto& kv : spawnsPerComp) if (kv.second > best) { best = kv.second; mainComp = kv.first; }
	if (mainComp < 0) for (int k = 0; k < ncomp; ++k) if (mainComp < 0 || compArea[k] > compArea[mainComp]) mainComp = k;
	// directed: reach from main-comp spawns forward and backward (two-way = both)
	std::vector<char> fwd(N, 0), bwd(N, 0);
	{
		std::vector<int> st;
		for (int n = 0; n < N; ++n) if (comp[n] == mainComp) { fwd[n] = 1; st.push_back(n); }
		std::vector<int> st2 = st; for (int s : st2) bwd[s] = 1;
		while (!st.empty()) { int u = st.back(); st.pop_back(); for (int v : adj[u]) if (!fwd[v]) { fwd[v] = 1; st.push_back(v); } }
		while (!st2.empty()) { int u = st2.back(); st2.pop_back(); for (int v : radj[u]) if (!bwd[v]) { bwd[v] = 1; st2.push_back(v); } }
	}
	float totalArea = 0, mainArea = 0, twoWayArea = 0;
	for (int n = 0; n < N; ++n) { totalArea += area[n]; if (comp[n] == mainComp) mainArea += area[n]; if (fwd[n] && bwd[n]) twoWayArea += area[n]; }

	// outputs
	if (!outObj.empty())
	{
		FILE* f = fopen(outObj.c_str(), "w"); int vbase = 1;
		fprintf(f, "# recast_bake polygon mesh, Crysis frame (Z up); g main / other\n");
		for (int pass = 0; pass < 2; ++pass)
		{
			fprintf(f, "g %s\n", pass == 0 ? "main" : "other");
			for (int n = 0; n < N; ++n)
			{
				if ((comp[n] == mainComp) != (pass == 0)) continue;
				const dtMeshTile* t; const dtPoly* p; cnav->getTileAndPolyByRefUnsafe(refs[n], &t, &p);
				if (p->getType() != DT_POLYTYPE_GROUND) continue;
				for (int k = 0; k < p->vertCount; ++k) { float o[3]; ToCrysis(&t->verts[p->verts[k] * 3], o); fprintf(f, "v %.3f %.3f %.3f\n", o[0], o[1], o[2]); }
				fprintf(f, "f"); for (int k = 0; k < p->vertCount; ++k) fprintf(f, " %d", vbase + k); fprintf(f, "\n");
				vbase += p->vertCount;
			}
		}
		fclose(f);
	}
	if (!outBin.empty())
	{
		// simple container: magic, params, then per tile (size, data) — same layout as RecastDemo's "MSET" files
		FILE* f = fopen(outBin.c_str(), "wb");
		const int magic = 'M' << 24 | 'S' << 16 | 'E' << 8 | 'T', version = 1; int count = 0;
		for (int ti = 0; ti < cnav->getMaxTiles(); ++ti) { const dtMeshTile* t = cnav->getTile(ti); if (t && t->header && t->dataSize) ++count; }
		fwrite(&magic, 4, 1, f); fwrite(&version, 4, 1, f); fwrite(&count, 4, 1, f); fwrite(nav->getParams(), sizeof(dtNavMeshParams), 1, f);
		for (int ti = 0; ti < cnav->getMaxTiles(); ++ti)
		{
			const dtMeshTile* t = cnav->getTile(ti); if (!t || !t->header || !t->dataSize) continue;
			dtTileRef tr = cnav->getTileRef(t); fwrite(&tr, sizeof tr, 1, f); fwrite(&t->dataSize, 4, 1, f); fwrite(t->data, 1, t->dataSize, f);
		}
		fclose(f);
	}

	// JSON report
	int sp = 0, spIn = 0, spOn = 0, it = 0, itOn = 0, itTwo = 0, itFwd = 0, spTwo = 0;
	printf("{\n \"input\": {\"obj\": \"%s\", \"triangles\": %d, \"dropped_by_z\": %d, \"bounds_crysis\": [%.1f, %.1f, %.1f, %.1f, %.1f, %.1f]},\n",
		obj.c_str(), nt, dropped, bmin[0], -bmax[2], bmin[1], bmax[0], -bmin[2], bmax[1]);
	printf(" \"params\": {\"cs\": %.2f, \"ch\": %.2f, \"radius\": %.2f, \"height\": %.2f, \"climb\": %.2f, \"slope\": %.1f, \"tile\": %d, \"min_region\": %d, \"partition\": \"%s\", \"recast\": \"v1.6.0\"},\n",
		c.cs, c.ch, c.radius, c.height, c.climb, c.slope, c.tile, c.minRegion, c.monotone ? "monotone" : "watershed");
	printf(" \"mesh\": {\"tiles\": %d, \"tiles_grid\": [%d, %d], \"polys\": %d, \"max_tile_polys\": %d, \"components\": %d, \"area_m2\": %.0f, \"main_area_m2\": %.0f, \"two_way_area_m2\": %.0f, \"offmesh_links\": %zu, \"generated\": {\"drop\": %d, \"jump\": %d, \"strength\": %d}},\n",
		builtTiles, tilesX, tilesY, totalPolys, maxTilePolys, ncomp, totalArea, mainArea, twoWayArea, links.size(), genStats[0], genStats[1], genStats[2]);
	printf(" \"points\": [\n");
	for (size_t i = 0; i < pts.size(); ++i)
	{
		const bool on = pr[i].poly >= 0, inMain = on && comp[pr[i].poly] == mainComp;
		const bool two = on && fwd[pr[i].poly] && bwd[pr[i].poly], fw = on && fwd[pr[i].poly];
		if (pts[i].kind == "spawn") { ++sp; spOn += on; spIn += inMain; spTwo += two; }
		else if (pts[i].kind == "item") { ++it; itOn += on; itTwo += two; itFwd += fw; }
		float o[3]; ToCrysis(pts[i].p, o);
		printf("  {\"kind\": \"%s\", \"id\": %d, \"pos\": [%.2f, %.2f, %.2f], \"on_mesh\": %s, \"snap_m\": %.2f, \"main\": %s, \"two_way\": %s, \"reach\": %s, \"comp\": %d, \"comp_area\": %.0f}%s\n",
			pts[i].kind.c_str(), pts[i].id, o[0], o[1], o[2], on ? "true" : "false", pr[i].d, inMain ? "true" : "false",
			two ? "true" : "false", fw ? "true" : "false", on ? comp[pr[i].poly] : -1, on ? compArea[comp[pr[i].poly]] : 0.f,
			i + 1 < pts.size() ? "," : "");
	}
	printf(" ],\n \"summary\": {\"spawns\": %d, \"spawns_on_mesh\": %d, \"spawns_in_main\": %d, \"spawns_two_way\": %d, \"items\": %d, \"items_on_mesh\": %d, \"items_two_way\": %d, \"items_reachable\": %d}\n}\n",
		sp, spOn, spIn, spTwo, it, itOn, itTwo, itFwd);
	dtFreeNavMeshQuery(q); dtFreeNavMesh(nav);
	return 0;
}
