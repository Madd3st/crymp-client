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
// Owner 06.11 (vanilla CryMP client over the home LAN): a dotted IPv4 literal in a private range only -
// 10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16. Not loopback, not 0.0.0.0, not a host name, no spaces. No leading zero in a
// multi-digit octet: WinSock inet_addr reads "010.0.0.1" as octal 8.0.0.1 and "172.016.0.1" as 172.14.0.1 (Codex 06.11).
inline bool IsPrivateIPv4(const char* s)
{
    if (!s) return false;
    int part[4] = {0, 0, 0, 0}, n = 0, digits = 0;
    for (const char* c = s; ; ++c)
    {
        if (*c >= '0' && *c <= '9')
        {
            if (digits > 0 && part[n] == 0) return false;   // "0" followed by a digit: ambiguous (octal) form
            if (++digits > 3) return false;
            part[n] = part[n] * 10 + (*c - '0');
            if (part[n] > 255) return false;
        }
        else if (*c == '.' || *c == 0)
        {
            if (!digits) return false;
            ++n; digits = 0;
            if (*c == 0) break;
            if (n > 3) return false;
        }
        else return false;
    }
    if (n != 4) return false;
    return part[0] == 10 || (part[0] == 172 && part[1] >= 16 && part[1] <= 31) || (part[0] == 192 && part[1] == 168);
}
// The bind the local gate accepts: loopback, or (wb_lan_test 1) a private LAN address; sv_lanonly=1 in both cases,
// so the server never registers with the public master list.
inline bool LocalBindOk(const char* bind, int lanOnly, int lanTest)
{
    if (lanOnly != 1 || !bind) return false;
    return !std::strcmp(bind, "127.0.0.1") || (lanTest == 1 && IsPrivateIPv4(bind));
}

} // namespace WarsBots
