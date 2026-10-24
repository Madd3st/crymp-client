// Jump-link executor (D-008 roadmap step 9, D-010 Claude module). Crysis is vertical: a navmesh off-mesh link
// (normal jump, Strength jump, drop) is an ACTION with its own state, not `intent.jump` at the end of a path corner.
// While a link runs, the brain must not replan or change goal (see `HoldsBrain()`); the executor owns movement, view,
// suit request and the jump trigger, and reports success or a failure reason. On failure the caller bans that link
// for the rest of this life (`JumpLinkBans`). Pure logic, no engine types; Tests/BotJumpLinkTests.cpp.
// Numbers: normal jump v0 4.43 m/s (1.0 m apex), Strength 9.60 m/s (4.7 m apex, 70 energy) — PlayerMovement.cpp
// 1134,1150-1170 (OpenCode OC-001); run 4.5 m/s, sprint 6.75 m/s (Player.cpp stance maxSpeed, ×1.5).
#pragma once
#include <cmath>

namespace WarsBots {

enum class LinkType { Normal = 0, Strength, Drop };
enum class LinkState { Idle = 0, Approach, Prepare, Launch, Airborne, Done, Failed };
enum class LinkFail { None = 0, ApproachTimeout, NoEnergy, SuitTimeout, NoTakeoff, AirTimeout, MissedLanding, Fell };
enum class LinkJump { None = 0, Normal, Strength };
enum class LinkSuit { Keep = 0, Armor, Strength };

struct LinkVec { float x = 0, y = 0, z = 0; };

struct JumpLinkDef {
	int id = 0;
	LinkType type = LinkType::Normal;
	LinkVec start, end;            // take-off and landing points (feet)
};

struct JumpLinkBody {              // filled from the blackboard each tick
	float now = 0.f;
	LinkVec pos;
	float speed = 0.f;             // horizontal m/s
	bool grounded = true;
	float energy = 200.f;
	bool strengthActive = false;   // suit is in Strength mode right now
};

struct JumpLinkCommand {           // what BotLocomotion/BotSuit should do this tick
	bool move = false;
	LinkVec moveTo;                // steer target
	bool sprint = false;
	float speed = 0.f;             // desired horizontal m/s (0 = locomotion default); set for the jump run-up/air
	LinkVec lookAt;                // view target (eye height added by the caller)
	LinkJump jump = LinkJump::None;
	LinkSuit suit = LinkSuit::Keep;
};

struct JumpLinkParams {
	float startRadius = 0.5f;      // m, close enough to the take-off point
	float approachTimeout = 4.0f;  // s
	float prepareTimeout = 1.5f;   // s to get Strength mode + energy
	float strengthEnergy = 70.f;
	float takeoffTimeout = 0.6f;   // s from jump request to leaving the ground
	float airTimeout = 4.0f;       // s
	float landRadius = 1.5f;       // m horizontal from the link end
	float landDz = 1.0f;           // m vertical tolerance at the end
	float fellDz = 2.0f;           // landed this much below the end -> fell
};

class JumpLinkExecutor {
public:
	explicit JumpLinkExecutor(const JumpLinkParams& p = JumpLinkParams()) : m_p(p) {}

	void Begin(const JumpLinkDef& link, float now)
	{
		m_link = link; m_state = LinkState::Approach; m_since = now; m_fail = LinkFail::None;
	}
	void Abort() { m_state = LinkState::Idle; }

	LinkState State() const { return m_state; }
	LinkFail Failure() const { return m_fail; }
	int LinkId() const { return m_link.id; }
	// While true the brain keeps its goal and does not replan (approach is still interruptible by combat).
	bool HoldsBrain() const { return m_state == LinkState::Prepare || m_state == LinkState::Launch || m_state == LinkState::Airborne; }
	bool Running() const { return m_state != LinkState::Idle && m_state != LinkState::Done && m_state != LinkState::Failed; }

