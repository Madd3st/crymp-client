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

	// DS-080 reset roster: only a live bot may be queued; an explicit pending removal, an already-DONE slot and a
	// number already queued by a true external DONE are skipped.
	assert(ShouldRosterForReset(false, false, false));
	assert(!ShouldRosterForReset(true, false, false));
	assert(!ShouldRosterForReset(false, true, false));
	assert(!ShouldRosterForReset(false, false, true));

	// Playtest 11.11: a failed re-creation (name still held by the deleted entity) is retried inside the window, not lost.
	assert(!KeepRosterForRetry(-1.f, 5.f, kRestoreRetryWindow));          // never failed: nothing to keep
	assert(KeepRosterForRetry(100.f, 100.f, kRestoreRetryWindow));         // same frame as the DONE (observed case)
	assert(KeepRosterForRetry(100.f, 109.9f, kRestoreRetryWindow));
	assert(!KeepRosterForRetry(100.f, 110.f, kRestoreRetryWindow));        // window over: give up, log it
	assert(!KeepRosterForRetry(100.f, 99.f, kRestoreRetryWindow));         // clock went back (new level): drop
	assert(kRestoreRetryStep > 0.f && kRestoreRetryStep < kRestoreRetryWindow);

	std::puts("BotRemovalPolicyTests OK");
	return 0;
}
