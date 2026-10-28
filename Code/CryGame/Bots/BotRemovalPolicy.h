// DS-079: classify an ENTITY_EVENT_DONE for a registered bot. Engine-free so explicit immediate/deferred removal,
// a true external reset, level end and duplicate DONE are all testable without the engine.
//
// The Manager used to check only `Removing()` (true only while its own RemoveEntity call is on the stack). A deferred
// explicit removal therefore looked external when DONE arrived later: it queued a roster restore and logged
// "removed externally", later re-creating a bot the operator had removed. RemovalPending() carries the durable
// intent, Expired() covers a duplicate DONE after OnEntityDone, and levelReady=false covers level teardown.
#pragma once

namespace WarsBots
{
enum class BotDoneAction
{
	Ignore = 0,
	RestoreExternal,
};

// known: the entity id resolved to one of our registered bots; levelReady: the level is live;
// removing: our RemoveEntity call is on the stack; removalPending: durable explicit-removal intent;
// expired: OnEntityDone already processed this instance (duplicate DONE).
inline BotDoneAction BotDoneDecision(bool known, bool levelReady, bool removing, bool removalPending, bool expired)
{
	if (!known || !levelReady) return BotDoneAction::Ignore;
	if (removing || removalPending || expired) return BotDoneAction::Ignore;
	return BotDoneAction::RestoreExternal;
}

// DS-080: the reset roster (eAE_resetBegin -> RestoreRoster) must not resurrect a bot whose explicit removal is still
// pending, an already-DONE slot, or a number already queued by a true external DONE.
inline bool ShouldRosterForReset(bool removalPending, bool expired, bool alreadyQueued)
{
	return !removalPending && !expired && !alreadyQueued;
}
}
