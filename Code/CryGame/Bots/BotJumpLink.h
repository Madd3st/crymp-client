// Off-mesh link executor (C-036, wb_bot_jump_links, default OFF). The baked navmesh (Tools/RecastBake --jumps) joins
// walkable parts with drop / jump / Strength-jump links whose arcs were checked offline with capsule rays. A corner path
// that uses one hands the bot to this executor at the take-off corner a; it owns movement until landing near b:
//   Approach  walk to a (<= 0.6 m), slow down, face b
//   Prepare   Strength link: ask the suit for Strength and wait until it is active with >= 70 energy (max 2.5 s);
//             jump link: wait for Armor (a normal jump is refused in Strength mode)
//   Act       drop: walk off toward b;  jump / Strength: one jump request toward b while running
//   Air       keep steering toward b; Armor before landing comes from the existing fall rule
//   Done      grounded within 1.5 m of b (|dz| < 1.5);  Failed: landed elsewhere, no take-off, or timeout (6 s)
// A failed link is banned for the level by the caller (NavMeshCore::BanLink) and the bot re-paths.
// Pure logic, no engine types; Tests/BotJumpLinkTests.cpp.
#pragma once
#include <cmath>

namespace WarsBots {

struct JVec { float x = 0, y = 0, z = 0; };

enum class LinkPhase { Idle = 0, Approach, Prepare, Act, Air, Done, Failed };
enum class LinkMove { Drop = 0, Jump, Strength };
enum class LinkJumpRequest { None = 0, Normal, Strength };
enum class LinkFail { None = 0, Timeout, NoTakeoff, MissedLanding, NoEnergy };

struct LinkInput {
	float now = 0.f;
	JVec feet;
	bool grounded = true;
	bool strengthActive = false;       // suit is in Strength mode
	bool armorActive = true;           // suit is in Armor (Defense) mode
	float energy = 200.f;
	float speed = 1.f;                 // horizontal m/s (an approach that stops short of a counts as arrived)
};

struct LinkOutput {
	LinkPhase phase = LinkPhase::Idle;
	JVec moveTo;                       // where to move this tick
	bool slow = false;                 // walk (approach the edge precisely)
	LinkJumpRequest jump = LinkJumpRequest::None;
	bool wantStrength = false;         // suit should be in Strength
	bool wantArmor = false;            // suit should be in Armor
	LinkFail fail = LinkFail::None;
};

struct LinkParams {
	float takeoffRadius = 1.0f;        // m from a to start the move (c061: 0.6 m left bots parked 0.62 m short)
	float nearRadius = 1.8f;           // m: standing still this close to a also counts as arrived
	float stillTime = 0.4f;            // s below 0.3 m/s near a
	float landRadius = 2.5f;           // m from b counts as landed (a running Strength jump carries past b)
	float landDz = 1.5f;
	float prepareMax = 4.f;            // s waiting for the suit / energy (regen ~10/s: c061 failed at 62..69 energy)
	float total = 6.f;                 // s for the whole link
	float noTakeoff = 1.2f;            // s after the jump request without leaving the ground
	float strengthEnergy = 70.f;
	float dropAirMin = 0.15f;          // m below a that counts as "left the edge" for a drop
};

class BotJumpLink {
public:
	explicit BotJumpLink(const LinkParams& p = LinkParams()) : m_p(p) {}
	bool Active() const { return m_phase != LinkPhase::Idle && m_phase != LinkPhase::Done && m_phase != LinkPhase::Failed; }
	LinkPhase Phase() const { return m_phase; }
	LinkMove Move() const { return m_move; }
	void Reset() { m_phase = LinkPhase::Idle; }

	void Start(float now, LinkMove move, const JVec& a, const JVec& b)
	{
		m_move = move; m_a = a; m_b = b; m_start = now; m_phase = LinkPhase::Approach; m_phaseAt = now; m_wasAir = false; m_stillSince = -1.f;
	}

