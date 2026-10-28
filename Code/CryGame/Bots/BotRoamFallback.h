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

// DS-092: a native swimmer's local roam leg is a WATER leg. Both the 4 samples *and* the committed goal must be
// built at the qualified native water plane, never at the feet z: NavMeshCore projects a path goal with the dry goal
// box (up 1.0), so a goal 1.4-2.3 m below the baked surface is unreachable even though the leg is open water
// (C147 feet 97.2..98.6 vs plane 99.511). Only the already-gated `waterStart` fact (waterMovement + valid plane +
// lift > 0.05, see BotBrain's WaterStartBounds) may lift it; dry callers keep the feet z byte-identically.
inline float RoamGoalZ(bool waterStart, float feetZ, float waterZ)
{
	return waterStart ? waterZ : feetZ;
}
}
