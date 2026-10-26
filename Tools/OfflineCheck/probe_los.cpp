// Probe fixture check (C-104): two points must be connected on the navmesh and see each other along the walkable
// surface, also with +-1 m offsets (not a sliver). The C-100 duel (x 1267 vs 1290) failed this: a wall at x 1281.4,
// 0:0 kills in 3 trials. Navmesh rays miss overhead geometry and low crates -> necessary, not sufficient.
// Build: g++ -std=c++17 -O1 -I. -ICode -IThirdParty/recastnavigation/Detour/Include Tools/OfflineCheck/probe_los.cpp
//        Code/CryGame/Bots/BotNavMeshCore.cpp ThirdParty/recastnavigation/Detour/Source/*.cpp -o probe_los
// Run:   probe_los Tools/RecastBake/meshes/Steelmill.navmesh x1 y1 z1 x2 y2 z2   (exit 0 = ok)
#include "CryGame/Bots/BotNavMeshCore.h"
#include <cstdio>
#include <cstdlib>
#include <string>
int main(int argc, char** argv)
{
	if (argc != 8) { std::puts("usage: probe_los <navmesh> x1 y1 z1 x2 y2 z2"); return 2; }
	WarsBots::NavMeshCore m; std::string e;
	if (!m.LoadFile(argv[1], &e)) { std::printf("load failed: %s\n", e.c_str()); return 2; }
	const float a[3] = {float(atof(argv[2])), float(atof(argv[3])), float(atof(argv[4]))};
	const float b[3] = {float(atof(argv[5])), float(atof(argv[6])), float(atof(argv[7]))};
	float sa[3], sb[3];
	if (!m.Project(a, sa) || !m.Project(b, sb)) { std::puts("FAIL: a point is off the navmesh"); return 1; }
	const bool reach = m.Reachable(sa, sb), ray = m.RayClear(sa, sb);
	int clear = 0, tried = 0;
	for (int dx = -1; dx <= 1; ++dx) for (int dy = -1; dy <= 1; ++dy)
	{
		const float a2[3] = {sa[0] + dx, sa[1] + dy, sa[2]}, b2[3] = {sb[0] + dx, sb[1] + dy, sb[2]};
		float x[3], y[3];
		if (!m.Project(a2, x, 0.3f) || !m.Project(b2, y, 0.3f)) continue;
		++tried; if (m.RayClear(x, y)) ++clear;
	}
	std::printf("reachable=%d ray=%d offsets_clear=%d/%d\n", reach, ray, clear, tried);
	const bool ok = reach && ray && clear * 2 > tried;
	std::puts(ok ? "OK" : "FAIL");
	return ok ? 0 : 1;
}
