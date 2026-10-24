#pragma once
#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "BotPacePolicy.h"
namespace WarsBots {
inline bool BotPaceEnabled(){const ICVar* flag=gEnv && gEnv->pConsole?gEnv->pConsole->GetCVar("wb_bot_pace"):nullptr;return flag && flag->GetIVal()==1;}
}
