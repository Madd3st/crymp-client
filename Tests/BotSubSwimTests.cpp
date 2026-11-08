// SPEC-006a p. 4 (D-058): real NavMeshCore on the shipped Shore navmesh (Tools/RecastBake/meshes, bake b23e85ff).
// Under the submarine deck the bot is below solid ground polygons and the nearest open water lies 15-19 m away at the
// hull side; out in the open sea nothing is overhead. Run from the repository root (CI does). No engine.
#include "CryGame/Bots/BotNavMeshCore.h"
#include <cmath>
#include <cstdio>
#include <string>

using namespace WarsBots;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

int main(int argc, char** argv)
{
	const char* path = argc > 1 ? argv[1] : "Tools/RecastBake/meshes/Shore.navmesh";
	NavMeshCore m; std::string err;
	if (!m.LoadFile(path, &err)) { std::printf("FAIL load %s: %s\n", path, err.c_str()); return 1; }
	// the bot in the water pocket under the lower deck (07.11: 130993 at (2118.7,1882.6,88.4), swim=1)
	const float pit[3] = {2118.7f, 1882.6f, 88.4f};
	CHECK(m.SolidAbove(pit));                                     // floor 90.16 / deck 99.76 above
	float w[3] = {0, 0, 0};
	CHECK(m.NearestOpenWater(pit, 40.f, w));
	const float d = std::hypot(w[0] - pit[0], w[1] - pit[1]);
	std::printf("pit -> open water (%.1f,%.1f,%.2f) %.1f m\n", w[0], w[1], w[2], d);
	CHECK(d > 10.f && d < 25.f);                                  // the hull side, not under the hull
	CHECK(std::fabs(w[2] - 99.5f) < 0.2f);                        // the sea surface
	CHECK(!m.SolidAbove(w));                                       // open sky over the target
	CHECK(!(w[0] > 2103.f && w[0] < 2138.f && w[1] > 1877.f && w[1] < 1892.f));   // outside the hull box
	// out in the open sea: nothing overhead
	const float sea[3] = {2118.6f, 1898.3f, 98.5f};
	CHECK(!m.SolidAbove(sea));
	// no open water far inland within a short radius
	const float land[3] = {1961.0f, 1387.6f, 110.3f};
	float w2[3];
	CHECK(!m.NearestOpenWater(land, 10.f, w2));
	std::printf(failures ? "BotSubSwimTests FAIL %d\n" : "BotSubSwimTests PASS\n", failures);
	return failures ? 1 : 0;
}
