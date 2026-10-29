#include "CryGame/Bots/BotLadder.h"
#include <cassert>
#include <cstdio>
using namespace WarsBots;

static LadderInput At(float t, float x, float y, float z, bool grounded = true, bool onLadder = false,
	bool exiting = false, bool canGrab = false)
{
	LadderInput in; in.now = t; in.feet = { x, y, z }; in.grounded = grounded; in.onLadder = onLadder;
	in.exiting = exiting; in.canGrab = canGrab;
	return in;
}

int main()
{
	const LVec a{ 0, 0, 10 }, b{ 1, 0, 20 };
	const LVec bottom{ 0.2f, 0.1f, 10.4f }, top{ 0.2f, 0.1f, 20.3f }, orientation{ 0, 1, 0 };

	// 1. Native frame validation: a short/wrong ladder is rejected; the real UP edge is accepted.
	{
		BotLadder L; L.Start(0, a, b);
		assert(!L.SetNative(LVec{ 0, 0, 10 }, LVec{ 0, 0, 12 }, orientation)); // short segment
		assert(!L.SetNative(LVec{ 30, 0, 10 }, LVec{ 30, 0, 20 }, orientation)); // wrong floor
		assert(L.SetNative(bottom, top, orientation));
	}
	// 2. Approach -> Grab; no mat_ladder match never requests a grab and fails after the window.
	{
		BotLadder L; L.Start(0, a, b);
		LadderOutput o = L.Update(At(0, 0, 3, 10)); assert(o.phase == LadderPhase::Approach && o.moveTo.z == 10.f);
		o = L.Update(At(0.2f, 0, 0.5f, 10)); assert(o.phase == LadderPhase::Grab && !o.grab);
		o = L.Update(At(3.4f, 0, 0.5f, 10)); assert(o.fail == LadderFail::NoGrab && !L.Active());
	}
	// 3. Grab -> Climb; the bot keeps forward into the ladder, no sideways steer, no exit request.
	{
		BotLadder L; L.Start(0, a, b); assert(L.SetNative(bottom, top, orientation));
		L.Update(At(0, 0, 0.5f, 10));
		LadderOutput o = L.Update(At(0.2f, 0, 0.5f, 10, true, false, false, true));
		assert(o.phase == LadderPhase::Grab && o.grab);
		o = L.Update(At(0.3f, 0, 0.5f, 10, true, true));
		assert(o.phase == LadderPhase::Climb && o.climbing);
		assert(o.moveTo.y < -1.f && o.moveTo.z == 10.f);
	}
	// 4. Native top check starts eLAT_ExitTop: WaitExit stops steering, pending is not success.
	{
		BotLadder L; L.Start(0, a, b); assert(L.SetNative(bottom, top, orientation));
		L.Update(At(0, 0, 0.5f, 10)); L.Update(At(0.1f, 0, 0.5f, 10, true, true));
		LadderOutput o = L.Update(At(8.0f, 0, 0.5f, 19.5f, true, true, true));
		assert(o.phase == LadderPhase::WaitExit && !o.climbing && o.moveTo.z == 19.5f);
		o = L.Update(At(8.2f, 0, 0.5f, 19.5f, true, true, true));
		assert(o.phase == LadderPhase::WaitExit && L.Active());
	}
	// 5. Native completion: not grounded -> Settle (safe movement); grounded near the landing floor -> Done.
	{
		BotLadder L; L.Start(0, a, b); assert(L.SetNative(bottom, top, orientation));
		L.Update(At(0, 0, 0.5f, 10)); L.Update(At(0.1f, 0, 0.5f, 10, true, true));
		L.Update(At(8.0f, 0, 0.5f, 19.5f, true, true, true));
		LadderOutput o = L.Update(At(8.2f, 0.2f, 0.1f, 19.5f, false, false, false));
		assert(o.phase == LadderPhase::Settle && o.settling && o.moveTo.x == b.x);
		o = L.Update(At(8.4f, 0.5f, 0.1f, 19.5f, true, false, false));
		assert(o.phase == LadderPhase::Done && !L.Active());
	}
	// 6. Blocked top: native exiting never completes -> BlockedTop (no fake ReachedEnd).
	{
		BotLadder L; L.Start(0, a, b); assert(L.SetNative(bottom, top, orientation));
		L.Update(At(0, 0, 0.5f, 10)); L.Update(At(0.1f, 0, 0.5f, 10, true, true));
		L.Update(At(8.0f, 0, 0.5f, 19.5f, true, true, true));
		LadderOutput o = L.Update(At(10.1f, 0, 0.5f, 19.5f, true, true, true));
		assert(o.fail == LadderFail::BlockedTop);

		// Native top band reached but the native ray/animation never starts the exit: real blocked top, not height success.
		BotLadder A; A.Start(0, a, b); assert(A.SetNative(bottom, top, orientation));
		A.Update(At(0, 0, 0.5f, 10)); A.Update(At(0.1f, 0, 0.5f, 10, true, true));
		LadderInput top = At(8.0f, 0, 0.5f, 19.5f, true, true, false); top.atTop = true;
		o = A.Update(top); assert(o.phase == LadderPhase::Climb);
		top.now = 10.1f; o = A.Update(top); assert(o.fail == LadderFail::BlockedTop);
	}
	// 7. Wrong floor: detached away from the landing floor -> WrongFloor; premature detach -> Lost.
	{
		BotLadder L; L.Start(0, a, b); assert(L.SetNative(bottom, top, orientation));
		L.Update(At(0, 0, 0.5f, 10)); L.Update(At(0.1f, 0, 0.5f, 10, true, true));
		L.Update(At(8.0f, 0, 0.5f, 19.5f, true, true, true));
		L.Update(At(8.2f, 5, 5, 5, false, false, false));
		LadderOutput o = L.Update(At(12.3f, 5, 5, 5, false, false, false));
		assert(o.fail == LadderFail::WrongFloor);

		BotLadder M; M.Start(0, a, b); assert(M.SetNative(bottom, top, orientation));
		M.Update(At(0, 0, 0.5f, 10)); M.Update(At(0.1f, 0, 0.5f, 10, true, true));
		o = M.Update(At(0.2f, 0, 0.5f, 10, true, false, false)); assert(o.fail == LadderFail::Lost);
	}
	// 8. Wrong native ladder -> WrongLadder; reset/cancel clears an active climb; total timeout fails.
	{
		BotLadder L; L.Start(0, a, b); L.RejectNative();
		LadderOutput o = L.Update(At(0, 0, 0.5f, 10)); assert(o.fail == LadderFail::WrongLadder);

		BotLadder M; M.Start(0, a, b); assert(M.SetNative(bottom, top, orientation));
		M.Update(At(0, 0, 0.5f, 10)); M.Update(At(0.1f, 0, 0.5f, 10, true, true));
		M.Reset(); assert(!M.Active() && M.Phase() == LadderPhase::Idle);

		BotLadder N; N.Start(0, a, b); o = N.Update(At(25.1f, 0, 3, 10)); assert(o.fail == LadderFail::Timeout);
	}
	// 9. A roof above the requested floor, a reattachment, or an unvalidated native frame cannot finish the route.
	for (int mode=0;mode<2;++mode)
	{
		BotLadder L;L.Start(0,a,b);assert(L.SetNative(bottom,top,orientation));
		L.Update(At(0,0,0.5f,10));L.Update(At(.1f,0,0.5f,10,true,true));
		L.Update(At(8,0,0.5f,19.5f,true,true,true));
		L.Update(At(8.2f,1,0,20,false,false,false));
		const auto o=L.Update(mode==0?At(8.3f,1,0,30,true):At(8.3f,1,0,20,true,true));
		assert(o.phase!=LadderPhase::Done);
	}
	{
		BotLadder L;L.Start(0,a,b);
		const auto o=L.Update(At(0,0,0,10,true,false,false,true));assert(!o.grab);
		assert(L.Update(At(.1f,0,0,10,true,true)).fail==LadderFail::WrongLadder);
	}
	// 10. An early/absent graph completion must not detach a bot still in the shaft; actual lateral egress can finish.
	assert(!NativeLadderExitPhysicalReady(LVec{0,0,17.6f},LVec{0,0,10},LVec{0,0,20}));
	assert(!NativeLadderExitPhysicalReady(LVec{0,0,19.3f},LVec{0,0,10},LVec{0,0,20}));
	assert(NativeLadderExitPhysicalReady(LVec{0,-.9f,19.3f},LVec{0,0,10},LVec{0,0,20}));
	std::printf("BotLadder native-frame, exit and settle tests PASS\n");
	return 0;
}
