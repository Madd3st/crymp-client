// WarsBots logging levels. Console gets only what a server operator needs (wb_log, default 1);
// detailed telemetry always goes to the log FILE only (ILog::LogToFile), never the console,
// unless wb_log 2. Works on server and client (client replica traces are level 2).
#pragma once

namespace WarsBots
{
enum { kLogEssential = 0, kLogEvent = 1, kLogTrace = 2 };
void Log(int level, const char* format, ...);
// Write the buffered .bots.log lines to the OS now (level end): the timer flush inside Log runs only on the next line, so
// the last lines of a level were lost when no bot logged again (server run f9d33a47: OFF "PATHTIME level-end").
void FlushTrace();
// SPEC-007 log cost: time spent inside Log() (format, main-log copy, trace write, flush) since the last call, and the
// line counts. BotManager logs it as LOGCOST next to FRAME every 10 s, for the A/B of the frame with and without logs.
struct LogCost { unsigned lines = 0, main = 0, flushes = 0; float ms = 0.f; };
LogCost TakeLogCost();
// Trace flush policy: essential lines at once, everything else every 256 lines or 1 s (and at level end).
inline bool TraceFlushDue(int level, unsigned unflushed, float sinceFlush) { return level <= 0 || unflushed >= 256 || sinceFlush >= 1.f; }
// Server MOVE TRACE rate, clamped to 1..4 rows/s.
inline int MoveTraceServerHz(int v) { return v < 1 ? 1 : v > 4 ? 4 : v; }
// DS-047: probe runner metric hook. BotLog calls this with the timestamped line; it is a no-op unless a
// probe scenario is running. Never log from inside it (it runs under Log).
void ProbeObserveLine(const char* line);
}
#define WB_LOG(level, ...) ::WarsBots::Log(level, __VA_ARGS__)
