// DS-014 difficulty ladder: monotonic per skill, Elite = near-perfect but LOS-honest, defaults untouched.
#include <cstring>
#ifndef _WIN32
#define _stricmp strcasecmp
#include <strings.h>
#endif
#include "CryGame/Bots/BotDifficulty.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace WarsBots;
int main()
{
	// defaults (wb_bot_difficulty=0) reproduce the old gate/cap/senses exactly
	const BotProfile n = BotProfile::Make(BotStyle::Universal, BotSkill::Normal);
	assert(std::fabs(std::cos(n.fireTolDeg * 3.14159265f / 180.f) - 0.96f) < 0.001f);
	assert(n.fightStrafe && n.fightJump < 0.f && n.reloadAt == 0.f);
	assert(n.headCap == 0.3f && n.turnRate == 0.f && n.sightRange == 80.f && n.coneDeg == 110.f && n.hearScale == 1.f);

	BotProfile p[4];
	for (int s = 0; s < 4; ++s) { p[s] = BotProfile::Make(BotStyle::Universal, BotSkill(s)); ApplyDifficulty(p[s]); }
	for (int s = 1; s < 4; ++s)
	{
		assert(p[s].reaction < p[s - 1].reaction);
		assert(p[s].turnRate > p[s - 1].turnRate);
		assert(p[s].aimErrorPerMetre < p[s - 1].aimErrorPerMetre);
		assert(p[s].fireTolDeg < p[s - 1].fireTolDeg);
		assert(p[s].headPreference > p[s - 1].headPreference);
		assert(p[s].fireRange > p[s - 1].fireRange && p[s].sightRange > p[s - 1].sightRange);
	}
	// Elite: <= 0.1 s reaction, 2 deg gate at range, head nearly always, sees 100 m
	assert(p[3].zoomScale < p[2].zoomScale && p[2].zoomScale < p[1].zoomScale && p[1].zoomScale < p[0].zoomScale);
	{ const BotProfile raw3 = BotProfile::Make(BotStyle::Universal, BotSkill::Elite); assert(p[3].burst < raw3.burst && p[3].burstGap < raw3.burstGap); }
	assert(!p[0].fightStrafe && p[1].fightStrafe && p[0].fightJump == 0.f && p[3].fightJump > p[2].fightJump && p[2].fightJump > 0.f);
	assert(p[3].cloakApproach && !p[2].cloakApproach && !n.cloakApproach);
	assert(p[0].reloadAt == 0.f && p[3].reloadAt > p[1].reloadAt);
	assert(p[3].reaction <= 0.1f && p[3].fireTolDeg == 2.f && p[3].headCap >= 0.95f && p[3].sightRange >= 100.f);
	// Elite universal fires past 50 m (owner: no bot hit beyond 40 m), Beginner stays under 25 m
	assert(p[3].fireRange > 50.f && p[0].fireRange < 25.f);
	// close range: gate opens to the body half-width (3 m -> ~7.6 deg), far keeps the skill value
	assert(FireToleranceDeg(p[3], 3.f) > 7.f && std::fabs(FireToleranceDeg(p[3], 40.f) - 2.f) < 1e-4f);
	// DS-018: 5 / 10 / 15 m -> 4.57 / 2.29 / 2.0 deg (body half-width until it drops under the skill gate)
	assert(std::fabs(FireToleranceDeg(p[3], 5.f) - 4.57f) < 0.02f && std::fabs(FireToleranceDeg(p[3], 10.f) - 2.29f) < 0.02f && FireToleranceDeg(p[3], 15.f) == 2.f);
	// applying twice is not used (restored rosters keep their profile) - ranges scale once from Make
	const BotProfile raw = BotProfile::Make(BotStyle::Marksman, BotSkill::Elite);
	BotProfile m = raw; ApplyDifficulty(m);
	// DS-035: Elite owns the 20-30 m band; caps stop a Marksman base from becoming a 100 m turret.
	assert(std::fabs(m.fireRange - 100.f) < 1e-3f);   // D-027: capped at 100, not weakened
	assert(std::fabs(m.holdRange - 44.f) < 1e-3f);
	assert(std::fabs(m.preferredRange - 32.f) < 1e-3f);

	// aim model: Elite settles near the target, Beginner keeps clearly more error
	auto settle = [](BotSkill s) {
		BotAimModel model(DifficultyAim(s), 7u);
		model.SetView(30.f, 0.f);
		AimInput in; in.dt = 0.05f; in.hasTarget = true; in.targetId = 1; in.targetYaw = 0.f; in.targetPitch = 0.f; in.targetSizeDeg = 1.f;
		float sum = 0; int k = 0;
		for (int i = 0; i < 200; ++i) { AimOutput o = model.Update(in); if (i >= 60) { sum += std::fabs(o.errorDeg); ++k; } }
		return sum / k;
	};
	const float e = settle(BotSkill::Elite), b = settle(BotSkill::Beginner);
	std::printf("settled error elite %.3f beginner %.3f deg\n", e, b);
	assert(e < 0.25f && b > 3.f * e);
	std::puts("BotDifficultyTests OK");
	return 0;
}
