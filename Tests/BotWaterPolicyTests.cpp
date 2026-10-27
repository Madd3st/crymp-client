// DS-070: one gated water policy for the owned PowerStruggle bot (engine-free).
// The same truth table gates canMove, the SafeStep ground ray, trap recovery and the native oxygen path, so an OFF
// build, Instant Action, a human or an unowned actor can never get any of the exceptions.
#include "CryGame/Bots/BotWaterPolicy.h"
#include <cassert>
#include <cstdio>

using namespace WarsBots;

static BotWaterFacts AllOn()
{
	BotWaterFacts f;
	f.experimentalBuild = true;
	f.multiplayer = true;
	f.powerStruggle = true;
	f.owned = true;
	f.botActor = true;
	f.captureEnabled = true;
	f.navMeshEnabled = true;
	f.navMeshReady = true;
	f.hasNanoSuit = true;
	return f;
}

int main()
{
	// Positive: the owned PowerStruggle bot on a multiplayer server with both existing switches on.
	assert(BotWaterAllowed(AllOn()));

	// Every field is required: flipping any one off disables all water exceptions together.
	{
		BotWaterFacts f = AllOn(); f.experimentalBuild = false; assert(!BotWaterAllowed(f)); // OFF build
		f = AllOn(); f.multiplayer = false;      assert(!BotWaterAllowed(f));              // singleplayer
		f = AllOn(); f.powerStruggle = false;    assert(!BotWaterAllowed(f));              // non-PS mode
		f = AllOn(); f.owned = false;            assert(!BotWaterAllowed(f));              // replica/unowned
		f = AllOn(); f.botActor = false;         assert(!BotWaterAllowed(f));              // human actor
		f = AllOn(); f.captureEnabled = false;   assert(!BotWaterAllowed(f));              // wb_ps_capture 0
		f = AllOn(); f.navMeshEnabled = false;   assert(!BotWaterAllowed(f));              // wb_bot_navmesh 0
		f = AllOn(); f.navMeshReady = false; assert(!BotWaterAllowed(f)); // flag enabled but mesh missing
		f = AllOn(); f.hasNanoSuit = false;      assert(!BotWaterAllowed(f));              // oxygen path needs a suit
	}

	// The default struct (all false) can never enable water.
	{ BotWaterFacts f; assert(!BotWaterAllowed(f)); }

	std::puts("BotWaterPolicyTests OK");
	return 0;
}
