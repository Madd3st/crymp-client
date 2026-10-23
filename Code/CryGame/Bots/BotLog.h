// WarsBots logging levels. Console gets only what a server operator needs (wb_log, default 1);
// detailed telemetry always goes to the log FILE only (ILog::LogToFile), never the console,
// unless wb_log 2. Works on server and client (client replica traces are level 2).
#pragma once

namespace WarsBots
{
enum { kLogEssential = 0, kLogEvent = 1, kLogTrace = 2 };
void Log(int level, const char* format, ...);
}
#define WB_LOG(level, ...) ::WarsBots::Log(level, __VA_ARGS__)
