// Offline test of the Detour runtime on a real baked mesh (Tools/RecastBake output, MSET v1).
// Usage: BotNavMeshCoreTests <Steelmill.navmesh>   (skips with exit 0 when no file is given)
#include "CryGame/Bots/BotNavMeshCore.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
#include <string>
using namespace WarsBots;

int main(int argc, char** argv)
{
	{
		NavMeshCore empty; std::string err;
		const unsigned char junk[8] = {1, 2, 3, 4, 5, 6, 7, 8};
		assert(!empty.LoadMemory(junk, sizeof junk, &err) && !empty.Ready());
		std::vector<float> p; const char* why = nullptr; const float a[3] = {0, 0, 0};
		assert(!empty.FindPath(a, a, p, &why));
	}
	if (argc < 2) { std::puts("BotNavMeshCoreTests OK (no mesh given: load/path on real data skipped)"); return 0; }
	NavMeshCore m; std::string err;
	if (!m.LoadFile(argv[1], &err)) { std::printf("load failed: %s\n", err.c_str()); return 1; }
	assert(m.Ready() && m.Polys() > 1000);
	// Steelmill spawns 289 (1290.1, 1362.4, 136.1) and 176 (1206.3, 1366.6, 136.1): same part, a path with corners.
	const float s289[3] = {1290.125f, 1362.375f, 136.125f}, s176[3] = {1206.33f, 1366.625f, 136.125f};
	assert(m.Reachable(s289, s176));
	std::vector<float> path; const char* why = nullptr;
	assert(m.FindPath(s289, s176, path, &why));
	assert(path.size() >= 3 && path.size() % 3 == 0);
	const size_t n = path.size() / 3;
	const float* last = &path[(n - 1) * 3];
	assert(std::hypot(last[0] - s176[0], last[1] - s176[1]) < 1.5f);
	float len = 0, px = s289[0], py = s289[1];
	for (size_t i = 0; i < n; ++i) { len += std::hypot(path[i * 3] - px, path[i * 3 + 1] - py); px = path[i * 3]; py = path[i * 3 + 1]; }
	assert(len >= 84.0f - 0.5f && len < 84.0f * 2.0f);   // straight line is ~84 m
	// A point far outside the level is off the mesh.
	const float off[3] = {0, 0, 0};
	assert(!m.FindPath(s289, off, path, &why));
	// C-036: a mesh with off-mesh links (meshes_links) + its links.txt: walk-only by default, kinds switch on by mask.
	if (argc >= 4)
	{
		NavMeshCore L; assert(L.LoadFile(argv[2], &err));
		std::vector<char> txt; if (FILE* f = std::fopen(argv[3], "rb")) { int c; while ((c = std::fgetc(f)) != EOF) txt.push_back(char(c)); std::fclose(f); }
		txt.push_back(0);
		const int kinds = L.LoadLinkKinds(txt.data());
		assert(L.Links() > 100 && kinds == L.Links() && L.LinkMismatches() == 0);
		{   // T-6: a reordered links.txt still gives every link its kind (matched by end points)
			std::string t(txt.data()); std::vector<std::string> rows; size_t p0 = 0, p1;
			while ((p1 = t.find('\n', p0)) != std::string::npos) { rows.push_back(t.substr(p0, p1 - p0 + 1)); p0 = p1 + 1; }
			std::string rev; for (size_t r = rows.size(); r-- > 0;) rev += rows[r];
			NavMeshCore R; assert(R.LoadFile(argv[2], &err));
			assert(R.LoadLinkKinds(rev.c_str()) == L.Links() && R.LinkMismatches() > 0);
		}
		const int walkParts = L.Components();
		std::vector<float> p2; std::vector<NavPathLink> links;
		assert(L.FindPathLinks(s289, s176, p2, links, &why) && links.empty());    // walk only: no links on the path
		L.SetLinkMask(NavLinkFlag(NavLinkDrop) | NavLinkFlag(NavLinkJump) | NavLinkFlag(NavLinkStrength));
		assert(L.Components() < walkParts);                                       // links join parts
		// every spawn-to-spawn path that uses links reports them on consecutive corners
		int withLinks = 0, total = 0, kindCount[NavLinkKinds] = {};
		const float spawns[][3] = {{1290.125f, 1362.375f, 136.125f}, {1206.33f, 1366.625f, 136.125f}, {1282.72f, 1513.17f, 143.0f},
			{1382.52f, 1507.1f, 190.0f}, {1265.5f, 1500.4f, 112.74f}, {1108.34f, 1473.84f, 119.21f}};
		for (auto& a : spawns) for (auto& b : spawns)
		{
			if (&a == &b) continue;
			const bool reach = L.Reachable(a, b), found = L.FindPathLinks(a, b, p2, links, &why);
			assert(reach == found || std::string(why) == "findpath_failed");   // directed parts agree with Detour
			if (!found) continue;
			++total; withLinks += !links.empty();
			for (const NavPathLink& k : links)
			{
				assert(k.corner >= 0 && size_t(k.corner + 1) < p2.size() / 3 && k.ref != 0 && k.kind >= 0 && k.kind < NavLinkKinds);
				assert(std::fabs(p2[k.corner * 3] - k.a[0]) < 1e-3f && std::fabs(p2[(k.corner + 1) * 3] - k.b[0]) < 1e-3f);
				++kindCount[k.kind];
			}
		}
		// banning a used link removes it from later paths
		if (L.FindPathLinks(spawns[4], spawns[3], p2, links, &why) && !links.empty())
		{
			const unsigned long long r = links[0].ref; L.BanLink(r);
			if (L.FindPathLinks(spawns[4], spawns[3], p2, links, &why)) for (const NavPathLink& k : links) assert(k.ref != r);
		}
		L.SetLinkMask(0); assert(L.Components() == walkParts);
		std::printf("links=%d walkParts=%d paths=%d withLinks=%d drop=%d jump=%d strength=%d\n", L.Links(), walkParts, total, withLinks,
			kindCount[0], kindCount[1], kindCount[2]);
	}
	std::printf("BotNavMeshCoreTests OK polys=%d parts=%d corners=%zu length=%.1f m\n", m.Polys(), m.Components(), n, len);
	return 0;
}
