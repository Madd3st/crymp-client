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
	return 0;
}
