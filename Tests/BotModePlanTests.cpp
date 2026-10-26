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
    std::printf("mode plan: all tests passed\n");
    return 0;
}
