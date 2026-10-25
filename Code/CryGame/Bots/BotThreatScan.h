// Threat glance (wb_bot_threat_scan, owner 25.10: he held roofs with a sniper rifle for whole matches and no bot ever
// looked up). Without a visible target a bot now and then glances for ~0.6 s at a high sniper spot (tactical_spots
// flag 4, the places a human camps) that is in front of it, 10-70 m away and clearly above the eye. The brain checks
// the eye ray (no looking through walls) and times the glances per skill; Beginner never does it.
// Pure logic, no engine types; Tests/BotThreatScanTests.cpp.
#pragma once
#include <cmath>

namespace WarsBots {

struct ThreatScanParams {
	float minDist = 10.f, maxDist = 70.f;  // m, horizontal
	float minRise = 2.5f;                  // m the spot (standing eye) must be above our eye
	float maxOffYaw = 100.f;               // deg from the travel/view direction (never look behind)
	float glance = 0.6f;                   // s
};

// seconds between glances per skill (0 beginner .. 3 elite); <= 0 = never
inline float ThreatScanPeriod(int skill) { static const float p[] = {0.f, 8.f, 5.f, 3.5f}; return skill >= 0 && skill < 4 ? p[skill] : 8.f; }

// Fills out[] with up to maxOut indices of spots worth a glance (xyz triples in spots[]), best first: the one closest
// to the travel direction, ties broken by distance. Returns the count.
inline int ThreatScanCandidates(const float* spots, int count, float ex, float ey, float ez, float travelYawDeg,
	int* out, int maxOut, const ThreatScanParams& p = ThreatScanParams())
{
	int n = 0; float key[16];
	if (maxOut > 16) maxOut = 16;
	for (int i = 0; i < count; ++i)
	{
		const float x = spots[i * 3], y = spots[i * 3 + 1], z = spots[i * 3 + 2] + 1.6f;
		const float dx = x - ex, dy = y - ey, d = std::sqrt(dx * dx + dy * dy);
		if (d < p.minDist || d > p.maxDist || z - ez < p.minRise) continue;
		float off = std::atan2(dx, dy) * 57.2957795f - travelYawDeg;
		off = std::fmod(off + 540.f, 360.f) - 180.f;
		if (std::fabs(off) > p.maxOffYaw) continue;
		const float k = std::fabs(off) + d * 0.2f;
		int j = n < maxOut ? n++ : maxOut;           // insertion into the sorted top list
		if (j == maxOut) { if (k >= key[maxOut - 1]) continue; j = maxOut - 1; }
		while (j > 0 && key[j - 1] > k) { key[j] = key[j - 1]; out[j] = out[j - 1]; --j; }
		key[j] = k; out[j] = i;
	}
	return n;
}

}
