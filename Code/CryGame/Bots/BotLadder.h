// C154 / DS-107 revision 2: bounded native ladder policy (wb_bot_ladder, default OFF).
// The policy is engine-free: it decides approach / grab / forward climb / native-exit wait / safe settle.
// The engine adapter (BotBrain) performs IsLadderUsable/RequestGrabOnLadder only before the grab, locks the native
// ladder frame, and lets the stock CPlayerMovement top ray + animation/OnLadder completion initiate eLAT_ExitTop.
// Bot logic never calls RequestLeaveLadder(eLAT_ExitTop), never steers sideways while attached, never uses a
// height-only success, and never snaps the actor. Success requires native !onLadder && !exiting, grounded, and a
// physical post-exit floor/target check.
#pragma once
#include <cmath>
#include <initializer_list>

namespace WarsBots {

struct LVec { float x = 0, y = 0, z = 0; };

// The native graph can already report 0 before an owned server bot has performed its exit root motion.
// Require the real long-shaft egress position as well: near the top and sideways out of the shaft, never a timer.
inline bool NativeLadderExitPhysicalReady(const LVec& feet, const LVec& bottom, const LVec& top)
{
	for (float v : {feet.x,feet.y,feet.z,bottom.x,bottom.y,bottom.z,top.x,top.y,top.z})
		if (!std::isfinite(v)) return false;
	const float x=top.x-bottom.x,y=top.y-bottom.y,z=top.z-bottom.z;
	const float length=std::sqrt(x*x+y*y+z*z);
	if (length<4.f) return false; // this opt-in executor supports the verified long shaft
	const float dx=feet.x-bottom.x,dy=feet.y-bottom.y,dz=feet.z-bottom.z;
	const float along=(dx*x+dy*y+dz*z)/length;
	const float side2=dx*dx+dy*dy+dz*dz-along*along;
	return along>=length-1.2f && along<=length+0.75f && side2>=0.35f*0.35f && side2<=2.5f*2.5f;
}

enum class LadderPhase { Idle = 0, Approach, Grab, Climb, WaitExit, Settle, Done, Failed };
enum class LadderFail { None = 0, Timeout, NoGrab, Lost, BlockedTop, WrongFloor, WrongLadder };

// Before attachment, falling into water or dropping off the take-off floor (>2 m from base.z) is a movement-state
// abort (cancel without banning the link), never a 25 s climb timeout that globally bans a valid ladder.
inline bool LadderApproachInvalid(LadderPhase phase, bool onLadder, bool exiting, bool swimming, float feetZ, float baseZ, float maxDz = 2.0f)
{
	if (swimming) return true;
	if (onLadder || exiting) return false;
	if (phase != LadderPhase::Approach && phase != LadderPhase::Grab) return false;
	return !std::isfinite(feetZ) || !std::isfinite(baseZ) || std::fabs(feetZ - baseZ) > maxDz;
}

// One bot owns a ladder link at a time. While a teammate is climbing `ref` (or waiting closer to its base inside
// the 2 m gate), hold outside the grab zone instead of starting a parallel 3 s grab timer into an occupied shaft.
inline bool ShouldYieldLadderStart(unsigned long long ref, float myDist2, int myNumber,
	unsigned long long otherRef, bool otherActive, bool otherWaitingAtGate, float otherDist2, int otherNumber)
{
	if (!ref || otherRef != ref || otherNumber == myNumber) return false;
	if (otherActive) return true;
	if (!otherWaitingAtGate) return false;
	if (otherDist2 + 0.01f < myDist2) return true;
	return std::fabs(otherDist2 - myDist2) <= 0.01f && otherNumber < myNumber;
}

struct LadderInput {
	float now = 0.f;
	LVec feet;
	bool grounded = false;
	bool onLadder = false;      // native m_stats.isOnLadder
	bool exiting = false;       // native m_stats.isExitingLadder (native top check initiated)
	bool atTop = false;         // native distance to ladderTop inside LADDER_TOP_DISTANCE
	bool canGrab = false;       // native IsLadderUsable() only before the grab
};

struct LadderOutput {
	LadderPhase phase = LadderPhase::Idle;
	LVec moveTo;
	bool grab = false;          // adapter may call RequestGrabOnLadder(eLAT_Use)
	bool climbing = false;      // keep forward into the ladder; no sideways steer
	bool settling = false;      // normal safe movement to the landing floor
	LadderFail fail = LadderFail::None;
};

struct LadderParams {
	float approachRadius = 1.2f;
	float grabTime = 3.0f;      // allow the existing capped view to face the real material before grabbing
	float total = 25.f;
	float topEps = 2.5f;        // native ladderTop band (LADDER_TOP_DISTANCE 2.41 + margin)
	float exitWait = 2.0f;      // native ladderTop band wait before BlockedTop (exit not started)
	// DS-124: WaitExit hard failure budget. The observed human same-shaft exit is ~2.0 s and the old 2.0 s window
	// left no headroom for the 10 Hz brain / graph handoff. This is a FAILURE bound only (never success/detach).
	float exitBudget = 3.0f;
	float settleTime = 4.0f;
	float floorDz = 1.0f;
	float floorDist = 0.8f;     // finish the safe walk-off onto the actual path landing point
	float nativeBottomTol = 1.5f;
	float nativeTopTol = 2.5f;
	float minRise = 4.0f;
};

class BotLadder {
public:
	explicit BotLadder(const LadderParams& p = LadderParams()) : m_p(p) {}
	void Reset()
	{
		m_phase = LadderPhase::Idle; m_fail = LadderFail::None;
		m_start = m_phaseAt = 0.f; m_a = m_b = m_bottom = m_top = m_into = LVec();
		m_rejectNative = false; m_nativeValid=false; m_atTopSince = -1.f;
	}
	void Start(float now, const LVec& a, const LVec& b)
	{
		m_a = a; m_b = b; m_bottom = a; m_top = b;
		m_into = Horiz(b.x - a.x, b.y - a.y);
		m_start = m_phaseAt = now; m_phase = LadderPhase::Approach;
		m_fail = LadderFail::None; m_rejectNative = false; m_nativeValid=false; m_atTopSince = -1.f;
	}
	// Called only before the grab. Returns false when the native ladder frame is not the requested UP edge.
	bool SetNative(const LVec& bottom, const LVec& top, const LVec& orientation)
	{
		const float rise = top.z - bottom.z;
		if (!Finite(bottom) || !Finite(top) || !Finite(orientation) || rise < m_p.minRise) return false;
		if (Dist2D(bottom, m_a) > m_p.nativeBottomTol || std::fabs(bottom.z - m_a.z) > m_p.nativeBottomTol) return false;
		if (Dist2D(top, m_b) > m_p.nativeTopTol || std::fabs(top.z - m_b.z) > m_p.nativeTopTol) return false;
		if (std::sqrt(orientation.x * orientation.x + orientation.y * orientation.y) < 0.2f) return false;
		m_bottom = bottom; m_top = top;
		m_into = Horiz(-orientation.x, -orientation.y);
		m_nativeValid=true;
		return true;
	}
	void RejectNative() { m_rejectNative = true; }
	bool Active() const { return m_phase != LadderPhase::Idle && m_phase != LadderPhase::Done && m_phase != LadderPhase::Failed; }
	LadderPhase Phase() const { return m_phase; }
	LadderFail Fail() const { return m_fail; }
	const LVec& Base() const { return m_a; }
	const LVec& Top() const { return m_b; }
	const LVec& Into() const { return m_into; }

