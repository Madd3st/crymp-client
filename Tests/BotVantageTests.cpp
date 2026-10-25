#include "CryGame/Bots/BotVantage.h"
#include <cassert>
#include <cstdio>
using namespace WarsBots;
int main()
{
	const char* text =
		"# header\n"
		"0 0 10 4 0 0 0 100 90\n"       // sniper, view 100, looks east
		"3 0 10 4 0 0 0 90 90\n"        // same cluster (3 m)
		"50 0 10 4 0 0 0 80 180\n"      // far one
		"10 10 0 1 0 0 0 500 0\n";      // cover only: not a vantage spot
	VantageMap map; assert(map.Load(text) == 3);
	auto all = [](const VSpot&) { return true; };
	// 1. Bot 1 picks the best nearby spot, travels, arrives, holds looking along yaw 90.
	BotVantage a; VantageOutput o = a.Update(map, 1, 0, 0, -10, 10, 7, all);
	assert(o.phase == VantagePhase::Travel && o.x == 0.f && o.move);
	o = a.Update(map, 1, 5, 0.5f, 0, 10, 7, all); assert(o.phase == VantagePhase::Hold && !o.move && o.lookYaw == 90.f);
	// 2. Bot 2 cannot take the same cluster -> the far spot.
	BotVantage b; o = b.Update(map, 2, 5, 0, -10, 10, 3, all); assert(o.x == 50.f);
	// 3. Two kills from the spot -> relocate; the whole cluster is banned and bot 2 holds the far one -> nothing left.
	a.OnKill(); a.OnKill(); o = a.Update(map, 1, 6, 0.5f, 0, 10, 7, all); assert(o.relocated && o.phase == VantagePhase::None);
	// 4. Bot 2 hit on its spot -> relocates once it holds there.
	o = b.Update(map, 2, 7, 50, 0, 10, 3, all); assert(o.phase == VantagePhase::Hold);
	b.OnHit(8); o = b.Update(map, 2, 8.5f, 50, 0, 10, 3, all); assert(o.relocated);
	// ... and after the ban the first cluster is available again for bot 1
	o = a.Update(map, 1, 70, 0, 0, 10, 7, all); assert(o.phase != VantagePhase::None);
	// 5. Unreachable spots are skipped.
	BotVantage c; VantageMap m2; m2.Load(text);
	o = c.Update(m2, 3, 0, 0, 0, 10, 1, [](const VSpot& s) { return s.x > 10.f; }); assert(o.x == 50.f);
	std::puts("BotVantageTests OK");
	return 0;
}
