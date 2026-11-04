// SPEC-004 engine-free tests: rows of the situations table are named in the comments.
#include "CryGame/Bots/BotUnstick.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
using namespace WarsBots;
struct V { float x, y, z; };
static bool Near(float a, float b, float e = 1e-3f) { return std::fabs(a - b) <= e; }

int main()
{
	// --- remaining path: same sum as the old inline wb_bot_path_progress code (row 20 keeps the weapon goal) ---
	std::vector<V> path{{0, 0, 0}, {10, 0, 0}, {10, 10, 0}};
	V feet{0, -3, 0};
	assert(Near(PathRemaining(path, 0, feet), 3.f + 10.f + 10.f));
	assert(Near(PathRemaining(path, 1, feet), std::sqrt(109.f) + 10.f));
	assert(Near(PathRemaining(path, 7, feet), std::sqrt(100.f + 169.f)));   // index past the end -> last corner only
	assert(PathRemaining(std::vector<V>{}, 0, feet) == 0.f);
	assert(Near(PathSignature(path), 0.f * 0.37f + 0.f * 1.13f + 3.f * 7.f));
	{   // signature rule: longer new path restarts, shorter keeps the clock, same path does nothing
		float sig = 0, best = 20, at = 1;
		assert(ApplyPathSignature(sig, best, at, 5.f, 30.f, 9.f) && best == 30.f && at == 9.f);
		assert(!ApplyPathSignature(sig, best, at, 5.005f, 50.f, 10.f) && best == 30.f);
		assert(ApplyPathSignature(sig, best, at, 6.f, 25.f, 11.f) && best == 30.f && at == 9.f);
	}

	// --- sensor ---
	{
		UnstickSensor s;
		assert(!s.Update(20, 1, 0, false));
		assert(!s.Update(19.5f, 1, 3.9f, false));
		assert(s.Update(19.5f, 1, 4.0f, false));               // row 1: running in place on the slope, 4 s
		assert(!s.Update(18.4f, 1, 4.5f, false) && s.Progressed());
		assert(!s.Update(18.4f, 1, 8.4f, false) && s.Update(18.4f, 1, 8.5f, false));
	}
	{   // row 13: replanned onto a longer path is not stuck (restart from its own length)
		UnstickSensor s;
		s.Update(20, 1, 0, false); s.Update(20, 1, 3, false);
		assert(!s.Update(40, 2, 3.5f, false));
		assert(!s.Update(39.5f, 2, 7.4f, false));
		assert(s.Update(39.5f, 2, 7.5f, false));
	}
	{   // repath churn: a new longer path every 3 s never reaches the 4 s window; the 12 s guard still fires
		UnstickSensor s; float rem = 20; int sig = 1; bool fired = false; float when = 0;
		for (float t = 0; t <= 14.f && !fired; t += 0.25f)
		{
			if (std::fmod(t, 3.f) < 0.01f && t > 0) { rem += 1.f; ++sig; }
			if (s.Update(rem, float(sig), t, false)) { fired = true; when = t; }
		}
		assert(fired && when >= 12.f && when < 12.3f);
	}
	{   // row 14: remaining under 2.5 m never arms
		UnstickSensor s;
		for (float t = 0; t < 20; t += 0.5f) assert(!s.Update(2.4f, 1, t, false));
		assert(!s.Armed());
	}
	{   // rows 6-9, 22, 23: muted (link, ladder, hold, combat, water) does not accumulate
		UnstickSensor s;
		s.Update(20, 1, 0, false);
		for (float t = 0.5f; t < 12.5f; t += 0.5f) assert(!s.Update(20, 1, t, true));   // 12 s ladder
		assert(!s.Update(20, 1, 13.f, false) && !s.Update(20, 1, 16.9f, false));
		assert(s.Update(20, 1, 17.f, false));
	}
	{   // clock rollback (new level) disarms
		UnstickSensor s; s.Update(20, 1, 100, false); assert(!s.Update(20, 1, 5, false) && !s.Armed());
	}

	// --- steps ---
	{
		UnstickSteps st; UnstickInput in; in.stuck = true; in.canStrength = true; in.transition = 42;
		assert(st.Update(0, in) == UnstickStep::Sprint && st.Event() == UnstickEvent::Started);
		assert(st.Update(2.4f, in) == UnstickStep::Sprint);
		assert(st.Update(2.5f, in) == UnstickStep::Jump);
		in.grounded = false;                                    // row 10: in the air -> no next step
		assert(st.Update(6.f, in) == UnstickStep::Jump);
		in.grounded = true;
		assert(st.Update(6.1f, in) == UnstickStep::Strength && st.StrengthUsed(42));
		assert(st.Update(8.6f, in) == UnstickStep::Backoff);
		assert(st.Closed(42, 8.6f));                            // row 25: strength failed -> closed uphill for this bot
		assert(st.Update(11.6f, in) == UnstickStep::None && st.Event() == UnstickEvent::Failed);
		// second chain on the same transition: strength is not offered again (exactly one per transition)
		assert(st.Update(12.f, in) == UnstickStep::Sprint);
		assert(st.Update(14.5f, in) == UnstickStep::Jump);
		assert(st.Update(17.f, in) == UnstickStep::Backoff);
		assert(st.Update(20.f, in) == UnstickStep::Wait);        // row 15: two failed back-offs -> wait aside
		assert(st.Update(24.9f, in) == UnstickStep::Wait);
		assert(st.Update(25.f, in) == UnstickStep::None);
		assert(st.Closed(42, 1000.f));                          // strength-closed stays closed
	}
	{   // row 12: ceiling and no energy -> straight to back off, without repeats
		UnstickSteps st; UnstickInput in; in.stuck = true; in.canJump = false; in.canStrength = false; in.transition = 7;
		assert(st.Update(0, in) == UnstickStep::Sprint);
		assert(st.Update(2.5f, in) == UnstickStep::Backoff && !st.StrengthUsed(7));
		assert(st.Closed(7, 3.f) && !st.Closed(7, 33.f));         // back-off ban is 30 s
	}
	{   // success by progress, whatever the step; strength exit is reported for the memory
		UnstickSteps st; UnstickInput in; in.stuck = true; in.canStrength = true; in.transition = 9;
		st.Update(0, in); st.Update(2.5f, in); st.Update(5.f, in);
		assert(st.Step() == UnstickStep::Strength);
		in.progressed = true;
		assert(st.Update(5.5f, in) == UnstickStep::None && st.Event() == UnstickEvent::Exited && st.LastExitByStrength());
		assert(!st.Closed(9, 6.f));
	}
	{   // not stuck -> nothing; stuck in the air -> waits for the ground
		UnstickSteps st; UnstickInput in; assert(st.Update(0, in) == UnstickStep::None);
		in.stuck = true; in.grounded = false; assert(st.Update(1, in) == UnstickStep::None);
	}

	// --- row 5: memory only for a lone bot without combat ---
	assert(UnstickShouldRecord(true, 0, false));
	assert(!UnstickShouldRecord(true, 1, false) && !UnstickShouldRecord(true, 0, true) && !UnstickShouldRecord(false, 0, false));

	// --- row 24: climb cost is directional ---
	{
		const float h = 10.f;
		auto up = [&](float deg) { return ClimbCostMultiplier(0, 0, 0, h, 0, h * std::tan(deg / 57.29578f)); };
		assert(Near(up(30.f), 1.f) && Near(up(38.f), 1.f) && Near(up(45.f), 3.f, 0.01f) && up(41.f) > 1.5f && up(41.f) < 2.f);
		assert(Near(up(60.f), 5.f));
		assert(Near(ClimbCostMultiplier(h, 0, h, 0, 0, 0), 1.f));                  // same slope downhill: base cost
		assert(Near(ClimbCostMultiplier(0, 0, 0, 0, 0, 0), 1.f));
	}

	// --- rows 16, 17, 24: memory ---
	{
		TransitionMemory m;
		m.Record(2111, 1370, 50, 2111, 1376, 55, 0);
		const uint64_t k = TransitionKey(2111, 1370, 2111, 1376);
		assert(Near(m.Penalty(k, 0), 1.f));
		m.Record(2111.5f, 1370.5f, 50, 2111.5f, 1376.5f, 55, 0); m.Record(2111, 1370, 50, 2111, 1376, 55, 0);
		m.Record(2111, 1370, 50, 2111, 1376, 55, 0);
		assert(m.Size() == 1 && Near(m.Penalty(k, 0), 3.f));           // cap: at most x4 the base
		assert(Near(m.Penalty(k, 1800), 1.5f, 0.01f));                  // half-life 30 min
		assert(m.Penalty(TransitionKey(2111, 1376, 2111, 1370), 0) == 0.f);   // reverse direction is another key
		const TransitionPenalty& p = m.Items().begin()->second;
		assert(SameDirectedTransition(p, 2111, 1371, 50, 2111.5f, 1375, 53));   // uphill, same way
		assert(!SameDirectedTransition(p, 2111, 1375, 53, 2111, 1371, 50));     // downhill: no penalty
		assert(!SameDirectedTransition(p, 2111, 1371, 50, 2115, 1371, 53));     // across
		const std::string file = m.Save("navA", 1800);
		TransitionMemory a; assert(Near(a.Load(file, "navA"), 1800.f) && a.Size() == 1);
		assert(Near(a.Penalty(k, 1800), 1.5f, 0.01f));
		TransitionMemory b; assert(b.Load(file, "navB") < 0 && b.Size() == 0); // another navmesh: nothing applied
		assert(b.Load(file, "") < 0 && b.Load("WBMEM01 1 2", "navA") < 0);
		TransitionMemory c; c.Load("WBUNSTK1 navA 10\n1 1 1 1 5 3 99 50\n1 9 1 1 13 3 -1 0\n", "navA");
		assert(c.Size() == 1 && Near(c.Items().begin()->second.value, 3.f) && Near(c.Items().begin()->second.at, 10.f));
		m.Prune(1800 * 8); assert(m.Size() == 0);
	}

	// --- row 26: spread ---
	{
		TroddenMap t;
		for (int i = 0; i < 20; ++i) t.Pass(5, 0);
		assert(Near(t.Extra(5, 0), 1.f) && Near(t.Extra(5, 120), 0.5f, 0.01f) && t.Extra(6, 0) == 0.f);
		t.Prune(2000); assert(t.Size() == 0);
		float lo = 2, hi = 0; int differ = 0;
		for (uint64_t poly = 1; poly < 2000; ++poly)
		{
			const float a = BotCostNoise(1, poly), b = BotCostNoise(2, poly);
			assert(a == BotCostNoise(1, poly));                          // fixed per bot and polygon
			lo = a < lo ? a : lo; hi = a > hi ? a : hi; differ += std::fabs(a - b) > 0.02f;
		}
		assert(lo >= 0.85f && hi <= 1.15f && lo < 0.87f && hi > 1.13f && differ > 1500);
	}
	{   // row 4: an ally in 4 m of the next 30 m of path
		std::vector<V> p{{10, 0, 0}, {40, 0, 0}};
		V f{0, 0, 0};
		assert(AllyNearPathAhead(p, 0, f, V{5, 3, 0}));
		assert(!AllyNearPathAhead(p, 0, f, V{5, 5, 0}));
		assert(!AllyNearPathAhead(p, 0, f, V{38, 1, 0}));                  // beyond the 30 m horizon
		assert(AllyNearPathAhead(p, 0, f, V{25, 1, 0}));
	}
	std::puts("BotUnstick OK");
	return 0;
}
