#include "PortIncludes.h"
#include "CryGame/Bots/BotLog.h"
#include "CryCommon/CrySystem/ILog.h"
#include <cstdarg>
#include <cstdio>

namespace WarsBots
{
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
		CryLogAlways("%s", buffer);      // console + file
	else
		gEnv->pLog->LogToFile("%s", buffer); // file only: analysable, no console spam
}
}
