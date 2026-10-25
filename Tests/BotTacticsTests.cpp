#include "CryGame/Bots/BotTactics.h"
#include <cassert>
#include <cstdio>
using namespace WarsBots;
int main()
{
	// traits
	assert(TraitJumper(3, 3) > TraitJumper(0, 4) && TraitJumper(3, 1) <= 0.9f && TraitHolder(4) > TraitHolder(1));
	// cover: threat north (+Y) at 20 m; spot A protected toward north, spot B not, spot C too far, D can't see
	const TVec bot{0, 0, 0}, threat{0, 20, 0};
	CoverCandidate c[4];
	c[0].p = {3, 2, 0}; c[0].coverMask = 1u << 0;            // wall to the north
	c[1].p = {1, 1, 0}; c[1].coverMask = 0;
	c[2].p = {30, 0, 0}; c[2].coverMask = 1u;
	c[3].p = {-2, 2, 0}; c[3].coverMask = 1u;
	int i = PickCover(c, 4, bot, threat, 15.f, [&](const TVec& p) { return !(p.x < -1.f); });
	assert(i == 0);
	i = PickCover(c, 4, bot, threat, 15.f, [&](const TVec& p) { return p.x > 0.5f && p.x < 2.f; });
	assert(i == 1);                                           // only B sees: B
	assert(PickCover(c, 4, bot, threat, 15.f, [](const TVec&) { return false; }) == -1);
	// hold episode
	BotAngleHold h;
	assert(!h.Begin(7, 0, 5.f, 20.f, 1.f, 1));                // seen too long ago
	BotAngleHold g;
	assert(g.Begin(9, 0, 1.f, 20.f, 1.f, 1));
	g.Assign(c[0].p, 0, 3.f);
	assert(g.Update(1, TVec{0, 0, 0}, false) == HoldPhase::Move);
	assert(g.Update(2, TVec{3, 2.5f, 0}, false) == HoldPhase::Hold);
	assert(g.Update(4, TVec{3, 2.5f, 0}, false) == HoldPhase::Hold);
	assert(g.Update(5.5f, TVec{3, 2.5f, 0}, false) == HoldPhase::Done);
	assert(!g.Begin(9, 6, 1.f, 20.f, 1.f, 1));               // one episode per target
	BotAngleHold k; k.Begin(3, 0, 1.f, 20.f, 1.f, 1); k.Assign(c[0].p, 0, 3.f);
	assert(k.Update(0.5f, bot, true) == HoldPhase::Done);     // target reappears -> fight
	// jump-strafe: never airborne / out of range; respects the timer
	BotJumpStrafe j; assert(!j.Want(0, false, 10, 1, 5));
	assert(j.Want(0, true, 10, 1.f, 5) && !j.Want(0.5f, true, 10, 1.f, 5));
	assert(!BotJumpStrafe().Want(0, true, 50, 1.f, 5));
	std::puts("BotTacticsTests OK");
	return 0;
}
