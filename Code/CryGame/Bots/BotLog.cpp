#include "PortIncludes.h"
#include "BotConfig.h"
#include "CryGame/Bots/BotLog.h"
#include "CryGame/Bots/BotDataPath.h"
#include "CryCommon/CrySystem/ILog.h"
#include "CryCommon/CrySystem/ICryPak.h"
#include "CryCommon/CrySystem/ITimer.h"
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <string>

namespace WarsBots
{
namespace
{
// Our own trace file next to the engine log: "<log name>.bots.log". The engine's file-only
// channel drops messages below -verbosity, so telemetry would otherwise be lost.
FILE* g_trace = nullptr;
bool g_traceTried = false;
unsigned g_unflushed = 0;
float g_lastFlush = 0;
LogCost g_cost;

void OpenTrace()
{
	g_traceTried = true;
	const char* logName = gEnv->pLog->GetFileName();
	std::filesystem::path base = logName && *logName ? std::filesystem::path(logName).stem() : std::filesystem::path("WarsBots");
	std::filesystem::path dir = DataDirectory(L"Logs");
	const std::filesystem::path path = dir / (base.wstring() + L".bots.log");
	g_trace = _wfopen(path.c_str(), L"w");
	if (g_trace) setvbuf(g_trace, nullptr, _IOFBF, 1 << 16);   // whole lines leave in 64 KB blocks, not per fprintf
}
}

void FlushTrace()
{
	if (g_trace) { fflush(g_trace); g_unflushed = 0; ++g_cost.flushes; }
}

LogCost TakeLogCost() { const LogCost c = g_cost; g_cost = LogCost(); return c; }

void Log(int level, const char* format, ...)
{
	if (!gEnv || !gEnv->pLog) return;
	const auto costStart = std::chrono::steady_clock::now();
	char buffer[1024];
	va_list args;
	va_start(args, format);
	_vsnprintf(buffer, sizeof(buffer) - 1, format, args);
	va_end(args);
	buffer[sizeof(buffer) - 1] = '\0';
	const BotConfig& config=BotConfig::Get();
	const int console = config.registered ? config.logLevel : 1;
	if (level <= console)
	{ CryLogAlways("%s", buffer); ++g_cost.main; } // console + main log (the engine flushes the main log per line)
	if (!g_traceTried) OpenTrace();
	if (g_trace)
	{
		// Full timeline (every level), timestamped, for Tools/wb_log_report.py.
		const float now=gEnv->pTimer ? gEnv->pTimer->GetCurrTime() : 0.f;
        fprintf(g_trace, "%.2f %s\n", now, buffer);
        if(TraceFlushDue(level,++g_unflushed,now-g_lastFlush)){fflush(g_trace);g_unflushed=0;g_lastFlush=now;++g_cost.flushes;}
	}
	++g_cost.lines;
	g_cost.ms += std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - costStart).count();
	// DS-047: feed the probe runner with the same timestamped line the report tools parse. It only parses text
	// and never calls WB_LOG, so there is no recursion.
	if (BotConfig::Get().probeRunner == 1)
	{
		const float now=gEnv->pTimer ? gEnv->pTimer->GetCurrTime() : 0.f;
		char line[1100];
		_snprintf(line, sizeof(line) - 1, "%.2f %s", now, buffer);
		line[sizeof(line) - 1] = '\0';
		ProbeObserveLine(line);
	}
}
}