	// Phase changes Approach -> Prepare -> Act can chain within one tick.
	LinkOutput Update(const LinkInput& in)
	{
		LinkOutput o;
		for (int i = 0; i < 3; ++i)
		{
			const LinkPhase before = m_phase;
			o = Step(in);
			if (m_phase == before || (m_phase != LinkPhase::Prepare && m_phase != LinkPhase::Act)) break;
		}
		return o;
	}

private:
	LinkOutput Step(const LinkInput& in)
	{
		LinkOutput o; o.moveTo = m_b;
		if (!Active()) { o.phase = m_phase; return o; }
		if (in.now - m_start > m_p.total) return Fail(o, LinkFail::Timeout);
		const float da = Dist2D(in.feet, m_a), db = Dist2D(in.feet, m_b);
		if (!in.grounded) m_wasAir = true;
		switch (m_phase)
		{
		case LinkPhase::Approach:
			o.moveTo = m_a; o.slow = da < 2.f;
			if (m_move == LinkMove::Strength) o.wantStrength = in.energy >= m_p.strengthEnergy;
			if (m_move == LinkMove::Jump) o.wantArmor = true;
			if (da <= m_p.nearRadius && in.speed < 0.3f) { if (m_stillSince < 0.f) m_stillSince = in.now; }
			else m_stillSince = -1.f;
			if (da <= m_p.takeoffRadius || (m_wasAir && m_move == LinkMove::Drop) ||
				(m_stillSince >= 0.f && in.now - m_stillSince >= m_p.stillTime)) Enter(LinkPhase::Prepare, in.now);
			break;
		case LinkPhase::Prepare:
			if (m_move == LinkMove::Strength)
			{
				o.wantStrength = true; o.moveTo = m_a; o.slow = true;
				if (in.strengthActive && in.energy >= m_p.strengthEnergy) Enter(LinkPhase::Act, in.now);
				else if (in.now - m_phaseAt > m_p.prepareMax) return Fail(o, LinkFail::NoEnergy);
			}
			else if (m_move == LinkMove::Jump)
			{
				o.wantArmor = true; o.moveTo = m_a; o.slow = true;
				if (in.armorActive) Enter(LinkPhase::Act, in.now);
				else if (in.now - m_phaseAt > m_p.prepareMax) return Fail(o, LinkFail::Timeout);
			}
			else Enter(LinkPhase::Act, in.now);
			break;
		case LinkPhase::Act:
			o.moveTo = m_b;
			if (m_move == LinkMove::Strength) o.wantStrength = true;
			if (m_move == LinkMove::Drop)
			{
				if (!in.grounded || in.feet.z < m_a.z - m_p.dropAirMin) Enter(LinkPhase::Air, in.now);
				else if (in.now - m_phaseAt > m_p.noTakeoff * 2.f) return Fail(o, LinkFail::NoTakeoff);
			}
			else
			{
				if (!in.grounded) Enter(LinkPhase::Air, in.now);
				else if (in.now - m_phaseAt > m_p.noTakeoff) return Fail(o, LinkFail::NoTakeoff);
				else o.jump = m_move == LinkMove::Strength ? LinkJumpRequest::Strength : LinkJumpRequest::Normal;
			}
			break;
		case LinkPhase::Air:
			o.moveTo = m_b;
			if (in.grounded && in.now - m_phaseAt > 0.1f)
			{
				if (db <= m_p.landRadius && std::fabs(in.feet.z - m_b.z) < m_p.landDz) { m_phase = LinkPhase::Done; o.phase = m_phase; return o; }
				// rolled on after landing toward b: give it a short moment, then judge
				if (in.now - m_phaseAt > 0.6f) return Fail(o, LinkFail::MissedLanding);
			}
			break;
		default: break;
		}
		o.phase = m_phase;
		return o;
	}

public:
	static float Dist2D(const JVec& a, const JVec& b) { return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y)); }

private:
	void Enter(LinkPhase p, float now) { m_phase = p; m_phaseAt = now; }
	LinkOutput Fail(LinkOutput o, LinkFail why) { m_phase = LinkPhase::Failed; o.phase = m_phase; o.fail = why; o.jump = LinkJumpRequest::None; return o; }

	LinkParams m_p;
	LinkPhase m_phase = LinkPhase::Idle;
	LinkMove m_move = LinkMove::Drop;
	JVec m_a, m_b;
	float m_start = 0.f, m_phaseAt = 0.f;
	bool m_wasAir = false;
	float m_stillSince = -1.f;
};

} // namespace WarsBots
