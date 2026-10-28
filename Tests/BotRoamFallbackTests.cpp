// DS-082 B1 regression: local roam fallback sampling. Full/3-4/1-2/1-4/none and no-mesh cases; the old code always
// returned the full leg, which produced the C145 US NAV fallback/stuck window.
#include "CryGame/Bots/BotRoamFallback.h"
#include <cassert>
#include <cstdio>

using namespace WarsBots;

int main()
{
	{
		const bool r[4] = { true, false, false, false };
		assert(RoamFallbackFraction(true, r) == 1.0f); // full leg
	}
	{
		const bool r[4] = { false, true, false, false };
		assert(RoamFallbackFraction(true, r) == 0.75f); // blocked at full length
	}
	{
		const bool r[4] = { false, false, true, false };
		assert(RoamFallbackFraction(true, r) == 0.5f);
	}
	{
		const bool r[4] = { false, false, false, true };
		assert(RoamFallbackFraction(true, r) == 0.25f); // only the nearest quarter is on the part
	}
	{
		const bool r[4] = { false, false, false, false };
		assert(RoamFallbackFraction(true, r) == 0.0f); // hold this leg, never steer into the hull
	}
	{
		// The first reachable fraction wins even when a closer sample is also reachable.
		const bool r[4] = { false, true, true, true };
		assert(RoamFallbackFraction(true, r) == 0.75f);
	}
	{
		// No navmesh: legacy full-leg behaviour, no probes.
		const bool r[4] = { false, false, false, false };
		assert(RoamFallbackFraction(false, r) == 1.0f);
	}
	{
		// DS-092: only the gated native-water fact lifts the local goal to the water plane; dry keeps the feet z.
		assert(RoamGoalZ(false, 98.0f, 99.5f) == 98.0f);
		assert(RoamGoalZ(true, 98.0f, 99.5f) == 99.5f);
		assert(RoamGoalZ(true, 97.2f, 99.59f) == 99.59f);
	}

	std::puts("BotRoamFallbackTests OK");
	return 0;
}
