// Shore playtest 02.11: a bot opened the Base_NK gate 930 although its navmesh path only rounded the end of the closed
// panel; the panel then slid over that path and 7 bots stayed behind it. Open a door only when the path goes through
// its central part. Coordinates are the door's local X/Y, the box is its closed model.
// Pure logic, no engine types; Tests/BotDoorPathTests.cpp.
#pragma once
#include <algorithm>

namespace WarsBots {

inline bool SegmentCrossesDoor(float ax, float ay, float bx, float by, float minX, float minY, float maxX, float maxY,
	float keepWidth = 0.75f)
{
	const float shrinkX = maxX - minX >= maxY - minY ? (maxX - minX) * (1.f - keepWidth) * 0.5f : 0.f;
	const float shrinkY = maxY - minY > maxX - minX ? (maxY - minY) * (1.f - keepWidth) * 0.5f : 0.f;
	const float lo[2] = {minX + shrinkX, minY + shrinkY}, hi[2] = {maxX - shrinkX, maxY - shrinkY};
	const float from[2] = {ax, ay}, delta[2] = {bx - ax, by - ay};
	float enter = 0.f, leave = 1.f;
	for (int i = 0; i < 2; ++i)
	{
		if (delta[i] == 0.f) { if (from[i] < lo[i] || from[i] > hi[i]) return false; continue; }
		const float t0 = (lo[i] - from[i]) / delta[i], t1 = (hi[i] - from[i]) / delta[i];
		enter = std::max(enter, std::min(t0, t1));
		leave = std::min(leave, std::max(t0, t1));
		if (enter > leave) return false;
	}
	return true;
}

} // namespace WarsBots
