// WarsBots difficulty ladder (DS-014, owner 25.10: "difficulties feel the same; Elite = honest head aimbot").
// One table per skill that changes reaction, turn speed, aim error, aim zone, fire discipline, engagement range and
// perception together, so the gap is felt within 30 s. Elite: ~80 ms after a clear LOS, near-zero tracking error,
// head when the head ray is clear, fires only within 2 deg of the target - still no wallhacks (LOS rules unchanged).
// Applied when wb_bot_difficulty=1 (default 0 keeps today's values). Pure logic; Tests/BotDifficultyTests.cpp.
#pragma once
#include <cmath>
#include "CryGame/Bots/BotProfile.h"
#include "CryGame/Bots/BotAimModel.h"

namespace WarsBots {

struct DifficultyRow {
	float reaction;       // s from noticing to the first burst
	float turnDeg;        // deg/s view turn cap
	float aimErrDeg;      // extra aim error at range (deg), on top of weapon spread
	float aimMaxM;        // m, cap of that error
	float aimFloorM;      // m, close-range floor
	float trackLag;       // s, observation-following lag
	float headPref;       // chance to choose the head zone (needs a clear head ray)
	float headCap;        // cap of that chance (old code: 0.3 for everyone)
	float fireTolDeg;     // fire only when the weapon points within this angle of the aim point (old: ~16 for all)
	float rangeScale;     // engage / hold / fire range multiplier
	float sightM;         // perception range (old: 80 for all)
	float coneDeg;        // view cone (old: 110 for all)
	float hearScale;      // hearing distances multiplier
	float zoomScale, burstScale, gapScale; // C-093: Elite headshots 7.8 % - hip spread ate the head aim; zoom early, tap fire
	// aim model (BotAimModel) when wb_bot_aim_model=1
	float saccadeBase, saccadeSlope, flickError, modelTrackLag, lead, noiseDeg;
};

inline const DifficultyRow& Difficulty(BotSkill skill)
{
	static const DifficultyRow rows[] = {
		// reac  turn  err  max  floor lag   head cap   tol  range sight cone hear | sacc  slope flick lag   lead  noise
		{ 0.90f, 250, 2.5f, .80f, .12f, .24f, .00f, .00f, 20.f, .55f, 50.f, 90.f, .6f, 1.5f, 1.6f, 1.4f, .22f, .070f, .25f, .24f, .25f, 1.6f },  // beginner
		{ 0.50f, 500, 1.2f, .45f, .07f, .14f, .08f, .15f, 10.f, .85f, 65.f, 100.f, .8f, 1.0f, 1.0f, 1.0f, .13f, .045f, .12f, .13f, .55f, 0.7f }, // normal
		{ 0.28f, 850, 0.6f, .25f, .04f, .09f, .35f, .50f,  5.f, 1.15f, 80.f, 110.f, 1.f, 0.7f, 0.7f, .75f, .09f, .035f, .06f, .08f, .75f, .35f }, // veteran
		{ 0.08f, 1200, 0.1f, .05f, .01f, .04f, .95f, 1.0f, 2.f, 1.45f, 100.f, 120.f, 1.2f, 0.4f, .45f, .5f, .05f, .020f, .02f, .04f, .95f, .05f },// elite
	};
	const int s = int(skill) >= 0 && int(skill) < int(BotSkill::Count) ? int(skill) : 1;
	return rows[s];
}

// Overwrites the skill-dependent fields of a profile already built by BotProfile::Make (style stays).
inline void ApplyDifficulty(BotProfile& p)
{
	const DifficultyRow& d = Difficulty(p.skill);
	p.reaction = d.reaction;
	p.aimErrorPerMetre = std::tan(d.aimErrDeg * 3.14159265f / 180.f);
	p.aimErrorMax = d.aimMaxM;
	p.aimErrorFloor = d.aimFloorM;
	p.trackingLag = d.trackLag;
	p.headPreference = d.headPref;
	p.headCap = d.headCap;
	p.fireTolDeg = d.fireTolDeg;
	p.turnRate = d.turnDeg;
	p.engageRange *= d.rangeScale; p.holdRange *= d.rangeScale; p.fireRange *= d.rangeScale;
	p.sightRange = d.sightM; p.coneDeg = d.coneDeg; p.hearScale = d.hearScale;
	p.zoomScale = d.zoomScale; p.burst *= d.burstScale; p.burstGap *= d.gapScale;
}

inline AimSkill DifficultyAim(BotSkill skill)
{
	const DifficultyRow& d = Difficulty(skill);
	AimSkill s;
	s.saccadeBase = d.saccadeBase; s.saccadeSlope = d.saccadeSlope; s.flickError = d.flickError;
	s.trackLag = d.modelTrackLag; s.leadFraction = d.lead; s.noiseDeg = d.noiseDeg; s.maxTurn = d.turnDeg;
	return s;
}

// Fire gate: allowed angle between weapon direction and the aim point. Close targets get at least the angle a
// 0.4 m body half-width subtends, so a 2 deg Elite can still hit a target jumping around it at 3 m.
inline float FireToleranceDeg(const BotProfile& p, float distance)
{
	const float body = distance > 0.1f ? std::atan(0.4f / distance) * 180.f / 3.14159265f : 90.f;
	return p.fireTolDeg > body ? p.fireTolDeg : body;
}

}
