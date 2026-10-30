// Offline test of the Detour runtime on a real baked mesh (Tools/RecastBake output, MSET v1).
// Usage: BotNavMeshCoreTests <Steelmill.navmesh>   (skips with exit 0 when no file is given)
#include <cstring>
#include <algorithm>
#include "CryGame/Bots/BotNavMeshCore.h"
#include "CryGame/Bots/BotCorners.h"
#include "CryGame/Bots/BotSniperReach.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <sstream>
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
	// Wall clearance: open floor in the middle of the spawn route has none nearby; the c068 stair trap
	// (1305.98, 1353.56, 144.36) sits on the eroded edge, and the inward direction stays on the mesh.
	{
		float d = 0, dir[2] = {0, 0};
		assert(!m.WallClearance(off, 0.5f, d, dir));
		const float trap[3] = {1305.98f, 1353.56f, 144.36f};
		if (m.WallClearance(trap, 0.5f, d, dir))
		{
			std::printf("trap clearance %.3f dir (%.2f, %.2f)\n", d, dir[0], dir[1]);
			assert(d < 0.5f && (dir[0] != 0 || dir[1] != 0));
			const float in[3] = {trap[0] + dir[0] * 0.15f, trap[1] + dir[1] * 0.15f, trap[2]};
			float s2[3]; assert(m.Project(in, s2, 0.05f, 0.5f, 0.5f));
			// The c068 turn: from the stair mouth the next corner east (1308.98, 1353.38) is behind the railing end,
			// so it must not be skipped to; a point straight down the walkway is.
			const float east[3] = {1308.98f, 1353.38f, 144.38f};
			assert(!m.RayClear(trap, east));
			const float w0[3] = {1307.5f, 1352.95f, 144.4f}, w1[3] = {1311.5f, 1352.95f, 144.4f};
			assert(m.RayClear(w0, w1));
			// The real first corner of a path out of the mouth hugs the railing end; pushing it inward gains clearance.
			const float goal[3] = {1312.0f, 1353.0f, 144.4f};
			std::vector<float> p2; const char* w = nullptr;
			assert(m.FindPath(trap, goal, p2, &w) && p2.size() >= 6);
			float cd = 0, cdir[2] = {0, 0};
			assert(m.WallClearance(&p2[0], 0.15f, cd, cdir) && cd < 0.15f && (cdir[0] != 0 || cdir[1] != 0));
			const float moved[3] = {p2[0] + cdir[0] * (0.15f - cd), p2[1] + cdir[1] * (0.15f - cd), p2[2]};
			float cd2 = 0, cdir2[2]; assert(m.WallClearance(moved, 0.5f, cd2, cdir2) && cd2 > cd + 0.1f);
			std::printf("mouth corner (%.2f, %.2f) clearance %.2f -> %.2f\n", p2[0], p2[1], cd, cd2);
			// corners v2 on the real path down the stair: the mouth corner moves outside the turn and stays on the mesh
			const float top[3] = {1306.1f, 1356.5f, 146.0f};
			std::vector<float> p3; assert(m.FindPath(top, goal, p3, &w) && p3.size() >= 6);
			const float pa[2] = {top[0], top[1]}, pc[2] = {p3[0], p3[1]}, pn[2] = {p3[3], p3[4]};
			const CornerPlan cp = OffsetCorner(pa, pc, pn);
			const float q[3] = {cp.x, cp.y, p3[2]}; float s3[3];
			assert(cp.moved && m.Project(q, s3, 0.2f, 0.6f, 0.6f) && std::hypot(s3[0] - q[0], s3[1] - q[1]) < 0.05f);
			assert(std::hypot(cp.x - 1306.57f, cp.y - 1353.54f) > 0.45f);
			std::printf("stair corner (%.2f, %.2f) turn %.0f -> aim (%.2f, %.2f), next (%.2f, %.2f)\n", p3[0], p3[1], cp.turnDeg, cp.x, cp.y, p3[3], p3[4]);
			// the leg from the aim point to the next corner clears the railing end post by more than the capsule
			{
				float nx2 = p3[3], ny2 = p3[4];
				if (p3.size() >= 9) { const float c2[2] = {p3[3], p3[4]}, n2[2] = {p3[6], p3[7]}; const CornerPlan cp2 = OffsetCorner(pc, c2, n2); nx2 = cp2.x; ny2 = cp2.y; }
				const float ex = nx2 - cp.x, ey = ny2 - cp.y, L2 = ex * ex + ey * ey;
				float t = ((1306.57f - cp.x) * ex + (1353.54f - cp.y) * ey) / L2; t = t < 0 ? 0 : (t > 1 ? 1 : t);
				const float dpost = std::hypot(cp.x + ex * t - 1306.57f, cp.y + ey * t - 1353.54f);
				std::printf("  leg clearance to the post %.2f\n", dpost);
				assert(dpost > 0.4f);
			}
		}
	}
	// C-036: a mesh with off-mesh links (meshes_links) + its links.txt: walk-only by default, kinds switch on by mask.
	if (argc >= 4)
	{
		NavMeshCore L; assert(L.LoadFile(argv[2], &err));
		std::vector<char> txt; if (FILE* f = std::fopen(argv[3], "rb")) { int c; while ((c = std::fgetc(f)) != EOF) txt.push_back(char(c)); std::fclose(f); }
		txt.push_back(0);
		const int kinds = L.LoadLinkKinds(txt.data());
		// DS-037 #3: links.txt is filtered (flat/zero/duplicate Strength links removed) -> those links stay unknown on purpose
		int removed = 0; for (const char* q = txt.data(); (q = std::strstr(q, " removed")) != nullptr; ++q) ++removed; // DS-048: data-disabled links
		int tooHigh = 0;
		{
			std::istringstream in(txt.data()); std::string line;
			while (std::getline(in, line))
			{
				std::istringstream ls(line); float ax, ay, az, bx, by, bz; int bidir = 0; std::string kind;
				if ((ls >> ax >> ay >> az >> bx >> by >> bz >> bidir >> kind) && kind == "strength")
				{
					const float rise = bidir ? std::fabs(bz - az) : (bz - az);
					if (rise > MaxStrengthLinkRise()) ++tooHigh;
				}
			}
		}
		const int lines = int(std::count(txt.begin(), txt.end(), '\n')) - removed;
		assert(tooHigh == 76);
		assert(L.Links() > 100 && kinds == lines - tooHigh && kinds <= L.Links() && L.Links() - (kinds + tooHigh) < 100);
		{   // T-6: a reordered links.txt still gives every link its kind (matched by end points)
			std::string t(txt.data()); std::vector<std::string> rows; size_t p0 = 0, p1;
			while ((p1 = t.find('\n', p0)) != std::string::npos) { rows.push_back(t.substr(p0, p1 - p0 + 1)); p0 = p1 + 1; }
			std::string rev; for (size_t r = rows.size(); r-- > 0;) rev += rows[r];
			NavMeshCore R; assert(R.LoadFile(argv[2], &err));
			assert(R.LoadLinkKinds(rev.c_str()) == kinds && R.LinkMismatches() > 0);   // same set, any order
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
				if (k.kind == NavLinkStrength) assert(k.b[2] - k.a[2] <= MaxStrengthLinkRise());
				++kindCount[k.kind];
			}
		}
		// Steelmill sniper roof routes: rejecting >MaxStrengthLinkRise() links avoids the impossible +4.22 m
		// (1354.96,1347.60,147.82)->(1351.94,1351.98,152.04) shortcut and routes via executable stairs/links.
		{
			const float roofCatwalk254[3] = {1299.90f, 1326.70f, 158.50f};
			assert(L.Reachable(s289, roofCatwalk254) && L.FindPathLinks(s289, roofCatwalk254, p2, links, &why));
			assert(links.empty());
			const float upperRoof181[3] = {1301.72f, 1318.30f, 161.54f};
			assert(L.Reachable(s289, upperRoof181) && L.FindPathLinks(s289, upperRoof181, p2, links, &why));
			assert(links.size() == 1 && links[0].kind == NavLinkStrength && links[0].b[2] - links[0].a[2] <= MaxStrengthLinkRise());
			const float impossibleLedge36[3] = {1160.89f, 1350.00f, 144.14f};
			assert(!L.Reachable(s289, impossibleLedge36) && !L.FindPathLinks(s289, impossibleLedge36, p2, links, &why));
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
