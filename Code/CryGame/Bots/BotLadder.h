// C154 / DS-107 revision 2: bounded native ladder policy (wb_bot_ladder, default OFF).
// The policy is engine-free: it decides approach / grab / forward climb / native-exit wait / safe settle.
// The engine adapter (BotBrain) performs IsLadderUsable/RequestGrabOnLadder only before the grab, locks the native
// ladder frame, and lets the stock CPlayerMovement top ray + animation/OnLadder completion initiate eLAT_ExitTop.
// Bot logic never calls RequestLeaveLadder(eLAT_ExitTop), never steers sideways while attached, never uses a
// height-only success, and never snaps the actor. Success requires native !onLadder && !exiting, grounded, and a
// physical post-exit floor/target check.
#pragma once
#include <cmath>

namespace WarsBots {

struct LVec { float x = 0, y = 0, z = 0; };

enum class LadderPhase { Idle = 0, Approach, Grab, Climb, WaitExit, Settle, Done, Failed };
enum class LadderFail { None = 0, Timeout, NoGrab, Lost, BlockedTop, WrongFloor, WrongLadder };

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
	float grabTime = 1.0f;
	float total = 15.f;
	float topEps = 2.5f;        // native ladderTop band (LADDER_TOP_DISTANCE 2.41 + margin)
	float exitWait = 2.0f;      // native animation/root-motion completion
	float settleTime = 4.0f;
	float floorDz = 1.0f;
	float floorDist = 2.5f;
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
		m_rejectNative = false; m_atTopSince = -1.f;
	}
	void Start(float now, const LVec& a, const LVec& b)
	{
		m_a = a; m_b = b; m_bottom = a; m_top = b;
		m_into = Horiz(b.x - a.x, b.y - a.y);
		m_start = m_phaseAt = now; m_phase = LadderPhase::Approach;
		m_fail = LadderFail::None; m_rejectNative = false; m_atTopSince = -1.f;
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
			o.grab = in.canGrab;
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
			else if (in.now - m_phaseAt > m_p.exitWait) return Fail(o, LadderFail::BlockedTop);
			break;
		case LadderPhase::Settle:
			o.settling = true;
			o.moveTo = m_b;
			if (in.grounded && in.feet.z >= m_b.z - m_p.floorDz && Dist2D(in.feet, m_b) <= m_p.floorDist)
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
};

} // namespace WarsBots
