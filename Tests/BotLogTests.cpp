// SPEC-007 log cost: engine-free policy of BotLog (trace flush) and the server MOVE TRACE rate.
#include "CryGame/Bots/BotLog.h"
#include <cassert>
#include <cstdio>
using namespace WarsBots;
int main()
{
	// essential lines are flushed at once; events and telemetry wait for 256 lines or 1 s (was: every event at once)
	assert(TraceFlushDue(kLogEssential, 1, 0.f));
	assert(!TraceFlushDue(kLogEvent, 1, 0.f) && !TraceFlushDue(kLogTrace, 255, 0.99f));
	assert(TraceFlushDue(kLogTrace, 256, 0.f) && TraceFlushDue(kLogEvent, 3, 1.f));
	// server MOVE TRACE rate: 1..4 rows/s, default 4 unchanged
	assert(MoveTraceServerHz(4) == 4 && MoveTraceServerHz(1) == 1 && MoveTraceServerHz(0) == 1 && MoveTraceServerHz(-3) == 1 && MoveTraceServerHz(30) == 4);
	LogCost c; assert(c.lines == 0 && c.main == 0 && c.flushes == 0 && c.ms == 0.f);
	std::printf("BotLog OK\n");
	return 0;
}
