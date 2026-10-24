// Offline test of the Detour runtime on a real baked mesh (Tools/RecastBake output, MSET v1).
// Usage: BotNavMeshCoreTests <Steelmill.navmesh>   (skips with exit 0 when no file is given)
#include "CryGame/Bots/BotNavMeshCore.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
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
	std::printf("BotNavMeshCoreTests OK polys=%d parts=%d corners=%zu length=%.1f m\n", m.Polys(), m.Components(), n, len);
	return 0;
}
