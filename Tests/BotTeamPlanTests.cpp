// PS-0 #2 tests: team-before-spawn policy (engine-free).
#include "CryGame/Bots/BotTeamPlan.h"
#include <cassert>
#include <cstdio>
using namespace WarsBots;

int main()
{
    assert(TeamForSlot(1) == 1 && TeamForSlot(2) == 2 && TeamForSlot(3) == 1);
    assert(TeamForCounts(1, 0, 0) == 1);
    assert(TeamForCounts(2, 3, 1) == 2);
    assert(TeamForCounts(3, 1, 3) == 1);
    assert(TeamForCounts(4, 2, 2) == 2);
    std::printf("team plan: all tests passed\n");
    return 0;
}
