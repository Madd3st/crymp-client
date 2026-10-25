#include "CryGame/Bots/BotTraits.h"
#include <cassert>
#include <cstdio>
using namespace WarsBots;
int main()
{
	const BotTraits eliteMarks = MakeTraits(3, 2, 1), begAssault = MakeTraits(0, 1, 2), eliteAssault = MakeTraits(3, 1, 3);
	assert(eliteMarks.camper > 0.7f && eliteMarks.prone > 0.5f && begAssault.prone == 0.f);
	assert(eliteAssault.jumper > begAssault.jumper && eliteAssault.jumper <= 0.9f);
	assert(MakeTraits(2, 4, 5).holder > MakeTraits(2, 1, 5).holder);
	// deterministic per seed, different between seeds
	assert(MakeTraits(1, 0, 7).aggression == MakeTraits(1, 0, 7).aggression);
	assert(MakeTraits(1, 0, 7).aggression != MakeTraits(1, 0, 8).aggression || MakeTraits(1, 0, 7).jumper != MakeTraits(1, 0, 8).jumper);
	for (int s = 0; s < 4; ++s) for (int st = 0; st < 6; ++st) for (unsigned sd = 0; sd < 20; ++sd)
	{ BotTraits t = MakeTraits(s, st, sd); assert(t.jumper >= 0 && t.jumper <= 0.9f && t.camper >= 0 && t.camper <= 1 && t.holder >= 0 && t.holder <= 1); }
	std::puts("BotTraitsTests OK");
	return 0;
}
