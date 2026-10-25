#include "CryGame/Bots/BotJumpLink.h"
#include <cassert>
#include <cstdio>
using namespace WarsBots;

static LinkInput At(float t, float x, float y, float z, bool grounded = true, float speed = 4.f)
{ LinkInput in; in.now = t; in.feet = {x, y, z}; in.grounded = grounded; in.speed = speed; return in; }

int main()
{
	const JVec a{0, 0, 10}, b{3, 0, 7};
	// 1. Drop: approach a, walk off past b, fall, land near b -> Done; never a jump request.
	{
		BotJumpLink L; L.Start(0, LinkMove::Drop, a, b);
		LinkOutput o = L.Update(At(0, -3, 0, 10)); assert(o.phase == LinkPhase::Approach && o.moveTo.x == 0.f);
		o = L.Update(At(0.5f, -0.3f, 0, 10)); assert(o.phase == LinkPhase::Act && o.moveTo.x > 3.f && o.jump == LinkJumpRequest::None);
		o = L.Update(At(0.7f, 0.5f, 0, 9.5f, false)); assert(o.phase == LinkPhase::Air);
		o = L.Update(At(1.2f, 2.8f, 0, 7, true)); assert(o.phase == LinkPhase::Done && !L.Active());
	}
	// 2. Jump running in with Armor: takes off at a on the same tick, no stop.
	{
		BotJumpLink L; L.Start(0, LinkMove::Jump, a, JVec{3, 0, 11});
		LinkOutput o = L.Update(At(0, -0.5f, 0, 10)); assert(o.phase == LinkPhase::Act && o.jump == LinkJumpRequest::Normal);
		o = L.Update(At(0.3f, 1, 0, 11, false)); assert(o.phase == LinkPhase::Air && o.jump == LinkJumpRequest::None);
		o = L.Update(At(0.8f, 3, 0, 11, true)); assert(o.phase == LinkPhase::Done);
	}
	// 3. Strength not ready at a: stop (Prepare), suit ready -> back off 2.5 m (RunUp), run in, jump at a.
	{
		BotJumpLink L; L.Start(0, LinkMove::Strength, a, JVec{5, 0, 14});
		LinkInput in = At(0, -0.5f, 0, 10); in.energy = 150; LinkOutput o = L.Update(in);
		assert(o.phase == LinkPhase::Prepare && o.wantStrength && o.jump == LinkJumpRequest::None);
		in.now = 0.5f; in.speed = 0; in.strengthActive = true; o = L.Update(in);
		assert(o.phase == LinkPhase::RunUp && o.moveTo.x < -2.f);
		in = At(1.0f, -2.6f, 0, 10, true, 1.f); in.energy = 150; in.strengthActive = true; o = L.Update(in);
		assert(o.phase == LinkPhase::Act && o.jump == LinkJumpRequest::None);           // still 2.6 m before a
		in = At(1.5f, -0.4f, 0, 10); in.energy = 150; in.strengthActive = true; o = L.Update(in);
		assert(o.jump == LinkJumpRequest::Strength);
	}
	// 4. Strength with low energy waits for regen; still too low after prepareMax -> NoEnergy (not banned by the caller).
	{
		BotJumpLink M; M.Start(0, LinkMove::Strength, a, JVec{5, 0, 14});
		LinkInput low = At(0, -0.1f, 0, 10); low.energy = 30; LinkOutput o = M.Update(low); assert(o.phase == LinkPhase::Prepare);
		low.now = 4.5f; o = M.Update(low); assert(o.phase == LinkPhase::Failed && o.fail == LinkFail::NoEnergy);
	}
	// 5. Jump refused by the game (never airborne) -> NoTakeoff; landing far below b -> MissedLanding; timeout.
	{
		BotJumpLink L; L.Start(0, LinkMove::Jump, a, JVec{3, 0, 11});
		L.Update(At(0, 0, 0, 10)); LinkOutput o = L.Update(At(2.5f, 0.2f, 0, 10)); assert(o.fail == LinkFail::NoTakeoff);
		BotJumpLink M; M.Start(0, LinkMove::Drop, a, b);
		M.Update(At(0, 0, 0, 10)); M.Update(At(0.2f, 0.5f, 0, 9, false));
		M.Update(At(0.5f, 2.8f, 0, 2, true)); o = M.Update(At(1.4f, 2.8f, 0, 2, true)); assert(o.fail == LinkFail::MissedLanding);
		BotJumpLink N; N.Start(0, LinkMove::Drop, a, b); N.Update(At(0, -20, 0, 10)); o = N.Update(At(10, -19, 0, 10)); assert(o.fail == LinkFail::Timeout);
	}
	// 6. Approach that stalls 0.62 m short of a (c061) still goes on after stillTime.
	{
		BotJumpLink L; L.Start(0, LinkMove::Drop, a, b);
		L.Update(At(0, -1.5f, 0, 10, true, 0.f));
		LinkOutput o = L.Update(At(0.5f, -1.5f, 0, 10, true, 0.f)); assert(o.phase == LinkPhase::Act);
	}
	std::puts("BotJumpLinkTests OK");
	return 0;
}
