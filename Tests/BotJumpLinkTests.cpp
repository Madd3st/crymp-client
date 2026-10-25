#include "CryGame/Bots/BotJumpLink.h"
#include <cassert>
#include <cstdio>
using namespace WarsBots;

static LinkInput At(float t, float x, float y, float z, bool grounded = true) { LinkInput in; in.now = t; in.feet = {x, y, z}; in.grounded = grounded; return in; }

int main()
{
	const JVec a{0, 0, 10}, b{3, 0, 7};
	// 1. Drop: approach a, walk off, fall, land near b -> Done; never a jump request.
	{
		BotJumpLink L; L.Start(0, LinkMove::Drop, a, b);
		LinkOutput o = L.Update(At(0, -3, 0, 10)); assert(o.phase == LinkPhase::Approach && o.moveTo.x == 0.f);
		o = L.Update(At(0.5f, -0.3f, 0, 10)); assert(o.moveTo.x == 3.f && o.jump == LinkJumpRequest::None);
		o = L.Update(At(0.7f, 0.5f, 0, 9.5f, false)); assert(o.phase == LinkPhase::Air);
		o = L.Update(At(1.2f, 2.8f, 0, 7, true)); assert(o.phase == LinkPhase::Done && !L.Active());
	}
	// 2. Jump: waits for Armor, then requests a normal jump until airborne.
	{
		BotJumpLink L; L.Start(0, LinkMove::Jump, a, JVec{3, 0, 11});
		LinkInput in = At(0, -0.2f, 0, 10); in.armorActive = false;
		LinkOutput o = L.Update(in); assert(o.wantArmor && o.jump == LinkJumpRequest::None);
		o = L.Update(in); assert(o.phase == LinkPhase::Prepare);
		in.now = 0.3f; in.armorActive = true; o = L.Update(in); assert(o.phase == LinkPhase::Act && o.jump == LinkJumpRequest::Normal);
		o = L.Update(At(0.5f, 1, 0, 11, false)); assert(o.phase == LinkPhase::Air && o.jump == LinkJumpRequest::None);
		o = L.Update(At(1.0f, 3, 0, 11, true)); assert(o.phase == LinkPhase::Done);
	}
	// 3. Strength: asks for Strength from the approach on, jumps only when active with energy.
	{
		BotJumpLink L; L.Start(0, LinkMove::Strength, a, JVec{5, 0, 14});
		LinkInput in = At(0, -2, 0, 10); in.energy = 150; LinkOutput o = L.Update(in); assert(o.wantStrength);
		in = At(0.4f, -0.2f, 0, 10); in.energy = 150; o = L.Update(in); assert(o.phase == LinkPhase::Prepare && o.jump == LinkJumpRequest::None);
		in.now = 0.8f; in.strengthActive = true; o = L.Update(in); assert(o.jump == LinkJumpRequest::Strength);
	}
	// 4. Strength without energy fails at once (caller re-paths, nothing banned for energy).
	{
		BotJumpLink L; L.Start(0, LinkMove::Strength, a, JVec{5, 0, 14});
		LinkInput in = At(0, -0.1f, 0, 10); in.energy = 30;
		LinkOutput o = L.Update(in); assert(o.phase == LinkPhase::Failed && o.fail == LinkFail::NoEnergy);
	}
	// 5. Jump refused by the game (never airborne) -> NoTakeoff; landing far from b -> MissedLanding; timeout.
	{
		BotJumpLink L; L.Start(0, LinkMove::Jump, a, JVec{3, 0, 11});
		L.Update(At(0, 0, 0, 10)); LinkOutput o = L.Update(At(2.f, 0, 0, 10)); assert(o.fail == LinkFail::NoTakeoff);
		BotJumpLink M; M.Start(0, LinkMove::Drop, a, b);
		M.Update(At(0, 0, 0, 10)); M.Update(At(0.2f, 0.5f, 0, 9, false));
		M.Update(At(0.5f, 0.6f, 0, 2, true)); o = M.Update(At(1.4f, 0.6f, 0, 2, true)); assert(o.fail == LinkFail::MissedLanding);
		BotJumpLink N; N.Start(0, LinkMove::Drop, a, b); N.Update(At(0, -20, 0, 10)); o = N.Update(At(7, -19, 0, 10)); assert(o.fail == LinkFail::Timeout);
	}
	std::puts("BotJumpLinkTests OK");
	return 0;
}
