// DS-082 B1: local roam fallback sampling, engine-free.
//
// The 6-10 m golden-angle point at BotBrain::UpdateRoam's last resort is not a path request. When it lands outside
// the bot's nav part (through a hull wall or in an off-mesh pocket) the old code steered straight at it for the 1.5 s
// blocked window (C145 US run t173..179: NAV fallback/stuck). The adapter samples the leg at 1/4, 1/2, 3/4 and full
// length with the existing NavMeshCore::Reachable and walks only the reachable fraction; 0 means hold this leg.
// Without a navmesh the old full-leg behaviour is preserved (no extra probes).
#pragma once

namespace WarsBots
{
// reachable[0] = full local point, [1] = 3/4, [2] = 1/2, [3] = 1/4 of the leg (from the bot's feet).
// Returns 1.0, 0.75, 0.5, 0.25 or 0.0 (hold). meshAvailable=false returns 1.0 (legacy behaviour).
inline float RoamFallbackFraction(bool meshAvailable, const bool reachable[4])
{
	if (!meshAvailable) return 1.0f;
	for (int i = 0; i < 4; ++i)
		if (reachable[i]) return 1.0f - 0.25f * float(i);
	return 0.0f;
}
}
