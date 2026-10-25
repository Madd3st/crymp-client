// WarsBots: per-bot behaviour parameters. One AI core, different tunings.
// Skill changes decisions and execution timing, never perception rules, health or damage.
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
    float aimErrorFloor = 0.06f; // metres: close-range tracking never becomes exact
    float trackingLag = 0.15f;   // seconds: observation-following, not target prediction
    float headPreference = 0.06f; // strategic zone choice, separate from aim error
    // wb_bot_difficulty (BotDifficulty.h). Defaults reproduce the pre-difficulty behaviour exactly.
    float headCap = 0.3f;         // cap of the head-zone chance
    float fireTolDeg = 16.26f;    // fire gate: weapon within this angle of the aim point (dot 0.96)
    float turnRate = 0.0f;        // deg/s view cap; 0 = old per-skill table in BotBrain::TurnRate
    float sightRange = 80.0f;     // m: scan / perception range
    float coneDeg = 110.0f;       // perception view cone
    float hearScale = 1.0f;       // hearing distances multiplier
	// Chance to Strength-jump right after being hit in a fight (dodge), per hit event.
	float dodgeChance = 0.3f;
	// Tactical competence is separate from style. All inputs must be own state or visible cues.
	float rangeAdapt = 0.35f;
	float retreatHealth = 22.0f;
	float targetJudgement = 0.35f;
	float targetSwitchMargin = 0.35f;
	float targetHold = 2.0f;
	float blockedChasePatience = 2.0f;
	bool useSpeed = true;

	float WeaponRange(const char* weapon, unsigned salt) const
	{
		float range = preferredRange;
		if (weapon)
		{
			if (!strcmp(weapon,"SOCOM")) range=10.0f;
			else if (!strcmp(weapon,"Shotgun")) range=6.0f;
			else if (!strcmp(weapon,"SMG")) range=12.0f;
			else if (!strcmp(weapon,"SCAR") || !strcmp(weapon,"FY71")) range=18.0f;
			else if (!strcmp(weapon,"DSG1") || !strcmp(weapon,"GaussRifle")) range=35.0f;
			else if (!strcmp(weapon,"Hurricane")) range=20.0f;
		}
		const float styleScale = style==BotStyle::Assault ? 0.7f : style==BotStyle::Marksman ? 1.2f :
			(style==BotStyle::Flanker || style==BotStyle::Hunter) ? 0.8f : 1.0f;
		const float result = preferredRange + (range*styleScale-preferredRange)*rangeAdapt*0.65f;
		// Stable within an encounter/life: beginners sometimes choose an imperfect distance.
		return skill==BotSkill::Beginner ? result*((salt&1u)?1.25f:0.8f) : result;
	}

	float NextTargetBurst(float existingDeadline, float now) const
	{ return existingDeadline>now+reaction?existingDeadline:now+reaction; }

	bool CanSwitchTarget(float heldSeconds, float currentScore, float newScore) const
	{ return heldSeconds>=targetHold && newScore>=currentScore+targetSwitchMargin; }

	float TargetUtility(float distance, float desiredRange, bool clearMuzzle, bool facingUs,
		bool retaliator, float ownHealth, bool hasAmmo) const
	{
		auto bound=[](float x) { return x<0.0f?0.0f:x>1.0f?1.0f:x; };
		const float difference=distance>desiredRange?distance-desiredRange:desiredRange-distance;
		const float fit=1.0f-bound(difference/(desiredRange>8.0f?desiredRange:8.0f));
		const float threat=facingUs?(ownHealth<40.0f?-0.35f:0.25f):0.0f;
		return 1.0f-bound(distance/80.0f)+targetJudgement*(0.7f*fit+
			(clearMuzzle&&hasAmmo?0.35f:0.0f)+threat+(retaliator?0.8f:0.0f));
	}

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
		static const float dodge[] = { 0.0f, 0.3f, 0.45f, 0.6f };
		static const float reaction[] = { 0.75f, 0.35f, 0.25f, 0.22f };
		static const float aim[] = { 2.0f, 1.0f, 0.7f, 0.45f };
		static const float gap[] = { 1.35f, 1.0f, 0.9f, 0.85f };
		const int s = int(skill) >= 0 && int(skill) < int(BotSkill::Count) ? int(skill) : 1;
		static const float adaptation[] = {0.0f,0.35f,0.75f,1.0f};
		static const float retreat[] = {0.0f,22.0f,35.0f,45.0f};
		static const float judgement[] = {0.0f,0.35f,0.75f,1.0f};
		static const float margin[] = {0.45f,0.35f,0.28f,0.25f};
		static const float hold[] = {3.0f,2.0f,1.6f,1.3f};
		static const float rhythm[] = {1.6f,1.0f,0.9f,0.8f};
		p.rangeAdapt=adaptation[s]; p.retreatHealth=retreat[s]; p.targetJudgement=judgement[s];
		p.blockedChasePatience=s==0?4.0f:s==1?2.0f:s==2?1.5f:1.2f;
		p.targetSwitchMargin=margin[s]; p.targetHold=hold[s]; p.useSpeed=s>0;
		p.strafePeriod*=rhythm[s];
		p.reaction = reaction[s];
		p.dodgeChance = dodge[s] + ((style == BotStyle::Assault || style == BotStyle::Flanker) && s > 0 ? 0.15f : 0.0f);
		p.aimErrorPerMetre *= aim[s];
		p.aimErrorMax *= aim[s];
        static const float floors[]={.10f,.07f,.05f,.035f};
        static const float lag[]={.22f,.16f,.13f,.11f};
        static const float head[]={.02f,.06f,.10f,.16f};
        p.aimErrorFloor=floors[s];p.trackingLag=lag[s];p.headPreference=head[s];
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
