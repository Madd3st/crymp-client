// DS-079: classify an ENTITY_EVENT_DONE for a registered bot. Engine-free so explicit immediate/deferred removal,
// a true external reset, level end and duplicate DONE are all testable without the engine.
//
// The Manager used to check only `Removing()` (true only while its own RemoveEntity call is on the stack). A deferred
// explicit removal therefore looked external when DONE arrived later: it queued a roster restore and logged
// "removed externally", later re-creating a bot the operator had removed. RemovalPending() carries the durable
// intent, Expired() covers a duplicate DONE after OnEntityDone, and levelReady=false covers level teardown.
#pragma once

#include <vector>

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

// Codex 15.11 (#51): retry pacing must follow the game clock and the level lifecycle. A deadline left from an earlier
// attempt (t=100) gated restoration after a clock reset to 0 until t=100.5. Policy:
//  - lifecycle reset (resetBegin, loading start, level not found, disconnect): ResetRestoreSchedule, queued bots kept
//    where the lifecycle keeps them (resetBegin keeps the roster);
//  - clock went back (now < last attempt): the schedule is rebased to now, so the next attempt runs immediately;
//  - an entry whose firstFail lies in the future (clock went back) is rebased to now: its retry window restarts
//    instead of the bot being dropped.
struct RestoreSchedule
{
	float nextAt = 0.f;   // earliest next attempt
	float lastAt = -1.f;  // last attempt (-1: none since the last lifecycle reset)
};

inline void ResetRestoreSchedule(RestoreSchedule& s) { s = RestoreSchedule{}; }

// The OnPostUpdate gate. Rebases on a clock rollback.
inline bool RestoreDue(RestoreSchedule& s, float now)
{
	if (s.lastAt >= 0.f && now < s.lastAt) ResetRestoreSchedule(s);
	return now >= s.nextAt;
}

// The RestoreRoster body. tryAdd(entry) -> bool (re-created); onGiveUp(entry, waitedSeconds).
// Keeps only entries to retry in `roster`; returns the number restored in this pass.
template <class Entry, class TryAdd, class OnGiveUp>
int RunRestorePass(std::vector<Entry>& roster, RestoreSchedule& s, float now, TryAdd&& tryAdd, OnGiveUp&& onGiveUp)
{
	int restored = 0;
	std::vector<Entry> retry;
	for (Entry entry : roster)
	{
		if (entry.firstFail > now) entry.firstFail = now; // clock went back: restart the window, keep the bot
		if (tryAdd(entry)) { ++restored; continue; }
		if (entry.firstFail < 0.f) entry.firstFail = now;
		if (KeepRosterForRetry(entry.firstFail, now, kRestoreRetryWindow)) retry.push_back(entry);
		else onGiveUp(entry, now - entry.firstFail);
	}
	roster.swap(retry);
	s.lastAt = now;
	s.nextAt = now + kRestoreRetryStep;
	return restored;
}
}