	LadderOutput Update(const LadderInput& in)
	{
		LadderOutput o;
		for (int i = 0; i < 4; ++i)
		{
			const LadderPhase before = m_phase;
			o = Step(in);
			if (m_phase == before) break;
			if (m_phase != LadderPhase::Grab && m_phase != LadderPhase::Climb &&
				m_phase != LadderPhase::WaitExit && m_phase != LadderPhase::Settle) break;
		}
		return o;
	}

private:
	LadderOutput Step(const LadderInput& in)
	{
		LadderOutput o;
		o.phase = m_phase;
		o.moveTo = m_b;
		if (!Active()) return o;
		if (in.now - m_start > m_p.total) return Fail(o, LadderFail::Timeout);
		switch (m_phase)
		{
		case LadderPhase::Approach:
			o.moveTo = m_a;
			if (Dist2D(in.feet, m_a) <= m_p.approachRadius) Enter(LadderPhase::Grab, in.now);
			break;
		case LadderPhase::Grab:
			o.moveTo = m_a;
			o.grab = in.canGrab && m_nativeValid;
			if (in.onLadder && !m_nativeValid) return Fail(o,LadderFail::WrongLadder);
			if (in.onLadder) Enter(LadderPhase::Climb, in.now);
			else if (m_rejectNative) return Fail(o, LadderFail::WrongLadder);
			else if (in.now - m_phaseAt > m_p.grabTime) return Fail(o, LadderFail::NoGrab);
			break;
		case LadderPhase::Climb:
			o.climbing = true;
			// Keep the real forward climb while attached; never steer sideways or inject z.
			o.moveTo = LVec{ in.feet.x + m_into.x * 2.f, in.feet.y + m_into.y * 2.f, in.feet.z };
			if (in.exiting) Enter(LadderPhase::WaitExit, in.now);
			else if (!in.onLadder)
			{
				// A native detach below the top band is a lost climb, never success.
				if (in.feet.z < m_top.z - m_p.topEps) return Fail(o, LadderFail::Lost);
				Enter(LadderPhase::Settle, in.now);
			}
			else if (in.atTop)
			{
				// At the native top band but the native ray/animation has not started the exit: keep forward, but
				// report a real blocked top instead of climbing forever or detaching by height.
				if (m_atTopSince < 0.f) m_atTopSince = in.now;
				else if (in.now - m_atTopSince > m_p.exitWait) return Fail(o, LadderFail::BlockedTop);
			}
			else m_atTopSince = -1.f;
			break;
		case LadderPhase::WaitExit:
			// Native root motion owns the exit; stop bot steering and wait for the native completion.
			o.moveTo = in.feet;
			if (!in.onLadder && !in.exiting) Enter(LadderPhase::Settle, in.now);
			// Failure budget only: success stays the native detach plus the physical/grounded settle below.
			else if (in.now - m_phaseAt > m_p.exitBudget) return Fail(o, LadderFail::BlockedTop);
			break;
		case LadderPhase::Settle:
			if (in.onLadder || in.exiting) return Fail(o,LadderFail::Lost);
			o.settling = true;
			o.moveTo = m_b;
			if (!in.onLadder && !in.exiting && in.grounded && std::fabs(in.feet.z-m_b.z)<=m_p.floorDz && Dist2D(in.feet, m_b) <= m_p.floorDist)
			{
				m_phase = LadderPhase::Done; o.phase = m_phase; return o;
			}
			if (in.now - m_phaseAt > m_p.settleTime) return Fail(o, LadderFail::WrongFloor);
			break;
		default: break;
		}
		o.phase = m_phase;
		return o;
	}

	static bool Finite(const LVec& v)
	{
		return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
	}
	static LVec Horiz(float x, float y)
	{
		const float l = std::sqrt(x * x + y * y);
		return l > 1e-4f ? LVec{ x / l, y / l, 0.f } : LVec{ 0.f, 1.f, 0.f };
	}
	static float Dist2D(const LVec& a, const LVec& b)
	{
		const float dx = a.x - b.x, dy = a.y - b.y;
		return std::sqrt(dx * dx + dy * dy);
	}
	void Enter(LadderPhase p, float now) { m_phase = p; m_phaseAt = now; }
	LadderOutput Fail(LadderOutput o, LadderFail why)
	{
		m_phase = LadderPhase::Failed; m_fail = why;
		o.phase = m_phase; o.fail = why; o.grab = false; o.climbing = false; o.settling = false;
		return o;
	}

	LadderParams m_p;
	LadderPhase m_phase = LadderPhase::Idle;
	LadderFail m_fail = LadderFail::None;
	LVec m_a, m_b, m_bottom, m_top, m_into;
	float m_start = 0.f, m_phaseAt = 0.f, m_atTopSince = -1.f;
	bool m_rejectNative = false;
	bool m_nativeValid=false;
};

} // namespace WarsBots
