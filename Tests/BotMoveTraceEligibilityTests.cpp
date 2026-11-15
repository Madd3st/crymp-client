// DS-076 / C145 X-218: the diagnostic MOVE TRACE must accept both supported bot modes (Instant Action and
// PowerStruggle) while keeping the opt-in flag and the existing local/server and client/multiplayer guards.
#include "CryGame/Bots/BotMoveTraceEligibility.h"
#include <cassert>
#include <cstdio>

using namespace WarsBots;

int main()
{
	// C145 blocker: PowerStruggle on the allowed local server must now be eligible (previously rejected by the
	// hardcoded InstantAction class check).
	assert(MoveTraceEligible(1, true, true, true, MoveTraceMode::PowerStruggle));
	assert(MoveTraceEligible(2, true, true, true, MoveTraceMode::PowerStruggle));

	// Instant Action keeps working on the same server gate.
	assert(MoveTraceEligible(1, true, true, true, MoveTraceMode::InstantAction));
	assert(MoveTraceEligible(1, false, false, true, MoveTraceMode::InstantAction)); // client + multiplayer

	// Client replica capture (flag 2) for PS needs multiplayer, not the server local gate.
	assert(MoveTraceEligible(2, false, false, true, MoveTraceMode::PowerStruggle));

	// Disabled trace: 0 and any unsupported value must fail.
	assert(!MoveTraceEligible(0, true, true, true, MoveTraceMode::InstantAction));
	assert(!MoveTraceEligible(0, true, true, true, MoveTraceMode::PowerStruggle));
	assert(!MoveTraceEligible(3, true, true, true, MoveTraceMode::PowerStruggle));

	// Unsupported rules class (neither IA nor PS) never enables the trace.
	assert(!MoveTraceEligible(1, true, true, true, MoveTraceMode::Unsupported));
	assert(!MoveTraceEligible(2, false, false, true, MoveTraceMode::Unsupported));

	// Existing guards stay: a non-local server and a non-multiplayer client are still rejected for both modes.
	assert(!MoveTraceEligible(1, true, false, true, MoveTraceMode::InstantAction));
	assert(!MoveTraceEligible(1, true, false, true, MoveTraceMode::PowerStruggle));
	assert(!MoveTraceEligible(1, false, true, false, MoveTraceMode::InstantAction));
	assert(!MoveTraceEligible(1, false, true, false, MoveTraceMode::PowerStruggle));

	std::puts("BotMoveTraceEligibilityTests OK");
	// Package adb16bb8 (Codex 15.11): the 200000-row cap ended MOVE TRACE at t3464.99 of a 3643.66 s Shore level.
	// Simulate the writer (BotMoveTrace::Visit): `actors` at 4 Hz for `seconds`; every actor must still have rows in
	// the last 10 s, and the row count must stay under the hard cap.
	struct Run { unsigned rows = 0; float lastRow[64] = {}; bool capped = false; };
	auto simulate = [](int actors, float seconds)
	{
		Run r; unsigned due[64] = {};
		for (int tick = 0; float(tick) * 0.25f <= seconds; ++tick)
			for (int a = 0; a < actors; ++a)
			{
				if (r.capped) continue;
				const int keep = MoveTraceKeep(r.rows, due[a]++);
				if (keep < 0) { r.capped = true; continue; }
				if (keep) { ++r.rows; r.lastRow[a] = float(tick) * 0.25f; }
			}
		return r;
	};
	{
		// Shore-15 as in the package (16 bots, ~58 rows/s observed, 3643.66 s): the whole level at full rate
		const Run r = simulate(16, 3643.66f);
		assert(!r.capped && r.rows > 200000 && r.rows < MoveTraceBudget().full);
		for (int a = 0; a < 16; ++a) assert(r.lastRow[a] >= 3643.66f - 0.5f);
	}
	{
		// a full 32-player level: past 400000 rows every actor goes on at 2 Hz to the end, no cap
		const Run r = simulate(32, 3650.f);
		assert(!r.capped && r.rows > MoveTraceBudget().full && r.rows < MoveTraceBudget().cap);
		for (int a = 0; a < 32; ++a) assert(r.lastRow[a] >= 3650.f - 1.f);
	}
	{
		// the hard cap still ends a runaway capture
		const Run r = simulate(32, 20000.f);
		assert(r.capped && r.rows == MoveTraceBudget().cap);
		// thinning keeps every 2nd due row per actor, the cap is exact
		assert(MoveTraceKeep(399999, 1) == 1 && MoveTraceKeep(400000, 1) == 0 && MoveTraceKeep(400000, 2) == 1);
		assert(MoveTraceKeep(999999, 0) == 1 && MoveTraceKeep(1000000, 0) == -1);
	}
	std::puts("BotMoveTraceEligibilityTests budget OK");
	return 0;
}
