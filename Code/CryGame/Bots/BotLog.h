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
// DS-047: probe runner metric hook. BotLog calls this with the timestamped line; it is a no-op unless a
// probe scenario is running. Never log from inside it (it runs under Log).
void ProbeObserveLine(const char* line);
}
#define WB_LOG(level, ...) ::WarsBots::Log(level, __VA_ARGS__)
