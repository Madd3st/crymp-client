// SPEC-006a p. 4 (D-058): real NavMeshCore on the shipped Shore navmesh (Tools/RecastBake/meshes, bake b23e85ff).
// Under the submarine deck the bot is below solid ground polygons and the nearest open water lies 15-19 m away at the
// hull side; out in the open sea nothing is overhead. Run from the repository root (CI does). No engine.
#include "CryGame/Bots/BotNavMeshCore.h"
#include "CryGame/Bots/BotSubSwim.h"
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

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
	// Codex 59c1391 fact: start (2116.75,1882.61,87.60) -> detour_reason=start_off_mesh. The bounded water start box
	// reaches at most 2.8 m up (WaterQueryBounds) and the lowest polygon over that point is the deck at 93.4: no start
	// polygon, whatever the native water plane is. This is why the path never starts there.
	{
		const float start[3] = {2116.75f, 1882.61f, 87.60f};
		for (float lift : {1.5f, 6.f, 11.9f})
		{
			const NavQueryBounds b = WaterQueryBounds(lift);
			CHECK(b.up <= 2.8f);
			std::vector<float> xyz; std::vector<NavPathLink> links; const char* why = "";
			CHECK(!m.FindPathLinks(start, w, xyz, links, &why, &b));
			std::printf("start lift %.1f -> %s\n", lift, why);
			CHECK(std::string(why) == "start_off_mesh");
		}
		CHECK(m.SolidAbove(start));
	}
	// Codex #41 re-review 1bae48b5: both real swim-outs timed out deep under open sky - owner (2118.8,1898.0,78.7),
	// server B (2118.6,1898.1,78.1), surface 99.5. There nothing is overhead (SubSwimVertical asks the native ascent),
	// the deep start does not reach the open-water target, and a start near the surface over the same point does: that
	// reach is the physical success of the swim-out, so only the ascent can end it.
	{
		const float deep[2][3] = {{2118.8f, 1898.0f, 78.7f}, {2118.6f, 1898.1f, 78.1f}};
		for (const auto& d : deep)
		{
			CHECK(!m.SolidAbove(d));
			CHECK(SubSwimVertical(m.SolidAbove(d), 99.5f, d[2]) == 1);
			const NavQueryBounds low = WaterQueryBounds(99.5f - d[2]);
			const bool deepReach = m.Reachable(d, w, 2.6f, &low);
			const float top[3] = {d[0], d[1], 98.2f};
			const NavQueryBounds near = WaterQueryBounds(99.5f - top[2]);
			const bool topReach = m.Reachable(top, w, 2.6f, &near);
			std::printf("deep (%.1f,%.1f,%.1f) reach=%d, surface 98.2 reach=%d\n", d[0], d[1], d[2], deepReach ? 1 : 0, topReach ? 1 : 0);
			CHECK(!deepReach);
			CHECK(topReach);
			CHECK(SubSwimVertical(m.SolidAbove(top), 99.5f, top[2]) == 0);   // released 1.3 m under the surface
		}
		CHECK(SubSwimVertical(m.SolidAbove(pit), 99.5f, pit[2]) == -1);       // under the deck: descend
	}
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
