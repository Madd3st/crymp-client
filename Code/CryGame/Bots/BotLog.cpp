#include "PortIncludes.h"
#include "CryGame/Bots/BotLog.h"
#include "CryGame/Bots/BotDataPath.h"
#include "CryCommon/CrySystem/ILog.h"
#include "CryCommon/CrySystem/ICryPak.h"
#include "CryCommon/CrySystem/ITimer.h"
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

void OpenTrace()
{
	g_traceTried = true;
	const char* logName = gEnv->pLog->GetFileName();
	std::filesystem::path base = logName && *logName ? std::filesystem::path(logName).stem() : std::filesystem::path("WarsBots");
	std::filesystem::path dir = DataDirectory(L"Logs");
	const std::filesystem::path path = dir / (base.wstring() + L".bots.log");
	g_trace = _wfopen(path.c_str(), L"w");
}
}

void Log(int level, const char* format, ...)
{
	if (!gEnv || !gEnv->pLog) return;
	char buffer[1024];
	va_list args;
	va_start(args, format);
	_vsnprintf(buffer, sizeof(buffer) - 1, format, args);
	va_end(args);
	buffer[sizeof(buffer) - 1] = '\0';
	ICVar* cvar = gEnv->pConsole ? gEnv->pConsole->GetCVar("wb_log") : nullptr;
	const int console = cvar ? cvar->GetIVal() : 1;
	if (level <= console)
		CryLogAlways("%s", buffer); // console + main log
	if (!g_traceTried) OpenTrace();
	if (g_trace)
	{
		// Full timeline (every level), timestamped, for Tools/wb_log_report.py.
		const float now=gEnv->pTimer ? gEnv->pTimer->GetCurrTime() : 0.f;
        fprintf(g_trace, "%.2f %s\n", now, buffer);
        if(level<=kLogEvent || ++g_unflushed>=64 || now-g_lastFlush>=1.f){fflush(g_trace);g_unflushed=0;g_lastFlush=now;}
	}
}
}