	JumpLinkCommand Update(const JumpLinkBody& b)
	{
		JumpLinkCommand c;
		c.lookAt = m_link.end;
		const float hs = Horiz(m_link.start, m_link.end);
		switch (m_state)
		{
		case LinkState::Approach:
			if (Horiz(b.pos, m_link.start) <= m_p.startRadius && b.grounded)
			{ To(LinkState::Prepare, b.now); return Update(b); }
			if (b.now - m_since > m_p.approachTimeout) return Fail(LinkFail::ApproachTimeout);
			c.move = true; c.moveTo = m_link.start; c.lookAt = m_link.end;
			return c;
		case LinkState::Prepare:
			if (m_link.type == LinkType::Strength)
			{
				if (b.energy < m_p.strengthEnergy && b.now - m_since > 0.2f) return Fail(LinkFail::NoEnergy);
				c.suit = LinkSuit::Strength;
				if (!b.strengthActive)
				{
					if (b.now - m_since > m_p.prepareTimeout) return Fail(LinkFail::SuitTimeout);
					c.move = false;           // stand still at the edge while the suit switches
					return c;
				}
			}
			To(LinkState::Launch, b.now);
			return Update(b);
		case LinkState::Launch:
			c.move = true; c.moveTo = m_link.end; c.speed = TakeoffSpeed(hs); c.sprint = c.speed > 4.5f;
			if (m_link.type == LinkType::Strength) c.suit = LinkSuit::Strength;
			if (!b.grounded) { To(LinkState::Airborne, b.now); c.jump = LinkJump::None; return c; }
			if (b.now - m_since > m_p.takeoffTimeout) return Fail(LinkFail::NoTakeoff);
			if (!m_jumpSent)
			{
				c.jump = m_link.type == LinkType::Strength ? LinkJump::Strength :
					m_link.type == LinkType::Normal ? LinkJump::Normal : LinkJump::None;
				m_jumpSent = true;
			}
			return c;
		case LinkState::Airborne:
			c.move = true; c.moveTo = m_link.end; c.speed = TakeoffSpeed(hs);   // air control toward the landing
			if (b.now - m_since > m_p.airTimeout) return Fail(LinkFail::AirTimeout);
			if (b.grounded && b.now - m_since > 0.1f)
			{
				const float dz = b.pos.z - m_link.end.z;
				if (dz < -m_p.fellDz) return Fail(LinkFail::Fell);
				if (Horiz(b.pos, m_link.end) > m_p.landRadius || std::fabs(dz) > m_p.landDz) return Fail(LinkFail::MissedLanding);
				To(LinkState::Done, b.now);
				c.move = false; c.suit = LinkSuit::Armor;
			}
			return c;
		default:
			return c;
		}
	}

	static const char* FailName(LinkFail f)
	{
		switch (f) { case LinkFail::ApproachTimeout: return "approach_timeout"; case LinkFail::NoEnergy: return "no_energy";
			case LinkFail::SuitTimeout: return "suit_timeout"; case LinkFail::NoTakeoff: return "no_takeoff";
			case LinkFail::AirTimeout: return "air_timeout"; case LinkFail::MissedLanding: return "missed_landing";
			case LinkFail::Fell: return "fell"; default: return "none"; }
	}

private:
	// Horizontal speed that lands on the link end: distance / flight time to the end height on the descending branch.
	float TakeoffSpeed(float hs) const
	{
		const float v0 = m_link.type == LinkType::Strength ? 9.60f : m_link.type == LinkType::Normal ? 4.43f : 0.f;
		const float dz = m_link.end.z - m_link.start.z, g = 9.81f;
		float t;
		if (v0 <= 0.f) t = dz < 0.f ? std::sqrt(-2.f * dz / g) : 0.3f;
		else { const float disc = v0 * v0 - 2.f * g * dz; t = disc > 0.f ? (v0 + std::sqrt(disc)) / g : v0 / g; }
		const float v = t > 0.05f ? hs / t : 6.75f;
		return v < 1.0f ? 1.0f : (v > 6.75f ? 6.75f : v);
	}
	static float Horiz(const LinkVec& a, const LinkVec& b) { const float dx = a.x - b.x, dy = a.y - b.y; return std::sqrt(dx * dx + dy * dy); }
	void To(LinkState s, float now) { m_state = s; m_since = now; if (s == LinkState::Launch) m_jumpSent = false; }
	JumpLinkCommand Fail(LinkFail f) { m_state = LinkState::Failed; m_fail = f; JumpLinkCommand c; c.suit = LinkSuit::Armor; return c; }

	JumpLinkParams m_p;
	JumpLinkDef m_link;
	LinkState m_state = LinkState::Idle;
	LinkFail m_fail = LinkFail::None;
	float m_since = 0.f;
	bool m_jumpSent = false;
};

// Links that failed in this life are not used again until the next revive (call Clear() on revive).
class JumpLinkBans {
public:
	void Clear() { m_n = 0; }
	void Ban(int id) { if (Banned(id)) return; if (m_n < kMax) m_ids[m_n++] = id; else { for (int i = 1; i < kMax; ++i) m_ids[i - 1] = m_ids[i]; m_ids[kMax - 1] = id; } }
	bool Banned(int id) const { for (int i = 0; i < m_n; ++i) if (m_ids[i] == id) return true; return false; }
	int Count() const { return m_n; }
private:
	static const int kMax = 32;
	int m_ids[kMax] = {};
	int m_n = 0;
};

} // namespace WarsBots
