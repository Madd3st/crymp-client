// Hold fire while an ally is near the real fire line before the target (Shore 03.11, WarsBot_10 -> WarsBot_12).
#include "CryGame/Bots/BotFireLane.h"
#include <cassert>
#include <cstdio>
#include <utility>
using namespace WarsBots;

int main()
{
	const LaneVec muzzle{0.f, 0.f, 1.5f}, east{1.f, 0.f, 0.f};
	auto body = [](float x, float y) { return std::pair<LaneVec, LaneVec>{LaneVec{x, y, 0.f}, LaneVec{x, y, 1.7f}}; };
	// Ally straight on the line, half way to a target 20 m away.
	auto a = body(10.f, 0.f);
	assert(FriendlyInLane(muzzle, east, 20.f, a.first, a.second));
	// Ally 0.6 m to the side at 10 m: inside the lane widened by spread (0.5 + 0.3 m).
	a = body(10.f, 0.6f);
	assert(FriendlyInLane(muzzle, east, 20.f, a.first, a.second));
	// Ally 1 m ahead, 0.6 m beside the line: the shoulder is hit (Shore server run, 0.52 m from the axis).
	a = body(1.f, 0.6f);
	assert(FriendlyInLane(muzzle, east, 16.f, a.first, a.second));
	// Ally 2 m to the side: clear.
	a = body(10.f, 2.f);
	assert(!FriendlyInLane(muzzle, east, 20.f, a.first, a.second));
	// Ally behind the target: the shot stops at the target.
	a = body(25.f, 0.f);
	assert(!FriendlyInLane(muzzle, east, 20.f, a.first, a.second));
	// Ally shoulder to shoulder or behind the shooter: not in the lane.
	a = body(0.f, 0.4f);
	assert(!FriendlyInLane(muzzle, east, 20.f, a.first, a.second));
	a = body(-3.f, 0.f);
	assert(!FriendlyInLane(muzzle, east, 20.f, a.first, a.second));
	// Uphill shot: at the crouching ally (head at 1.0 m) the line is 2.5 m higher.
	const LaneVec up{0.894f, 0.f, 0.447f};
	assert(!FriendlyInLane(muzzle, up, 20.f, LaneVec{4.f, 0.f, 0.f}, LaneVec{4.f, 0.f, 1.0f}));
	// Downhill shot through a standing ally's head.
	const LaneVec down{0.995f, 0.f, -0.0995f};
	assert(FriendlyInLane(muzzle, down, 20.f, LaneVec{5.f, 0.f, -0.5f}, LaneVec{5.f, 0.f, 1.2f}));
	// Zero range never blocks.
	a = body(1.f, 0.f);
	assert(!FriendlyInLane(muzzle, east, 0.f, a.first, a.second));
	std::puts("BotFireLaneTests OK");
	return 0;
}
