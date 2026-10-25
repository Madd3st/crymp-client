// Combat tactics (fragmovie patterns, SHOOTER_BOT_PATTERNS.md; wb_bot_tactics, default OFF):
//   * hold an angle from cover: a target just broke line of sight -> instead of running straight at its last seen
//     position, step to a nearby cover spot (tactical_spots flag 1/2) that still sees that position, aim there at chest
//     height and wait a few seconds for the peek (Killzone position scoring: distance, line of fire, cover toward the
//     threat, preferred range);
//   * jump-strafe in a duel: good players never stand still in the open, they strafe and hop;
//   * loot after a kill: take the dropped weapon / ammo near the body before moving on.
// Traits are numbers per skill/style for now (to become the character profile later). Pure logic; Tests/BotTacticsTests.cpp.
#pragma once
#include <cmath>

namespace WarsBots {

struct TVec { float x = 0, y = 0, z = 0; };
inline float TDist(const TVec& a, const TVec& b) { return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z)); }

struct CoverCandidate { TVec p; unsigned coverMask = 0; };

// Best cover spot to hold `threat` from, or -1. `sees(spot)` = eye ray from the spot to the threat is clear.
// coverMask bit k = protected side at yaw k*22.5 deg (0 = +Y, clockwise), as written by tactical_spots.
template <class Sees>
int PickCover(const CoverCandidate* c, int n, const TVec& bot, const TVec& threat, float preferred, Sees sees,
	float maxMove = 12.f, int maxRays = 6)
{
	struct S { int i; float s; } best[8]; int nb = 0;
	for (int i = 0; i < n; ++i)
	{
		const float move = TDist(c[i].p, bot), range = TDist(c[i].p, threat);
		if (move > maxMove || range < 6.f || range > 50.f) continue;
		const float yaw = std::atan2(threat.x - c[i].p.x, threat.y - c[i].p.y) * 57.2957795f;
		int k = int(std::floor((yaw < 0 ? yaw + 360.f : yaw) / 22.5f + 0.5f)) % 16;
		const bool toward = (c[i].coverMask >> k & 1u) || (c[i].coverMask >> ((k + 1) % 16) & 1u) || (c[i].coverMask >> ((k + 15) % 16) & 1u);
		const float s = -move - std::fabs(range - preferred) * 0.3f + (toward ? 6.f : 0.f);
		// keep the 8 best by score, then spend rays only on those
		int j;
		if (nb < 8) j = nb++;
		else { if (s <= best[7].s) continue; j = 7; }
		best[j] = {i, s};
		while (j > 0 && best[j].s > best[j - 1].s) { S t = best[j]; best[j] = best[j - 1]; best[j - 1] = t; --j; }
	}
	for (int j = 0, rays = 0; j < nb && rays < maxRays; ++j, ++rays) if (sees(c[best[j].i].p)) return best[j].i;
	return -1;
}

enum class HoldPhase { None = 0, Move, Hold, Done };

// One angle-hold episode per lost target.
class BotAngleHold {
public:
	void Reset() { m_phase = HoldPhase::None; m_target = 0; }
	HoldPhase Phase() const { return m_phase; }
	const TVec& Spot() const { return m_spot; }
	// Called when a fight target is not visible. Returns true when the bot should hold instead of chase.
	bool Begin(unsigned target, float now, float sinceSeen, float distance, float holderTrait, unsigned rng)
	{
		if (target == m_target) return m_phase == HoldPhase::Move || m_phase == HoldPhase::Hold;
		m_target = target; m_phase = HoldPhase::Done;
		if (sinceSeen > 3.f || distance > 45.f || distance < 6.f) return false;
		if (float(rng % 1000) / 1000.f >= holderTrait) return false;
		m_phase = HoldPhase::None; m_started = now; return true;     // caller picks a spot next
	}
	void Assign(const TVec& spot, float now, float holdFor) { m_spot = spot; m_phase = HoldPhase::Move; m_started = now; m_holdFor = holdFor; }
	void Fail() { m_phase = HoldPhase::Done; }
	// Advance: arrive within 1 m -> Hold; give up moving after 4 s; hold for holdFor seconds.
	HoldPhase Update(float now, const TVec& bot, bool targetVisible)
	{
		if (targetVisible) { m_phase = HoldPhase::Done; return m_phase; }  // the fight takes over
		if (m_phase == HoldPhase::Move)
		{
			const float dx = bot.x - m_spot.x, dy = bot.y - m_spot.y;
			if (dx * dx + dy * dy < 1.f && std::fabs(bot.z - m_spot.z) < 1.5f) { m_phase = HoldPhase::Hold; m_started = now; }
			else if (now - m_started > 4.f) m_phase = HoldPhase::Done;
		}
		else if (m_phase == HoldPhase::Hold && now - m_started > m_holdFor) m_phase = HoldPhase::Done;
		return m_phase;
	}
private:
	HoldPhase m_phase = HoldPhase::None;
	unsigned m_target = 0;
	TVec m_spot;
	float m_started = 0, m_holdFor = 4.f;
};

// Jump-strafe timer: in a duel, a grounded bot hops every 1.5..4 s with probability = jumper trait.
class BotJumpStrafe {
public:
	bool Want(float now, bool grounded, float distance, float jumper, unsigned rng)
	{
		if (!grounded || distance < 4.f || distance > 30.f || now < m_next) return false;
		m_next = now + 1.5f + 2.5f * float(rng % 1000) / 1000.f;
		return float((rng / 1000u) % 1000) / 1000.f < jumper;
	}
private:
	float m_next = 0;
};

} // namespace WarsBots
