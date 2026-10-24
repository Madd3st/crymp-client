#include "CryGame/Bots/BotPacePolicy.h"
#include <cassert>
using namespace WarsBots;
int main(){assert(!PaceSprintEnergy(false,39.9f));assert(PaceSprintEnergy(false,40));assert(PaceSprintEnergy(true,21));assert(!PaceSprintEnergy(true,20));assert(!PaceSpeedBurst(false,19.9f,200));assert(!PaceSpeedBurst(false,20,129.9f));assert(PaceSpeedBurst(false,20,130));assert(PaceSpeedBurst(true,10,61));assert(!PaceSpeedBurst(true,10,60));assert(!PaceSpeedBurst(true,7.9f,200));}
