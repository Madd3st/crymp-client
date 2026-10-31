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
		BotJumpLink L; L.Start(0, LinkMove::Strength, a, JVec{5, 0, 13.5});
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
		BotJumpLink M; M.Start(0, LinkMove::Strength, a, JVec{5, 0, 13.5});
		LinkInput low = At(0, -0.1f, 0, 10); low.energy = 30; LinkOutput o = M.Update(low); assert(o.phase == LinkPhase::Prepare);
		low.now = 6.5f; o = M.Update(low);   // DS-036: prepareMax 6 s assert(o.phase == LinkPhase::Failed && o.fail == LinkFail::NoEnergy);
	}
	// 5. Jump refused by the game (never airborne) -> NoTakeoff; landing far below b -> MissedLanding; timeout.
	{
		BotJumpLink L; L.Start(0, LinkMove::Jump, a, JVec{3, 0, 11});
		L.Update(At(0, 0, 0, 10)); LinkOutput o = L.Update(At(2.5f, 0.2f, 0, 10)); assert(o.phase == LinkPhase::Act); // DS-033: 3.5 s window
		o = L.Update(At(4.0f, 0.2f, 0, 10)); assert(o.fail == LinkFail::NoTakeoff);
		BotJumpLink M; M.Start(0, LinkMove::Drop, a, b);
		M.Update(At(0, 0, 0, 10)); M.Update(At(0.2f, 0.5f, 0, 9, false));
		M.Update(At(0.5f, 2.8f, 0, 2, true)); o = M.Update(At(1.4f, 2.8f, 0, 2, true)); assert(o.fail == LinkFail::MissedLanding);
		BotJumpLink N; N.Start(0, LinkMove::Drop, a, b); N.Update(At(0, -20, 0, 10)); o = N.Update(At(13, -19, 0, 10)); assert(o.fail == LinkFail::Timeout);   // total 12 s
	}
	// 6. Approach that stalls 0.62 m short of a (c061) still goes on after stillTime.
	{
		BotJumpLink L; L.Start(0, LinkMove::Drop, a, b);
		L.Update(At(0, -1.5f, 0, 10, true, 0.f));
		LinkOutput o = L.Update(At(0.5f, -1.5f, 0, 10, true, 0.f)); assert(o.phase == LinkPhase::Act);
	}
	// 7. DS-048: blocked 1 m short of the take-off (railing/step) fails fast instead of waiting 3.5 s and retrying.
	{
		LinkParams fp; fp.blockedFailFast = true; BotJumpLink L(fp); L.Start(0, LinkMove::Strength, JVec{0, 0, 0}, JVec{10, 0, 0});
		LinkInput in = At(0, -1, 0, 0, true, 0); in.energy = 200; in.strengthActive = true; L.Update(in);
		in = At(0.5f, -1, 0, 0, true, 0); in.energy = 200; in.strengthActive = true; L.Update(in);
		in = At(1.8f, -1, 0, 0, true, 0); in.energy = 200; in.strengthActive = true; LinkOutput o = L.Update(in);
		assert(o.phase == LinkPhase::Act && o.jump == LinkJumpRequest::None);
		in = At(2.5f, -1, 0, 0, true, 0); in.energy = 200; in.strengthActive = true; o = L.Update(in);
		assert(o.phase == LinkPhase::Failed && o.fail == LinkFail::NoTakeoff);
	}
	// 8. Steelmill roof landing: multi-tier roof/catwalk landing within landDzStrength (e.g. z=151.91 vs 150.04, dz=1.87 m)
	// succeeds for Strength jumps, and long air flights (> 0.6 s) do not prematurely trigger MissedLanding on touchdown.
	{
		const JVec roofA{1340.64f, 1351.55f, 151.84f}, roofB{1332.83f, 1346.08f, 150.04f};
		BotJumpLink L; L.Start(0, LinkMove::Strength, roofA, roofB);
		LinkInput in = At(0, 1340.f, 1351.f, 151.84f, true, 4.f); in.energy = 180; in.strengthActive = true;
		LinkOutput o = L.Update(in);
		assert(o.phase == LinkPhase::Act);
		// Air flight of 1.9s (t = 0.2 to 2.1)
		o = L.Update(At(0.2f, 1338.f, 1349.f, 154.5f, false)); assert(o.phase == LinkPhase::Air);
		o = L.Update(At(1.5f, 1336.f, 1348.f, 153.5f, false)); assert(o.phase == LinkPhase::Air);
		// Touchdown at t=2.1s (1.9s after entering Air): landed on roof deck z=151.91, 1.58 m from B in 2D
		in = At(2.1f, 1334.28f, 1346.72f, 151.91f, true); in.speed = 3.5f;
		o = L.Update(in);
		assert(o.phase == LinkPhase::Done && !L.Active()); // dz=1.87 m <= 2.5 m -> Done

		// Roll-in after touchdown: landing just outside landRadius (3.4 m) gives 0.6 s window to roll to B
		BotJumpLink M; M.Start(0, LinkMove::Strength, roofA, roofB);
		in = At(0, 1340.f, 1351.f, 151.84f, true, 4.f); in.energy = 180; in.strengthActive = true;
		M.Update(in);
		M.Update(At(0.2f, 1338.f, 1349.f, 154.5f, false));
		// Touchdown at t=2.0s: landed 3.4 m away (outside 3.0 m radius)
		in = At(2.0f, 1332.83f + 3.4f, 1346.08f, 150.04f, true);
		o = M.Update(in);
		assert(o.phase == LinkPhase::Air); // Still Air, waiting for roll or timeout
		// At t=2.3s (0.3s after touchdown), rolled within 1.5 m of B
		in = At(2.3f, 1332.83f + 1.5f, 1346.08f, 150.04f, true);
		o = M.Update(in);
		assert(o.phase == LinkPhase::Done && !M.Active());

		// But if bot stays > 3.0 m away for > 0.6 s after touchdown, fails MissedLanding
		BotJumpLink N; N.Start(0, LinkMove::Strength, roofA, roofB);
		in = At(0, 1340.f, 1351.f, 151.84f, true, 4.f); in.energy = 180; in.strengthActive = true;
		N.Update(in);
		N.Update(At(0.2f, 1338.f, 1349.f, 154.5f, false));
		N.Update(At(2.0f, 1332.83f + 3.4f, 1346.08f, 150.04f, true));
		o = N.Update(At(2.7f, 1332.83f + 3.4f, 1346.08f, 150.04f, true)); // 0.7s after landing
		assert(o.phase == LinkPhase::Failed && o.fail == LinkFail::MissedLanding);
	}
	std::puts("BotJumpLinkTests OK");
	return 0;
}

