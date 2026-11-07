// PS-0 #1 tests: mode/bind policy (engine-free).
#include "CryGame/Bots/BotModePlan.h"
#include <cassert>
#include <cstdio>
using namespace WarsBots;

int main()
{
    assert(IsBotRulesClass("InstantAction"));
    assert(IsBotRulesClass("PowerStruggle"));
    assert(!IsBotRulesClass("SinglePlayer"));
    assert(!IsBotRulesClass(""));
    assert(IsPowerStruggleClass("PowerStruggle"));
    assert(!IsPowerStruggleClass("InstantAction"));
    assert(GateBindOk(false, true));    // local loopback
    assert(!GateBindOk(false, false));  // public without opt-in
    assert(GateBindOk(true, false));    // explicit public opt-in
    assert(GateBindOk(true, true));
    // owner 06.11: LAN test bind - private IPv4 literals only, always with sv_lanonly=1
    assert(IsPrivateIPv4("192.168.77.114") && IsPrivateIPv4("10.0.0.1") && IsPrivateIPv4("172.16.0.1") && IsPrivateIPv4("172.31.255.254"));
    assert(!IsPrivateIPv4("172.15.0.1") && !IsPrivateIPv4("172.32.0.1") && !IsPrivateIPv4("8.8.8.8") && !IsPrivateIPv4("127.0.0.1"));
    assert(!IsPrivateIPv4("0.0.0.0") && !IsPrivateIPv4("") && !IsPrivateIPv4(nullptr) && !IsPrivateIPv4("192.168.1"));
    assert(!IsPrivateIPv4("192.168.1.300") && !IsPrivateIPv4(" 192.168.1.1") && !IsPrivateIPv4("192.168.1.1 ") && !IsPrivateIPv4("192.168..1"));
    assert(!IsPrivateIPv4("192.168.1.1.5") && !IsPrivateIPv4("192.168.0001.1") && !IsPrivateIPv4("lan.local") && !IsPrivateIPv4("192.168.1.1:64087"));
    // leading zeros (Codex 06.11: inet_addr parses them as octal -> 8.0.0.1, 172.14.0.1); a single 0 octet stays valid
    assert(!IsPrivateIPv4("010.0.0.1") && !IsPrivateIPv4("172.016.0.1") && !IsPrivateIPv4("192.168.01.1") && !IsPrivateIPv4("192.168.1.00"));
    assert(!IsPrivateIPv4("10.0.0.09") && !IsPrivateIPv4("00.0.0.1"));
    assert(IsPrivateIPv4("10.0.0.0") && IsPrivateIPv4("192.168.0.1") && IsPrivateIPv4("172.20.100.200") && IsPrivateIPv4("10.250.0.10"));
    assert(!LocalBindOk("010.0.0.1", 1, 1) && !LocalBindOk("172.016.0.1", 1, 1));
    assert(LocalBindOk("127.0.0.1", 1, 0) && LocalBindOk("127.0.0.1", 1, 1));          // loopback as before
    assert(!LocalBindOk("192.168.77.114", 1, 0));                                       // LAN needs the opt-in
    assert(LocalBindOk("192.168.77.114", 1, 1));
    assert(!LocalBindOk("192.168.77.114", 0, 1) && !LocalBindOk("127.0.0.1", 0, 0));   // never without sv_lanonly=1
    assert(!LocalBindOk("0.0.0.0", 1, 1) && !LocalBindOk("", 1, 1) && !LocalBindOk("93.184.216.34", 1, 1) && !LocalBindOk(nullptr, 1, 1));
    std::printf("mode plan: all tests passed\n");
    return 0;
}
