#include "CryGame/Bots/BotPerception.h"
#include <cassert>
#include <cstdio>
using namespace WarsBots;

static ObserverState Obs(float t) { ObserverState o; o.now = t; o.eye = {0, 0, 1.6f}; o.forward = {0, 1, 0}; return o; }
static TargetCue Cue(float x, float y) { TargetCue c; c.id = 1; c.pos = {x, y, 1.2f}; c.losClear = true; return c; }

// seconds of continuous sight until visible (10 Hz), -1 if never within `limit`
static float TimeToSee(BotPerception& p, TargetCue c, float limit = 10.f)
{
	p.Reset();
	for (int i = 0; i <= int(limit * 10); ++i)
	{
		p.BeginTick(Obs(i * 0.1f));
		if (p.Observe(c).visible) return i * 0.1f;
	}
	return -1.f;
}

int main()
{
	BotPerception p;
	// 1. Behind the bot (180°) and at 60° off-axis (outside a 110° cone): never seen even with a clear ray.
	assert(TimeToSee(p, Cue(0, -20)) < 0.f);
	assert(TimeToSee(p, Cue(17.3f, 10)) < 0.f);                  // 60° off forward
	// 2. Close and centred is fast; cone edge is slower; far is slower still.
	const float centre = TimeToSee(p, Cue(0, 10));
	const float edge = TimeToSee(p, Cue(13.0f, 10));            // ~52°
	const float far = TimeToSee(p, Cue(0, 70));
	assert(centre > 0.f && centre <= 0.5f);
	assert(edge > centre && far > centre);
	// 3. Blocked ray: never visible.
	{ TargetCue c = Cue(0, 10); c.losClear = false; assert(TimeToSee(p, c) < 0.f); }
	// 4. Cloak: invisible at 20 m; slow to notice at 6 m; revealed when it fires at 20 m.
	{ TargetCue c = Cue(0, 20); c.cloaked = true; assert(TimeToSee(p, c) < 0.f); }
	{ TargetCue c = Cue(0, 6); c.cloaked = true; float t = TimeToSee(p, c); assert(t > centre * 3.f); }
	{ TargetCue c = Cue(0, 20); c.cloaked = true; c.sinceShot = 0.1f; assert(TimeToSee(p, c) >= 0.f && TimeToSee(p, c) <= 0.3f); }
	// 5. Hearing: a gunshot behind at 40 m is heard but not seen; silenced at 40 m is not heard.
	{
		p.Reset(); p.BeginTick(Obs(0)); TargetCue c = Cue(0, -40); c.sinceShot = 0.05f;
		TargetPercept r = p.Observe(c); assert(r.heard && !r.visible && !r.inCone);
		c.silenced = true; r = p.Observe(c); assert(!r.heard);
	}
	// 6. Skill changes reaction time only: an elite notices faster, but the cone still applies.
	{
		BotPerception e; e.SetReactionTime(0.22f); BotPerception b; b.SetReactionTime(0.6f);
		assert(TimeToSee(e, Cue(0, 10)) < TimeToSee(b, Cue(0, 10)));
		assert(TimeToSee(e, Cue(0, -20)) < 0.f);
	}
	// 7. Awareness decays when the target leaves the cone, so a re-entry is not instant after a long time away.
	{
		p.Reset(); TargetCue c = Cue(0, 30);
		for (int i = 0; i < 4; ++i) { p.BeginTick(Obs(i * 0.1f)); p.Observe(c); }
		TargetCue away = Cue(0, -30);
		float a = 1.f; for (int i = 4; i < 40; ++i) { p.BeginTick(Obs(i * 0.1f)); a = p.Observe(away).awareness; }
		assert(a == 0.f);
	}
	std::puts("BotPerceptionTests OK");
	return 0;
}
