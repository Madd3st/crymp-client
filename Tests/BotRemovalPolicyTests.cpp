// DS-079 regression: explicit immediate/deferred removal must never be treated as an external reset, while a true
// external deletion still restores, and level end / duplicate DONE / foreign entities stay ignored.
#include "CryGame/Bots/BotRemovalPolicy.h"
#include <cassert>
#include <cstdio>

using namespace WarsBots;

int main()
{
	// Explicit immediate removal: DONE arrives inside our RemoveEntity call.
	assert(BotDoneDecision(true, true, true, true, false) == BotDoneAction::Ignore);
	// Explicit deferred removal (observed C145 bug): Removing() is already false, RemovalPending() is the durable truth.
	assert(BotDoneDecision(true, true, false, true, false) == BotDoneAction::Ignore);
	// True external deletion (native RestartGame/Reset): no explicit intent, level live -> restore the same bot.
	assert(BotDoneDecision(true, true, false, false, false) == BotDoneAction::RestoreExternal);
	// Level teardown: never queue a restore.
	assert(BotDoneDecision(true, false, false, false, false) == BotDoneAction::Ignore);
	// Duplicate DONE after OnEntityDone cleared the identity.
	assert(BotDoneDecision(true, true, false, false, true) == BotDoneAction::Ignore);
	// Deferred removal followed by a duplicate DONE stays ignored.
	assert(BotDoneDecision(true, true, false, true, true) == BotDoneAction::Ignore);
	// Foreign/unknown entity id never resolves to a registered bot.
	assert(BotDoneDecision(false, true, false, false, false) == BotDoneAction::Ignore);

	std::puts("BotRemovalPolicyTests OK");
	return 0;
}
