// Tactical spots offline (CS-style hiding / sniper / exposed spots, Killzone-style position data) for WarsBots.
// Input: a baked Detour mesh (MSET v1, Tools/RecastBake) + the static collision crop OBJ (Crysis frame, Z up).
// For walkable sample points (every walk polygon, plus a ~2.5 m grid on big ones) it casts rays against collision:
//   cover   16 horizontal directions at chest height (1.0 m), hit within 1.5 m = protected side;
//           "low" cover if the same direction is free at eye height (1.6 m): shoot over it.
//   open    16 directions at eye height free for 25 m.
//   view    eye -> chest of ~800 random walkable targets 15..150 m away: how much of the map is visible.
// Flags: COVER (1..11 protected sides, >= 3 open), LOW_COVER (any low side), SNIPER (view in the top 5 % and at least
// 3 m above the median floor), EXPOSED (no protected side, >= 12 open). Output text, one spot per line:
//   x y z flags coverMask lowMask openCount view
// Only polygons reachable (walk + baked links, either direction) from the map's spawn points count: the mesh also
// covers out-of-bounds terrain. Usage: tactical_spots <map.navmesh> <crop.obj> <map.pts> <out.txt> [grid=2.5] [targets=800]
#include "DetourNavMesh.h"
#include "DetourCommon.h"
#include "DetourNavMeshQuery.h"
#include <unordered_set>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>
#include <unordered_map>

namespace {
struct V3 { float x, y, z; };
inline V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V3 operator*(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float Dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 Cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }

struct Tri { V3 a, e1, e2; };
struct Node { float lo[3], hi[3]; int left, right, first, count; };

struct Bvh {
	std::vector<Tri> tris; std::vector<int> idx; std::vector<Node> nodes;
	void Build()
	{
		idx.resize(tris.size()); for (size_t i = 0; i < idx.size(); ++i) idx[i] = int(i);
		nodes.reserve(tris.size() * 2 / 4 + 16);
		Make(0, int(idx.size()));
	}
	void Bounds(int t, float* lo, float* hi) const
	{
		const Tri& r = tris[t]; V3 b = r.a + r.e1, c = r.a + r.e2;
		lo[0] = std::min({r.a.x, b.x, c.x}); lo[1] = std::min({r.a.y, b.y, c.y}); lo[2] = std::min({r.a.z, b.z, c.z});
		hi[0] = std::max({r.a.x, b.x, c.x}); hi[1] = std::max({r.a.y, b.y, c.y}); hi[2] = std::max({r.a.z, b.z, c.z});
	}
	int Make(int first, int count)
	{
		Node n; for (int k = 0; k < 3; ++k) { n.lo[k] = 1e30f; n.hi[k] = -1e30f; }
		for (int i = first; i < first + count; ++i)
		{ float lo[3], hi[3]; Bounds(idx[i], lo, hi); for (int k = 0; k < 3; ++k) { n.lo[k] = std::min(n.lo[k], lo[k]); n.hi[k] = std::max(n.hi[k], hi[k]); } }
		const int id = int(nodes.size()); nodes.push_back(n);
		if (count <= 4) { nodes[id].left = nodes[id].right = -1; nodes[id].first = first; nodes[id].count = count; return id; }
		int axis = 0; float ext = 0; for (int k = 0; k < 3; ++k) if (n.hi[k] - n.lo[k] > ext) { ext = n.hi[k] - n.lo[k]; axis = k; }
		auto cen = [&](int t) { float lo[3], hi[3]; Bounds(t, lo, hi); return lo[axis] + hi[axis]; };
		const int mid = first + count / 2;
		std::nth_element(idx.begin() + first, idx.begin() + mid, idx.begin() + first + count, [&](int a, int b) { return cen(a) < cen(b); });
		const int l = Make(first, mid - first), r = Make(mid, first + count - mid);
		nodes[id].left = l; nodes[id].right = r; nodes[id].first = 0; nodes[id].count = 0;
		return id;
	}
	// true if the segment o -> o + d*len hits a triangle
	bool Hit(V3 o, V3 d, float len) const
	{
		const float inv[3] = {std::fabs(d.x) > 1e-9f ? 1 / d.x : 1e30f, std::fabs(d.y) > 1e-9f ? 1 / d.y : 1e30f, std::fabs(d.z) > 1e-9f ? 1 / d.z : 1e30f};
		const float oo[3] = {o.x, o.y, o.z};
		int stack[128]; int sp = 0; stack[sp++] = 0;
		while (sp)
		{
			const Node& n = nodes[stack[--sp]];
			float t0 = 0, t1 = len;
			bool miss = false;
			for (int k = 0; k < 3 && !miss; ++k)
			{
				float a = (n.lo[k] - oo[k]) * inv[k], b = (n.hi[k] - oo[k]) * inv[k];
				if (a > b) std::swap(a, b);
				t0 = std::max(t0, a); t1 = std::min(t1, b); miss = t0 > t1;
			}
			if (miss) continue;
			if (n.left < 0)
			{
				for (int i = n.first; i < n.first + n.count; ++i)
				{
					const Tri& r = tris[idx[i]];
					V3 p = Cross(d, r.e2); float det = Dot(r.e1, p); if (std::fabs(det) < 1e-9f) continue;
					float f = 1 / det; V3 s = o - r.a; float u = Dot(s, p) * f; if (u < 0 || u > 1) continue;
					V3 q = Cross(s, r.e1); float v = Dot(d, q) * f; if (v < 0 || u + v > 1) continue;
					float t = Dot(r.e2, q) * f; if (t > 0.03f && t < len) return true;
				}
			}
			else if (sp < 126) { stack[sp++] = n.left; stack[sp++] = n.right; }
		}
		return false;
	}
};

bool ReadAll(const char* path, std::string& s)
{
	FILE* f = std::fopen(path, "rb"); if (!f) return false;
	char buf[1 << 16]; size_t n; while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) s.append(buf, n);
	std::fclose(f); return true;
}

