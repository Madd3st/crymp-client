// WarsBots: per-bot behaviour parameters. One AI core, different tunings.
// Skill changes reaction, aim error and fire discipline only: never health or damage.
#pragma once

namespace WarsBots
{
enum class BotStyle { Universal, Assault, Marksman, Flanker, Defender, Hunter, Count };
enum class BotSkill { Beginner, Normal, Veteran, Elite, Count };

struct BotProfile
{
	BotStyle style = BotStyle::Universal;
	BotSkill skill = BotSkill::Normal;

	// Combat ranges in metres. Universal/Normal reproduces the validated caf1a815 values.
	float engageRange = 18.0f;    // start attacking when the target is visible within this range
	float holdRange = 22.0f;      // keep attacking (hysteresis) until the target leaves this range
	float preferredRange = 12.0f; // strafe pattern corrects distance toward this value
	float retreatEnter = 6.0f;    // back off when closer than this
	float retreatExit = 9.0f;     // stop backing off beyond this
	float sprintChaseRange = 20.0f; // chase with native Speed sprint beyond this distance
	float fireRange = 40.0f;      // shoot a visible target while closing in up to this distance

	// Strafe rhythm: period + jitter*(life%3), as in the tested build.
	float strafePeriod = 1.8f;
	float strafeJitter = 0.2f;

	// Reaction: time aiming in ATTACKING before the first burst is allowed.
	float reaction = 0.3f;
	// Additional aim error on top of native weapon spread: min(max, distance*perMetre).
	float aimErrorPerMetre = 0.006f;
	float aimErrorMax = 0.25f;
	// Fire discipline.
	float burst = 0.2f;
	float burstGap = 0.65f;

	static BotProfile Make(BotStyle style, BotSkill skill)
	{
		BotProfile p;
		p.style = style;
		p.skill = skill;
		switch (style)
		{
		case BotStyle::Assault:
			p.engageRange = 20.0f; p.holdRange = 24.0f; p.preferredRange = 8.0f;
			p.retreatEnter = 3.5f; p.retreatExit = 5.5f; p.sprintChaseRange = 16.0f;
			p.strafePeriod = 1.4f; p.burst = 0.3f; p.burstGap = 0.55f; p.fireRange = 32.0f;
			break;
		case BotStyle::Marksman:
			p.engageRange = 28.0f; p.holdRange = 34.0f; p.preferredRange = 20.0f;
			p.retreatEnter = 9.0f; p.retreatExit = 13.0f; p.sprintChaseRange = 30.0f;
			p.strafePeriod = 2.4f; p.burst = 0.12f; p.burstGap = 0.5f;
			p.aimErrorPerMetre = 0.0045f; p.fireRange = 65.0f;
			break;
		case BotStyle::Flanker:
			p.engageRange = 18.0f; p.holdRange = 22.0f; p.preferredRange = 10.0f;
			p.strafePeriod = 1.1f; p.strafeJitter = 0.3f; p.sprintChaseRange = 15.0f;
			break;
		case BotStyle::Defender:
			p.engageRange = 22.0f; p.holdRange = 26.0f; p.preferredRange = 15.0f;
			p.retreatEnter = 7.0f; p.retreatExit = 11.0f; p.sprintChaseRange = 1000.0f; // never sprints after targets
			p.strafePeriod = 2.2f; p.fireRange = 50.0f;
			break;
		case BotStyle::Hunter:
			p.engageRange = 20.0f; p.holdRange = 26.0f; p.preferredRange = 11.0f;
			p.sprintChaseRange = 12.0f; p.strafePeriod = 1.6f;
			break;
		default: break;
		}
		static const float reaction[] = { 0.65f, 0.3f, 0.2f, 0.12f };
		static const float aim[] = { 2.0f, 1.0f, 0.7f, 0.45f };
		static const float gap[] = { 1.35f, 1.0f, 0.9f, 0.85f };
		const int s = int(skill) >= 0 && int(skill) < int(BotSkill::Count) ? int(skill) : 1;
		p.reaction = reaction[s];
		p.aimErrorPerMetre *= aim[s];
		p.aimErrorMax *= aim[s];
		p.burstGap *= gap[s];
		return p;
	}

	static const char* StyleName(BotStyle style)
	{
		static const char* names[] = { "universal", "assault", "marksman", "flanker", "defender", "hunter" };
		return int(style) >= 0 && int(style) < int(BotStyle::Count) ? names[int(style)] : "unknown";
	}
	static const char* SkillName(BotSkill skill)
	{
		static const char* names[] = { "beginner", "normal", "veteran", "elite" };
		return int(skill) >= 0 && int(skill) < int(BotSkill::Count) ? names[int(skill)] : "unknown";
	}
	static bool ParseSkill(const char* text, BotSkill& out)
	{
		for (int i = 0; text && i < int(BotSkill::Count); ++i)
			if (!_stricmp(text, SkillName(BotSkill(i)))) { out = BotSkill(i); return true; }
		return false;
	}
	static bool ParseStyle(const char* text, BotStyle& out)
	{
		for (int i = 0; text && i < int(BotStyle::Count); ++i)
			if (!_stricmp(text, StyleName(BotStyle(i)))) { out = BotStyle(i); return true; }
		return false;
	}
};
}
