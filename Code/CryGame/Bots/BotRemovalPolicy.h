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

// Playtest 11.11: re-creation right after an external DONE failed because the deleted entity still held the bot's
// name ("spawn refused: entity name ... is occupied"), the roster was cleared and the bot was gone for the match.
// A failed entry is kept and retried until `window` seconds after its first failure.
constexpr float kRestoreRetryWindow = 10.f;  // s after the first failure
constexpr float kRestoreRetryStep = 0.5f;    // s between retries (bounds the "spawn refused" log)
inline bool KeepRosterForRetry(float firstFailAt, float now, float window)
{
	return firstFailAt >= 0.f && now >= firstFailAt && now - firstFailAt < window;
}
}