struct MsetHeader { int magic; int version; int numTiles; dtNavMeshParams params; };
struct TileHeader { dtTileRef tileRef; int dataSize; };

inline V3 FromRecast(const float* r) { return {r[0], -r[2], r[1]}; }
}

int main(int argc, char** argv)
{
	if (argc < 5) { std::fprintf(stderr, "usage: tactical_spots map.navmesh crop.obj map.pts out.txt [grid] [targets]\n"); return 2; }
	const float grid = argc > 5 ? float(std::atof(argv[5])) : 2.5f;
	const int ntargets = argc > 6 ? std::atoi(argv[6]) : 800;
	const char* outPath = argv[4];
	// --- navmesh
	std::string nm; if (!ReadAll(argv[1], nm)) { std::fprintf(stderr, "cannot read navmesh\n"); return 1; }
	MsetHeader h; std::memcpy(&h, nm.data(), sizeof h);
	dtNavMesh* mesh = dtAllocNavMesh(); mesh->init(&h.params);
	size_t o = sizeof h;
	for (int i = 0; i < h.numTiles; ++i)
	{
		TileHeader t; std::memcpy(&t, nm.data() + o, sizeof t); o += sizeof t;
		unsigned char* d = (unsigned char*)dtAlloc(t.dataSize, DT_ALLOC_PERM); std::memcpy(d, nm.data() + o, t.dataSize); o += t.dataSize;
		mesh->addTile(d, t.dataSize, DT_TILE_FREE_DATA, 0, nullptr);
	}
	// playable polygons: flood from spawns over all links (undirected), walk + off-mesh
	dtNavMeshQuery* query = dtAllocNavMeshQuery(); query->init(mesh, 2048);
	dtQueryFilter filter; filter.setIncludeFlags(0xffff);
	std::unordered_set<dtPolyRef> playable; std::vector<dtPolyRef> stack;
	{
		FILE* pf = std::fopen(argv[3], "r"); char kind[32]; int id; float x, y, z; int spawns = 0;
		while (pf && std::fscanf(pf, "%31s %d %f %f %f", kind, &id, &x, &y, &z) == 5)
		{
			if (std::strcmp(kind, "spawn")) continue;
			const float c[3] = {x, z + 0.5f, -y}, ext[3] = {2.f, 3.f, 2.f}; dtPolyRef ref = 0; float np[3];
			if (dtStatusSucceed(query->findNearestPoly(c, ext, &filter, &ref, np)) && ref && playable.insert(ref).second) { stack.push_back(ref); ++spawns; }
		}
		if (pf) std::fclose(pf);
		std::fprintf(stderr, "spawn seeds %d\n", spawns);
	}
	std::vector<std::vector<dtPolyRef>> back;  // reverse links collected lazily: build a full adjacency once
	std::unordered_map<dtPolyRef, std::vector<dtPolyRef>> rev;
	for (int ti = 0; ti < mesh->getMaxTiles(); ++ti)
	{
		const dtMeshTile* tile = static_cast<const dtNavMesh*>(mesh)->getTile(ti); if (!tile || !tile->header) continue;
		const dtPolyRef base = mesh->getPolyRefBase(tile);
		for (int p = 0; p < tile->header->polyCount; ++p)
			for (unsigned l = tile->polys[p].firstLink; l != DT_NULL_LINK; l = tile->links[l].next) rev[tile->links[l].ref].push_back(base | dtPolyRef(p));
	}
	while (!stack.empty())
	{
		const dtPolyRef r = stack.back(); stack.pop_back();
		const dtMeshTile* tile = nullptr; const dtPoly* poly = nullptr;
		if (dtStatusFailed(mesh->getTileAndPolyByRef(r, &tile, &poly))) continue;
		for (unsigned l = poly->firstLink; l != DT_NULL_LINK; l = tile->links[l].next) if (playable.insert(tile->links[l].ref).second) stack.push_back(tile->links[l].ref);
		auto it = rev.find(r); if (it != rev.end()) for (dtPolyRef q : it->second) if (playable.insert(q).second) stack.push_back(q);
	}
	std::vector<V3> samples;
	const dtNavMesh* cm = mesh;
	for (int ti = 0; ti < cm->getMaxTiles(); ++ti)
	{
		const dtMeshTile* tile = cm->getTile(ti); if (!tile || !tile->header) continue;
		const dtPolyRef base = cm->getPolyRefBase(tile);
		for (int p = 0; p < tile->header->polyCount; ++p)
		{
			const dtPoly& poly = tile->polys[p];
			if (poly.getType() != DT_POLYTYPE_GROUND || !(poly.flags & 1) || !playable.count(base | dtPolyRef(p))) continue;
			std::vector<V3> v; for (int k = 0; k < poly.vertCount; ++k) v.push_back(FromRecast(&tile->verts[poly.verts[k] * 3]));
			V3 c{0, 0, 0}; for (auto& q : v) c = c + q; c = c * (1.f / v.size());
			samples.push_back(c);
			for (size_t k = 1; k + 1 < v.size(); ++k)       // grid inside each fan triangle
			{
				V3 a = v[0], e1 = v[k] - a, e2 = v[k + 1] - a;
				const float area = 0.5f * std::sqrt(Dot(Cross(e1, e2), Cross(e1, e2)));
				const int n = int(area / (grid * grid));
				for (int s = 0; s < n; ++s)
				{
					float u = float(std::rand()) / RAND_MAX, w = float(std::rand()) / RAND_MAX;
					if (u + w > 1) { u = 1 - u; w = 1 - w; }
					samples.push_back(a + e1 * u + e2 * w);
				}
			}
		}
	}
	// --- collision (only near the walkable area)
	float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
	for (auto& s : samples) { lo[0] = std::min(lo[0], s.x); lo[1] = std::min(lo[1], s.y); lo[2] = std::min(lo[2], s.z); hi[0] = std::max(hi[0], s.x); hi[1] = std::max(hi[1], s.y); hi[2] = std::max(hi[2], s.z); }
	std::string obj; if (!ReadAll(argv[2], obj)) { std::fprintf(stderr, "cannot read obj\n"); return 1; }
	Bvh bvh; std::vector<V3> verts;
	{
		const char* p = obj.c_str(); const char* end = p + obj.size(); std::vector<int> idx;
		while (p < end)
		{
			const char* nl = (const char*)std::memchr(p, '\n', end - p); if (!nl) nl = end;
			if (p[0] == 'v' && p[1] == ' ') { char* q; float x = std::strtof(p + 2, &q), y = std::strtof(q, &q), z = std::strtof(q, &q); verts.push_back({x, y, z}); }
			else if (p[0] == 'f' && p[1] == ' ')
			{
				idx.clear(); const char* q = p + 2;
				while (q < nl) { while (q < nl && (*q == ' ' || *q == '\t' || *q == '\r')) ++q; if (q >= nl) break; char* e; long i = std::strtol(q, &e, 10); if (e == q) break;
					idx.push_back(i > 0 ? int(i - 1) : int(verts.size() + i)); q = e; while (q < nl && *q != ' ' && *q != '\t') ++q; }
				for (size_t k = 1; k + 1 < idx.size(); ++k)
				{
					V3 a = verts[idx[0]], b = verts[idx[k]], c = verts[idx[k + 1]];
					const float mx = 160.f;
					if (std::max({a.x, b.x, c.x}) < lo[0] - mx || std::min({a.x, b.x, c.x}) > hi[0] + mx ||
						std::max({a.y, b.y, c.y}) < lo[1] - mx || std::min({a.y, b.y, c.y}) > hi[1] + mx ||
						std::max({a.z, b.z, c.z}) < lo[2] - 20 || std::min({a.z, b.z, c.z}) > hi[2] + 60) continue;
					bvh.tris.push_back({a, b - a, c - a});
				}
			}
			p = nl + 1;
		}
	}
	bvh.Build();
	std::fprintf(stderr, "samples %zu, triangles %zu\n", samples.size(), bvh.tris.size());
	// --- targets
	std::mt19937 rng(7); std::vector<V3> targets;
	for (int i = 0; i < ntargets && !samples.empty(); ++i) targets.push_back(samples[rng() % samples.size()] + V3{0, 0, 1.2f});
	std::vector<float> zs; for (auto& s : samples) zs.push_back(s.z); std::nth_element(zs.begin(), zs.begin() + zs.size() / 2, zs.end());
	const float zmed = zs[zs.size() / 2];
	struct Out { V3 p; unsigned cover, low; int open; float view; };
	std::vector<Out> out(samples.size());
	for (size_t i = 0; i < samples.size(); ++i)
	{
		const V3 p = samples[i], chest = p + V3{0, 0, 1.0f}, eye = p + V3{0, 0, 1.6f};
		Out r{p, 0, 0, 0, 0};
		for (int k = 0; k < 16; ++k)
		{
			const float a = k * 3.14159265f / 8; V3 d{std::sin(a), std::cos(a), 0};
			const bool c = bvh.Hit(chest, d, 1.5f);
			if (c) { r.cover |= 1u << k; if (!bvh.Hit(eye, d, 1.5f)) r.low |= 1u << k; }
			if (!bvh.Hit(eye, d, 25.f)) ++r.open;
		}
		for (const V3& t : targets)
		{
			V3 d = t - eye; const float len = std::sqrt(Dot(d, d));
			if (len < 15.f || len > 150.f) continue;
			if (!bvh.Hit(eye, d * (1 / len), len - 0.3f)) r.view += 1.f;
		}
		out[i] = r;
		if (i % 2000 == 0) std::fprintf(stderr, "%zu/%zu\n", i, samples.size());
	}
	std::vector<float> views; for (auto& r : out) views.push_back(r.view);
	std::sort(views.begin(), views.end());
	const float v95 = views.empty() ? 0 : views[size_t(views.size() * 0.95)];
	FILE* f = std::fopen(outPath, "w");
	std::fprintf(f, "# tactical spots v1: x y z flags(1 cover,2 low_cover,4 sniper,8 exposed) coverMask lowMask open16 view(targets 15-150m of %d) zmed=%.2f v95=%.0f\n", int(targets.size()), zmed, v95);
	int counts[4] = {};
	for (auto& r : out)
	{
		int nc = __builtin_popcount(r.cover);
		unsigned flags = 0;
		if (nc >= 1 && nc <= 11 && r.open >= 3) flags |= 1;
		if (r.low) flags |= 2;
		if (r.view >= v95 && v95 > 0 && r.p.z >= zmed + 3.f) flags |= 4;
		if (nc == 0 && r.open >= 12) flags |= 8;
		for (int b = 0; b < 4; ++b) counts[b] += (flags >> b) & 1;
		std::fprintf(f, "%.2f %.2f %.2f %u %u %u %d %.0f\n", r.p.x, r.p.y, r.p.z, flags, r.cover, r.low, r.open, r.view);
	}
	std::fclose(f);
	std::fprintf(stderr, "spots %zu: cover %d, low cover %d, sniper %d, exposed %d (v95 %.0f, zmed %.1f)\n", out.size(), counts[0], counts[1], counts[2], counts[3], v95, zmed);
	return 0;
}
