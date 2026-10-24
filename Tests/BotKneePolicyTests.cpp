#include "CryGame/Bots/BotKneePolicy.h"
#include <cassert>
using namespace WarsBots;
int main(){assert(KneeRayHeight(0)==.5f && KneeRayHeight(1)==.8f && KneeRayHeight(2)==.5f);assert(!KneeHitIsStep(0,.6f,.1f,1));assert(KneeHitIsStep(2,.6f,.1f,1));assert(!KneeHitIsStep(2,.8f,.3f,1));assert(!KneeHitIsStep(2,.1f,-.4f,1));assert(!KneeHitIsStep(2,.6f,.1f,.5f));}
