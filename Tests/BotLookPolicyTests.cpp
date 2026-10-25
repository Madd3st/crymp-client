#include "CryGame/Bots/BotLookPolicy.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace WarsBots;

static LookInput Base(float t)
{
	LookInput in; in.now = t; in.eye = {0, 0, 1.6f}; in.speed = 4.5f;
	in.hasCorner = true; in.corner = {0, 10, 0};          // walking north
	return in;
}
static bool Near(float a, float b, float tol = 1.f) { return std::fabs(BotLookPolicy::Wrap(a - b)) <= tol; }

int main()
{
	// 1. No target, interest behind a wall (ray blocked) -> look along the path, not at the interest.
	{
		BotLookPolicy p; LookInput in = Base(0); in.hasInterest = true; in.interest = {10, 0, 1}; in.interestRayClear = false;
		LookOutput o = p.Update(in);
		assert(o.reason == LookReason::Path && Near(o.yaw, 0.f));
	}
	// 2. Interest with a clear ray, 45 deg off travel -> look at it; far behind (150 deg) while moving -> path.
	{
		BotLookPolicy p; LookInput in = Base(0); in.hasInterest = true; in.interest = {10, 10, 1}; in.interestRayClear = true;
		LookOutput o = p.Update(in); assert(o.reason == LookReason::Interest && Near(o.yaw, 45.f));
		BotLookPolicy q; LookInput b = Base(0); b.hasInterest = true; b.interest = {5, -9, 1}; b.interestRayClear = true;
		o = q.Update(b); assert(o.reason == LookReason::Path);
	}
	// 3. Close range does not bypass an occluding wall.
	{
		BotLookPolicy p; LookInput in = Base(0); in.hasInterest = true; in.interest = {2, 0, 1}; in.interestRayClear = false;
		assert(p.Update(in).reason == LookReason::Path);
        in.now=1.f;in.interestRayClear=true;assert(p.Update(in).reason==LookReason::Interest);
	}
	// 4. Visible target overrides everything immediately (no hysteresis), damage glance next.
	{
		BotLookPolicy p; LookInput in = Base(0); p.Update(in);
		in.now = 0.05f; in.targetVisible = true; in.target = {-10, 0, 1.6f};
		LookOutput o = p.Update(in); assert(o.reason == LookReason::Target && Near(o.yaw, -90.f));
		LookInput d = Base(0.1f); d.sinceDamaged = 0.2f; d.attackerKnown = true; d.attacker = {0, -20, 1.6f};
		o = p.Update(d); assert(o.reason == LookReason::Attacker && Near(o.yaw, 180.f));
		d.now = 2.f; d.sinceDamaged = 2.f; o = p.Update(d); assert(o.reason == LookReason::Path);
	}
	// 5. Hysteresis: an interest that flickers clear/blocked every tick does not ping-pong the view.
	{
		BotLookPolicy p; int changes = 0;
		for (int i = 0; i < 40; ++i)
		{
			LookInput in = Base(i * 0.05f); in.hasInterest = true; in.interest = {10, 10, 1}; in.interestRayClear = (i % 2) == 0;
			changes += p.Update(in).changed;
		}
		assert(changes <= 6);   // at most one change per minHold (0.4 s) over 2 s
	}
	// 6. Near a corner the view turns to the next corner (anticipation).
	{
		BotLookPolicy p; LookInput in = Base(0); in.corner = {0, 2, 0}; in.hasNextCorner = true; in.nextCorner = {10, 2, 0};
		LookOutput o = p.Update(in); assert(o.reason == LookReason::NextCorner && o.yaw > 60.f);
	}
	// 7. Standing still with nothing to look at keeps the current view.
	{
		BotLookPolicy p; LookInput in = Base(0); in.speed = 0.f; in.viewYaw = 123.f;
		LookOutput o = p.Update(in); assert(o.reason == LookReason::Keep && Near(o.yaw, 123.f));
	}
	// 8. Path look-ahead: 3 m along an L-shaped path turns the corner.
	{
		LookVec pts[] = {{0, 2, 0}, {10, 2, 0}};
		LookVec a = PathLookAhead({0, 0, 0}, pts, 2, 3.f);
		assert(std::fabs(a.x - 1.f) < 1e-3f && std::fabs(a.y - 2.f) < 1e-3f);
		LookVec e = PathLookAhead({0, 0, 0}, pts, 2, 50.f); assert(e.x == 10.f);
	}
    // C055: occluded look-ahead must not point through a turn; budget includes interests.
    {
        const LookVec pos{0,0,0}, pts[]={{0,1,0},{4,1,0}};
        unsigned rays=0,calls=0;
        auto clear=[&](const LookVec& p){++calls;assert(Near(p.z,1.4f));return true;};
        LookVec a=ClearPathLookAhead(pos,pts,2,rays,clear);
        assert(Near(a.x,2.f) && Near(a.y,1.f) && rays==1 && calls==1);
        rays=calls=0;
        a=ClearPathLookAhead(pos,pts,2,rays,[&](const LookVec& p){++calls;return p.x>3.f;});
        assert(a.x==4.f && a.y==1.f && rays==2 && calls==2); // near corner: visible next corner
        rays=calls=0;
        a=ClearPathLookAhead(pos,pts,2,rays,[&](const LookVec&){++calls;return false;});
        assert(a.x==0.f && a.y==1.f && rays==2 && calls==2); // both hidden: current corner
        rays=1;calls=0;
        a=ClearPathLookAhead(pos,pts,2,rays,[&](const LookVec&){++calls;return false;});
        assert(a.x==0.f && rays==2 && calls==1); // no third ray after an existing query
        const LookVec distant[]={{0,4,0},{4,4,0}};rays=calls=0;
        a=ClearPathLookAhead(pos,distant,2,rays,[&](const LookVec&){++calls;return false;});
        assert(a.y==4.f && rays==1 && calls==1); // don't skip a distant current corner
        rays=calls=0;a=ClearPathLookAhead(pos,pts,0,rays,[&](const LookVec&){++calls;return true;});
        assert(rays==0 && calls==0 && a.x==0.f && a.y==0.f);
    }
    // C055: blocked movement still follows a path; Keep means actual view, not the raw goal.
    {
        BotLookPolicy p;LookInput in=Base(0);in.speed=0;in.pathActive=true;
        assert(p.Update(in).reason==LookReason::Path);
        BotLookPolicy q;in.hasCorner=false;in.pathActive=false;in.viewYaw=123;in.viewPitch=17;
        const auto out=q.Update(in);assert(out.reason==LookReason::Keep && Near(out.yaw,123) && Near(out.pitch,17));
    }
	std::puts("BotLookPolicyTests OK");
	return 0;
}
