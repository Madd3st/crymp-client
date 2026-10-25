#include "CryGame/Bots/BotThreatScan.h"
#include <cassert>
#include <cstdio>
using namespace WarsBots;
int main()
{
	// eye at (0,0,1.6), walking north (yaw 0)
	const float spots[] = {
		0, 30, 12,     // 0: ahead, 30 m, high -> best
		20, 20, 12,    // 1: 45 deg right, high
		0, -30, 12,    // 2: behind -> never
		0, 5, 12,      // 3: too close
		0, 100, 12,    // 4: too far
		10, 30, 0.5f,  // 5: ahead but not above the eye
		-25, 25, 15,   // 6: 45 deg left, high, farther
	};
	int out[4];
	const int n = ThreatScanCandidates(spots, 7, 0, 0, 1.6f, 0.f, out, 4);
	assert(n == 3 && out[0] == 0);
	for (int i = 0; i < n; ++i) assert(out[i] != 2 && out[i] != 3 && out[i] != 4 && out[i] != 5);
	// facing east (yaw 90): #1 (north-east) is closest to the view; #0 and #2 are 90 deg to the side (allowed),
	// #6 (north-west, 135 deg off) drops out
	const int m = ThreatScanCandidates(spots, 7, 0, 0, 1.6f, 90.f, out, 4);
	assert(m == 3 && out[0] == 1);
	for (int i = 0; i < m; ++i) assert(out[i] != 6);
	assert(ThreatScanPeriod(0) <= 0.f && ThreatScanPeriod(3) < ThreatScanPeriod(1));
	std::puts("BotThreatScanTests OK");
	return 0;
}
