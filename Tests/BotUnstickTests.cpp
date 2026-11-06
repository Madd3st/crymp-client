// SPEC-004 engine-free tests: rows of the situations table are named in the comments.
#include "CryGame/Bots/BotUnstick.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
using namespace WarsBots;
struct V { float x, y, z; };
static bool Near(float a, float b, float e = 1e-3f) { return std::fabs(a - b) <= e; }
// The step tests below were written for 2.5 s attempts; the server fix of 06.11 shortened them (see Episodes()).
static UnstickSteps Legacy() { UnstickSteps s; s.sprintTime = s.jumpTime = s.strengthTime = 2.5f; return s; }

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
	{   // row 13: a walking bot replanned onto a longer path is never stuck (progress along the new path)
		UnstickSensor s; float rem = 30; int sig = 1;
		for (float t = 0; t <= 30.f; t += 0.25f)
		{
			if (std::fmod(t, 5.f) < 0.01f && t > 0) { rem += 20.f; ++sig; }   // Steer repath every 5 s, +20 m each
			assert(!s.Update(rem, float(sig), t, false));
			rem -= 0.75f;                                                    // 3 m/s
		}
	}
	{   // a shorter new path is not progress by itself: the clock keeps running
		UnstickSensor s;
		s.Update(20, 1, 0, false);
		assert(!s.Update(15, 2, 2, false) && !s.Progressed());
		assert(s.Update(15, 2, 4, false));
	}
	{   // Codex review 96a09959: standing still, alternating 20/22 m replans every 3 s -> stuck at 4 s, no false progress
		UnstickSensor s; int falseProgress = 0; float firstStuck = -1;
		for (float t = 0; t <= 120.f; t += 0.25f)
		{
			const int k = int(t / 3.f);
			const bool stuck = s.Update(k % 2 ? 22.f : 20.f, float(k + 1), t, false);
			falseProgress += s.Progressed();
			if (stuck && firstStuck < 0) firstStuck = t;
			if (t >= 4.f) assert(stuck);
		}
		assert(falseProgress == 0 && firstStuck == 4.f);
	}
	{   // the same churn through the steps: a chain runs, never "exits" without real progress
		UnstickSensor s; UnstickSteps st = Legacy(); int exits = 0, starts = 0;
		for (float t = 0; t <= 60.f; t += 0.25f)
		{
			const int k = int(t / 3.f);
			UnstickInput in; in.stuck = s.Update(k % 2 ? 22.f : 20.f, float(k + 1), t, false); in.escaped = s.Progressed();
			in.canStrength = true; in.transition = 77;
			st.Update(t, in);
			exits += st.Event() == UnstickEvent::Exited; starts += st.Event() == UnstickEvent::Started;
			if (st.Event() == UnstickEvent::Failed) s.Reset();
		}
		assert(exits == 0 && starts > 3);
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
		UnstickSteps st = Legacy(); UnstickInput in; in.stuck = true; in.canStrength = true; in.transition = 42;
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
		UnstickSteps st = Legacy(); UnstickInput in; in.stuck = true; in.canJump = false; in.canStrength = false; in.transition = 7;
		assert(st.Update(0, in) == UnstickStep::Sprint);
		assert(st.Update(2.5f, in) == UnstickStep::Backoff && !st.StrengthUsed(7));
		assert(st.Closed(7, 3.f) && !st.Closed(7, 33.f));         // back-off ban is 30 s
	}
	{   // Codex review of 3834ac64: each jump step stands on its own probe. Matrix (normal probe, strength probe):
		struct Row { bool normal, strength; UnstickStep at25, at50; };
		const Row rows[] = {
			{false, true,  UnstickStep::Strength, UnstickStep::Backoff},   // normal landing bad, strength landing safe
			{false, false, UnstickStep::Backoff,  UnstickStep::Backoff},   // ceiling for both
			{true,  false, UnstickStep::Jump,     UnstickStep::Backoff},   // normal only, no strength
			{true,  true,  UnstickStep::Jump,     UnstickStep::Strength}};
		for (const Row& r : rows)
		{
			UnstickSteps st = Legacy(); UnstickInput in; in.stuck = true; in.transition = 77;
			const UnstickJumpAbility a = UnstickJumpInput(true, r.normal, r.strength, 200.f, false);
			in.canJump = a.canJump; in.canStrength = a.canStrength;
			assert(st.Update(0, in) == UnstickStep::Sprint);
			assert(st.Update(2.5f, in) == r.at25);
			if (r.at25 != UnstickStep::Backoff) assert(st.Update(5.f, in) == r.at50);
			const bool spent = r.strength;
			assert(st.StrengthUsed(77) == spent);
			if (spent)
			{   // no second strength on the same transition, even with a safe probe
				if (st.Step() == UnstickStep::Strength)
				{ in.escaped = true; assert(st.Update(6.f, in) == UnstickStep::None && st.Event() == UnstickEvent::Exited); in.escaped = false; }
				else { assert(st.Step() == UnstickStep::Backoff); assert(st.Update(8.f, in) == UnstickStep::None && st.Event() == UnstickEvent::Failed); }   // no exit inside back-off
				assert(st.Update(10.f, in) == UnstickStep::Sprint);
				st.Update(12.5f, in); st.Update(15.f, in);
				assert(st.Step() != UnstickStep::Strength);
			}
		}
		// the caller's gates on the strength probe: energy, crowd, and no probe at all when not needed
		assert(!UnstickJumpInput(true, false, true, 129.f, false).canStrength);
		assert(!UnstickJumpInput(true, false, true, 200.f, true).canStrength);
		const UnstickJumpAbility idle = UnstickJumpInput(false, true, true, 200.f, false);
		assert(!idle.canJump && !idle.canStrength);
	}
	{   // success by progress, whatever the step; strength exit is reported for the memory
		UnstickSteps st = Legacy(); UnstickInput in; in.stuck = true; in.canStrength = true; in.transition = 9;
		st.Update(0, in); st.Update(2.5f, in); st.Update(5.f, in);
		assert(st.Step() == UnstickStep::Strength);
		in.escaped = true;
		assert(st.Update(5.5f, in) == UnstickStep::None && st.Event() == UnstickEvent::Exited && st.LastExitByStrength());
		assert(!st.Closed(9, 6.f));
	}
	{   // not stuck -> nothing; stuck in the air -> waits for the ground
		UnstickSteps st = Legacy(); UnstickInput in; assert(st.Update(0, in) == UnstickStep::None);
		in.stuck = true; in.grounded = false; assert(st.Update(1, in) == UnstickStep::None);
	}

	// --- memory clock across matches: ages without new records, survives a timer restart ---
	{
		UnstickClockState c; c.Start(100.f);
		assert(c.Now(50.f) == 100.f && !c.Due(50.f));
		assert(c.Now(110.f) == 160.f && c.Due(110.f) && !c.Due(111.f));   // periodic save every 60 s
		assert(c.Now(5.f) == 160.f && c.Now(15.f) == 170.f);                 // timer restarted: total kept
		assert(c.Last() == 170.f);
		TransitionMemory m; m.Record(0, 0, 0, 0, 6, 3, 100.f);
		const std::string file = m.Save("h", c.Last());                         // saved by the clock, no new record
		TransitionMemory next; UnstickClockState c2; c2.Start(next.Load(file, "h"));
		const float later = c2.Now(0.f) + 1800.f - 70.f;                      // next match: 30 min after the record
		assert(Near(next.Penalty(TransitionKey(0, 0, 0, 6), later), 0.5f, 0.01f));
	}
	// --- allies: same non-zero team only; Instant Action has none ---
	assert(UnstickAlly(1, 1) && UnstickAlly(2, 2) && !UnstickAlly(1, 2) && !UnstickAlly(0, 0) && !UnstickAlly(0, 1));

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
	{   // D-042 path timing: nearest-rank percentiles, window vs level, period, timer restart, cap
		PathQueryTiming t; t.levelCap = 150;
		assert(t.TakeWindow().n == 0 && t.Level().n == 0);
		for (int i = 1; i <= 100; ++i) t.Add(float(i) * 0.01f);       // 0.01 .. 1.00 ms
		PathTimeReport w = t.TakeWindow();
		assert(w.n == 100 && std::fabs(w.p50 - 0.50f) < 1e-4f && std::fabs(w.p95 - 0.95f) < 1e-4f && std::fabs(w.max - 1.f) < 1e-4f);
		assert(t.TakeWindow().n == 0 && t.Level().n == 100);           // the window empties, the level keeps all
		for (int i = 0; i < 99; ++i) t.Add(0.1f);
		t.Add(9.f);                                                    // one slow query among 100: p95 stays low, max shows it
		w = t.TakeWindow(); assert(w.n == 100 && w.p95 < 0.2f && w.max == 9.f);
		assert(t.Level().n == 150 && t.Dropped() == 50);               // capped level sample, drops are counted
		assert(!t.Due(10.f) && !t.Due(69.9f) && t.Due(70.f) && !t.Due(71.f));
		assert(!t.Due(5.f) && t.Due(65.f));                            // a restarted timer starts a new period
		const PathTimeReport one = PathTimePercentiles({3.f}); assert(one.n == 1 && one.p50 == 3.f && one.p95 == 3.f);
		t.Clear(); assert(t.Level().n == 0 && t.Dropped() == 0);
	}
	// --- server run 3ba5119c (Codex 06.11, E:/WarsBots/QA/PR20-3ba5119c/Runs/Shore-ON-01): the three stalls >= 10 s ---
	// MOVE TRACE positions (s from the stall start, x, y) and the corner each bot could not reach. On the server the bot
	// got a new path every ~1.5-5 s and "progressed" 0.4-1.3 m on it while sliding: the old sensor restarted its clock
	// and the steps took that for an exit. Replay: same positions, a new path signature every 1.5 s, the remaining
	// length = distance to the corner + 120 m (so sliding toward the corner looks like progress, as on the server).
	{
		struct P { float t, x, y; };
		struct Episode { const char* who; float cx, cy; std::vector<P> p; };
		const Episode eps[] = {
			{"131018 slope (2111,1376) 229-245 s", 2107.9f, 1374.3f, {{0.04f,2114.3f,1377.3f},{0.27f,2114.1f,1377.3f},{0.52f,2114.4f,1376.9f},{0.79f,2114.1f,1377.8f},{1.02f,2114.4f,1377.3f},{1.29f,2114.3f,1377.1f},{1.51f,2113.5f,1377.8f},{1.79f,2113.7f,1377.1f},{2.03f,2113.9f,1376.6f},{2.27f,2113.6f,1377.3f},{2.52f,2113.3f,1377.9f},{2.77f,2113.6f,1377.1f},{3.03f,2113.8f,1376.5f},{3.3f,2113.5f,1377.3f},{3.53f,2113.6f,1377.3f},{3.78f,2113.8f,1376.7f},{4.04f,2113.7f,1377.1f},{4.28f,2113.6f,1377.2f},{4.55f,2113.7f,1377.1f},{4.77f,2113.4f,1377.6f},{5.03f,2113.1f,1378.4f},{5.26f,2113.1f,1377.7f},{5.52f,2113.0f,1377.8f},{5.77f,2112.8f,1378.1f},{6.03f,2112.9f,1377.1f},{6.27f,2112.9f,1377.3f},{6.53f,2112.6f,1377.9f},{6.77f,2112.8f,1377.6f},{7.03f,2113.0f,1377.3f},{7.27f,2112.6f,1378.0f},{7.55f,2112.5f,1377.7f},{7.77f,2112.8f,1377.0f},{8.04f,2112.4f,1377.7f},{8.28f,2112.4f,1377.7f},{8.52f,2112.8f,1377.1f},{8.79f,2113.3f,1376.4f},{9.02f,2113.4f,1376.0f},{9.28f,2113.2f,1376.7f},{9.54f,2113.1f,1377.0f},{9.77f,2113.4f,1376.5f},{10.03f,2113.3f,1376.8f},{10.28f,2113.1f,1377.2f},{10.53f,2113.4f,1376.4f},{10.77f,2113.3f,1376.7f},{11.04f,2113.2f,1377.4f},{11.27f,2113.5f,1376.8f},{11.53f,2113.6f,1376.5f},{11.79f,2113.5f,1377.1f},{12.03f,2113.5f,1376.8f},{12.29f,2113.7f,1376.3f},{12.52f,2113.4f,1377.1f},{12.78f,2113.1f,1377.7f},{13.04f,2113.6f,1378.3f},{13.27f,2114.3f,1378.8f},{13.52f,2114.2f,1378.8f},{13.77f,2113.7f,1377.8f},{14.03f,2113.9f,1376.7f},{14.29f,2114.0f,1375.7f},{14.52f,2113.9f,1375.8f},{14.77f,2113.7f,1376.7f},{15.02f,2113.6f,1376.6f},{15.27f,2113.7f,1375.5f},{15.53f,2113.5f,1375.7f},{15.77f,2113.5f,1376.5f}}},
			{"130988 throat (1985,1241) 123-141 s", 1985.9f, 1234.1f, {{0.02f,1985.2f,1241.9f},{0.28f,1984.3f,1241.8f},{0.51f,1985.0f,1241.7f},{0.77f,1985.4f,1241.7f},{1.02f,1984.9f,1241.6f},{1.29f,1984.4f,1241.4f},{1.54f,1985.3f,1241.2f},{1.78f,1985.3f,1241.2f},{2.03f,1984.5f,1241.2f},{2.28f,1985.2f,1240.9f},{2.52f,1985.3f,1240.7f},{2.79f,1984.3f,1240.6f},{3.04f,1983.5f,1240.5f},{3.29f,1982.6f,1240.3f},{3.53f,1983.5f,1240.1f},{3.78f,1984.0f,1240.2f},{4.02f,1983.3f,1239.9f},{4.27f,1982.9f,1239.8f},{4.53f,1983.7f,1240.1f},{4.77f,1984.1f,1240.3f},{5.03f,1983.5f,1240.1f},{5.28f,1982.7f,1239.9f},{5.53f,1983.3f,1240.1f},{5.78f,1984.1f,1240.3f},{6.03f,1983.5f,1240.2f},{6.28f,1982.8f,1240.0f},{6.53f,1983.3f,1240.2f},{6.8f,1983.4f,1240.4f},{7.03f,1982.9f,1240.2f},{7.28f,1983.3f,1240.3f},{7.54f,1983.6f,1240.6f},{7.79f,1983.0f,1240.4f},{8.04f,1983.5f,1240.3f},{8.29f,1983.9f,1240.3f},{8.53f,1983.2f,1240.2f},{8.78f,1982.8f,1240.1f},{9.03f,1983.7f,1240.0f},{9.28f,1984.0f,1240.3f},{9.52f,1983.7f,1241.0f},{9.77f,1983.4f,1241.8f},{10.03f,1983.0f,1242.7f},{10.29f,1982.7f,1243.4f},{10.52f,1983.1f,1242.9f},{10.78f,1983.3f,1242.0f},{11.04f,1983.3f,1242.6f},{11.28f,1983.0f,1243.4f},{11.53f,1983.3f,1242.8f},{11.78f,1983.5f,1241.8f},{12.03f,1983.5f,1242.6f},{12.3f,1983.4f,1242.9f},{12.52f,1983.5f,1242.9f},{12.77f,1983.6f,1242.9f},{13.04f,1983.7f,1242.8f},{13.29f,1983.8f,1242.8f},{13.53f,1984.0f,1242.8f},{13.78f,1984.1f,1242.7f},{14.03f,1984.2f,1242.7f},{14.27f,1984.3f,1242.7f},{14.53f,1984.3f,1242.7f},{14.79f,1984.5f,1242.7f},{15.02f,1984.6f,1242.7f},{15.27f,1984.7f,1242.7f},{15.54f,1984.9f,1242.7f},{15.79f,1985.0f,1242.7f},{16.04f,1985.1f,1242.8f},{16.29f,1985.3f,1242.8f},{16.55f,1985.3f,1242.9f},{16.77f,1985.4f,1242.9f},{17.02f,1985.4f,1242.9f}}},
			{"131008 throat (1985,1241) 155-165 s", 1985.9f, 1238.2f, {{0.04f,1985.0f,1242.0f},{0.29f,1985.8f,1241.9f},{0.53f,1985.1f,1241.8f},{0.78f,1984.4f,1241.7f},{1.03f,1985.3f,1241.5f},{1.28f,1984.7f,1241.4f},{1.52f,1984.2f,1241.2f},{1.77f,1985.4f,1240.9f},{2.04f,1985.4f,1240.8f},{2.29f,1985.7f,1240.8f},{2.54f,1986.3f,1240.9f},{2.79f,1985.5f,1241.0f},{3.02f,1984.8f,1241.0f},{3.27f,1984.4f,1240.8f},{3.54f,1985.6f,1240.6f},{3.78f,1986.2f,1240.8f},{4.03f,1985.6f,1240.8f},{4.27f,1985.1f,1240.9f},{4.52f,1985.8f,1241.0f},{4.77f,1985.5f,1241.0f},{5.02f,1984.7f,1241.0f},{5.27f,1985.2f,1240.9f},{5.54f,1986.0f,1241.0f},{5.77f,1986.5f,1241.0f},{6.03f,1987.4f,1240.8f},{6.28f,1986.6f,1240.8f},{6.53f,1986.4f,1240.9f},{6.78f,1987.2f,1240.8f},{7.02f,1986.4f,1240.7f},{7.28f,1986.2f,1240.8f},{7.52f,1987.0f,1240.7f},{7.79f,1987.0f,1240.7f},{8.02f,1986.1f,1240.6f},{8.27f,1986.5f,1240.6f}}},
		};
		for (const Episode& e : eps)
		{
			for (int probes = 0; probes < 2; ++probes)   // 0: both jump probes fail (as on the server), 1: both pass
			{
				UnstickSensor s; UnstickSteps st; float firstStuck = -1.f, backoffAt = -1.f; int exits = 0;
				float ax = 0, ay = 0;
				for (const P& q : e.p)
				{
					const float rem = 120.f + std::sqrt((q.x - e.cx) * (q.x - e.cx) + (q.y - e.cy) * (q.y - e.cy));
					const float xy[2] = {q.x, q.y};
					UnstickInput in;
					in.stuck = s.Update(rem, float(1 + int(q.t / 1.5f)), q.t, false, xy);
					if (in.stuck && firstStuck < 0) firstStuck = q.t;
					in.canJump = probes == 1; in.canStrength = probes == 1; in.transition = 5;
					in.escaped = st.Step() != UnstickStep::None && UnstickEscaped(ax, ay, e.cx, e.cy, q.x, q.y);
					const UnstickStep was = st.Step();
					st.Update(q.t, in);
					if (was == UnstickStep::None && st.Step() != UnstickStep::None) { ax = q.x; ay = q.y; }
					if (st.Step() == UnstickStep::Backoff && backoffAt < 0) backoffAt = q.t;
					exits += st.Event() == UnstickEvent::Exited;
				}
				std::printf("episode %s probes=%d: stuck %.2f s, back-off %.2f s, exits %d\n", e.who, probes, firstStuck, backoffAt, exits);
				fflush(stdout); assert(firstStuck >= 0.f && firstStuck <= 3.3f);       // noticed by the place rule within 3 s
				assert(exits == 0);                                     // never moved 2.5 m toward the corner: no exit
				const float limit = probes ? 9.0f : 5.1f;                // 3 s + attempts (+1 sample at 4 Hz), well before 10 s
				assert(e.p.back().t < limit || (backoffAt >= 0.f && backoffAt <= limit));
			}
		}
	}
	{   // escape criterion: 2.5 m toward the corner or reaching it; sliding back, sideways or 1.3 m never count
		assert(!UnstickEscaped(0, 0, 10, 0, 1.3f, 0));
		assert(!UnstickEscaped(0, 0, 10, 0, -3.f, 0));
		assert(!UnstickEscaped(0, 0, 10, 0, 0, 3.f));
		assert(UnstickEscaped(0, 0, 10, 0, 2.5f, 0.5f));
		assert(UnstickEscaped(0, 0, 2, 0, 1.2f, 0));                 // short leg: within 1 m of the corner
		UnstickSteps st; UnstickInput in; in.stuck = true; in.canJump = false; in.canStrength = false; in.transition = 3;
		st.Update(0, in); st.Update(1.5f, in); assert(st.Step() == UnstickStep::Backoff);
		in.escaped = true; assert(st.Update(2.f, in) == UnstickStep::Backoff && st.Event() != UnstickEvent::Exited);   // no exit in back-off
		UnstickSteps ok; UnstickInput go; go.stuck = true; go.canStrength = true; go.transition = 4;
		ok.Update(0, go); ok.Update(1.5f, go); ok.Update(3.f, go); assert(ok.Step() == UnstickStep::Strength);
		go.escaped = true; assert(ok.Update(3.5f, go) == UnstickStep::None && ok.Event() == UnstickEvent::Exited && ok.LastExitByStrength());
	}
	{   // place rule: walking normally never trips it; 1-2 m oscillation does at 4 s even with "progress" on new paths
		UnstickSensor walk;
		for (float t = 0; t < 30.f; t += 0.25f) { const float xy[2] = {t * 3.f, 0}; assert(!walk.Update(100.f - t * 3.f, 1, t, false, xy)); }
		UnstickSensor osc; float first = -1;
		for (float t = 0; t < 12.f; t += 0.25f)
		{
			const float xy[2] = {std::fmod(t, 1.f) < 0.5f ? 0.f : 1.8f, 0};
			const bool st = osc.Update(130.f - (std::fmod(t, 1.5f) < 0.75f ? 0.f : 1.3f), float(1 + int(t / 1.5f)), t, false, xy);
			if (st && first < 0) first = t;
		}
		assert(first >= 3.f && first <= 3.25f);
	}
	{   // back-off target away from allies (server: 4 bots within 5 m at 133-138 s after backing into the followers)
		struct Q { float x, y; };
		const Q c[4] = {{-6, 0}, {0, 4}, {0, -4}, {-5, 0}};             // previous corner, left, right, straight back
		const Q allies[3] = {{-6, 1}, {-5, -1}, {-1, -4}};             // two coming up the path, one on the right
		assert(UnstickPickBackoff(c, 4, allies, 3) == 1);              // the left side is free
		assert(UnstickPickBackoff(c, 4, (const Q*)nullptr, 0) == 0);   // no allies: the previous corner, as before
	}
	std::puts("BotUnstick OK");
	return 0;
}
