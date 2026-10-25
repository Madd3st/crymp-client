#include "CryGame/Bots/BotSuitPolicy.h"
#include <cassert>
#include <cstdio>
using namespace WarsBots;

static SuitInput Travel(float t, float energy, float dist) { SuitInput in; in.now = t; in.energy = energy; in.moving = true; in.travelDistance = dist; return in; }

int main()
{
	// 1. Long travel with energy -> Speed; keeps Speed down to the exit threshold, then Armor.
	{
		BotSuitPolicy p; assert(p.Update(Travel(0, 150, 40)).mode == SuitMode::Speed);
		assert(p.Update(Travel(2, 90, 30)).mode == SuitMode::Speed);    // 90 > exit 60
		assert(p.Update(Travel(4, 55, 30)).mode == SuitMode::Armor);
		assert(p.Update(Travel(6, 100, 30)).mode == SuitMode::Armor);   // needs 130 again to enter
	}
	// 2. Short hops never start Speed; beginners never use it.
	{
		BotSuitPolicy p; assert(p.Update(Travel(0, 200, 10)).mode == SuitMode::Armor);
		SuitInput b = Travel(0, 200, 50); b.canUseSpeed = false; BotSuitPolicy q; assert(q.Update(b).mode == SuitMode::Armor);
	}
	// 3. Emergencies are immediate (no hold): hit while in Speed -> Armor; falling -> Armor; Strength jump with energy.
	{
		BotSuitPolicy p; p.Update(Travel(0, 150, 40));
		SuitInput hit = Travel(0.2f, 150, 40); hit.damagedRecently = true;
		SuitDecision d = p.Update(hit); assert(d.mode == SuitMode::Armor && d.reason == SuitReason::UnderFire);
		SuitInput j = Travel(0.4f, 100, 40); j.wantStrengthJump = true;
		d = p.Update(j); assert(d.mode == SuitMode::Strength);
		SuitInput f = Travel(0.6f, 100, 40); f.airborne = true; f.verticalSpeed = -9.f;
		d = p.Update(f); assert(d.mode == SuitMode::Armor && d.reason == SuitReason::Falling);
		SuitInput weak = Travel(1.0f, 50, 40); weak.wantStrengthJump = true; BotSuitPolicy q;
		assert(q.Update(weak).mode == SuitMode::Armor);                  // 50 < 70: no Strength jump
	}
	// 4. Fight: visible enemy -> Armor even with full energy; Speed travel resumes only after the hold.
	{
		BotSuitPolicy p; SuitInput f = Travel(0, 200, 40); f.enemyVisible = true;
		assert(p.Update(f).mode == SuitMode::Armor);
		assert(p.Update(Travel(0.5f, 200, 40)).mode == SuitMode::Armor);   // just out of the fight: hold Armor
		assert(p.Update(Travel(2.0f, 200, 40)).mode == SuitMode::Speed);   // 1.5 s later: travel in Speed
	}
	// 5. Stealth: only allowed + stealth style + hidden enemy 12..35 m + energy; firing breaks it.
	{
		BotSuitPolicy p; SuitInput s = Travel(0, 150, 20); s.cloakAllowed = true; s.stealthStyle = true; s.enemyKnownHidden = true; s.enemyDistance = 20;
		assert(p.Update(s).mode == SuitMode::Cloak);
		SuitInput fire = s; fire.now = 0.3f; fire.firing = true;
		assert(p.Update(fire).mode == SuitMode::Armor);
		SuitInput off = s; off.cloakAllowed = false; BotSuitPolicy q; assert(q.Update(off).mode != SuitMode::Cloak);
		SuitInput near = s; near.enemyDistance = 8; BotSuitPolicy r; assert(r.Update(near).mode != SuitMode::Cloak);
	}
	// 6. Hysteresis: alternating travel distance around the threshold does not ping-pong the suit.
	{
		BotSuitPolicy p; int changes = 0;
		for (int i = 0; i < 20; ++i) changes += p.Update(Travel(i * 0.1f, 140, (i % 2) ? 25.f : 5.f)).changed;
		assert(changes <= 2);
	}
	std::puts("BotSuitPolicyTests OK");
	return 0;
}
