// Doors open only for a path through them (Shore gate 930, 6 m panel, local Y along the panel).
#include "CryGame/Bots/BotDoorPath.h"
#include <cassert>
#include <cstdio>
using namespace WarsBots;

int main()
{
	const float minX = -0.2f, minY = -3.f, maxX = 0.2f, maxY = 3.f;
	// Straight through the middle.
	assert(SegmentCrossesDoor(-2.f, 0.f, 2.f, 0.f, minX, minY, maxX, maxY));
	// Oblique through the panel, far from the ends.
	assert(SegmentCrossesDoor(-2.f, 1.f, 2.f, -1.f, minX, minY, maxX, maxY));
	// Playtest path: along the inner side, then around the panel end. Neither corner segment opens the gate.
	assert(!SegmentCrossesDoor(-1.8f, 2.7f, -0.4f, -2.8f, minX, minY, maxX, maxY));
	assert(!SegmentCrossesDoor(-0.4f, -2.8f, 2.4f, -2.5f, minX, minY, maxX, maxY));
	// Stops before the door, or passes behind it.
	assert(!SegmentCrossesDoor(-3.f, 0.f, -1.f, 0.f, minX, minY, maxX, maxY));
	assert(!SegmentCrossesDoor(1.f, -4.f, 1.f, 4.f, minX, minY, maxX, maxY));
	// A narrow 1.5 m door: the doorway centre still counts, whatever axis is the long one.
	assert(SegmentCrossesDoor(0.3f, -2.f, 0.3f, 2.f, -0.75f, -0.1f, 0.75f, 0.1f));
	assert(!SegmentCrossesDoor(0.7f, -2.f, 0.7f, 2.f, -0.75f, -0.1f, 0.75f, 0.1f));
	std::puts("BotDoorPathTests OK");
	return 0;
}
