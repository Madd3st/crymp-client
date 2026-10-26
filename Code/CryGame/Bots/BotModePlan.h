// PS-0 #1: mode and bind policy for bots on a public/PS server. Pure logic, no engine types.
// Tests/BotModePlanTests.cpp.
#pragma once
#include <cstring>

namespace WarsBots {

inline bool IsBotRulesClass(const char* className)
{
    return className && (!std::strcmp(className, "InstantAction") || !std::strcmp(className, "PowerStruggle"));
}
inline bool IsPowerStruggleClass(const char* className)
{
    return className && !std::strcmp(className, "PowerStruggle");
}
// The local gate needs loopback+LAN unless the owner explicitly allowed a public server.
inline bool GateBindOk(bool allowPublic, bool localBind)
{
    return allowPublic || localBind;
}

} // namespace WarsBots
