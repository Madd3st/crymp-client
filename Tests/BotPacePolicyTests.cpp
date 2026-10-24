#include "CryGame/Bots/BotPacePolicy.h"
#include <cassert>
using namespace WarsBots;
int main(){assert(!PaceSprintEnergy(false,39.9f));assert(PaceSprintEnergy(false,40));assert(PaceSprintEnergy(true,21));assert(!PaceSprintEnergy(true,20));assert(!PaceSpeedBurst(false,29.9f,200));assert(!PaceSpeedBurst(false,30,149));assert(PaceSpeedBurst(false,30,150));assert(PaceSpeedBurst(true,10,61));assert(!PaceSpeedBurst(true,10,60));assert(!PaceSpeedBurst(true,7.9f,200));}
