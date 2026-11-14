// DS-079 regression: explicit immediate/deferred removal must never be treated as an external reset, while a true
// external deletion still restores, and level end / duplicate DONE / foreign entities stay ignored.
#include "CryGame/Bots/BotRemovalPolicy.h"
#include <cassert>
#include <cstdio>
#include <set>
#include <vector>

using namespace WarsBots;

// Codex 15.11 (#51): a manager-shaped caller driving the real RestoreDue/RunRestorePass/ResetRestoreSchedule the way
// BotManager::OnPostUpdate/RestoreRoster/OnActionEvent do (test_bot_frozen_shatter_wiring.py pins that wiring).
namespace
{
struct Entry { int number; float firstFail = -1.f; };
struct FakeManager
{
	std::vector<Entry> roster;
	RestoreSchedule schedule;
	bool pending = false;
	std::set<int> occupied;   // names still held by the deleted entity ("spawn refused: entity name is occupied")
	std::vector<int> live, gaveUp;
	int calls = 0;
	void Queue(int n) { roster.push_back({ n }); pending = true; }
	void ResetBegin() { ResetRestoreSchedule(schedule); pending = !roster.empty(); }   // eAE_resetBegin keeps the roster
	void Disconnect() { roster.clear(); pending = false; ResetRestoreSchedule(schedule); }
	void PostUpdate(float now)   // m_restorePending && level ready && RestoreDue(...) -> RestoreRoster()
	{
		if (!pending || !RestoreDue(schedule, now)) return;
		++calls;
		pending = false;
		RunRestorePass(roster, schedule, now,
			[this](const Entry& e) { if (occupied.count(e.number)) return false; live.push_back(e.number); return true; },
			[this](const Entry& e, float) { gaveUp.push_back(e.number); });
		pending = !roster.empty();
	}
};
}

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

	// Codex repro: attempt at t=100 fails, the clock restarts at 0 -> the next attempt must run at once (was gated to 100.5).
	{
		FakeManager m; m.occupied = { 3 }; m.Queue(3);
		m.PostUpdate(100.f); assert(m.calls == 1 && m.roster.size() == 1);
		m.occupied.clear();
		m.PostUpdate(0.f); assert(m.calls == 2 && m.live == std::vector<int>{ 3 } && !m.pending);
	}
	// Rollback while still occupied: the bot is kept (firstFail rebased), not dropped, and later restored.
	{
		FakeManager m; m.occupied = { 5 }; m.Queue(5);
		m.PostUpdate(100.f); m.PostUpdate(0.f);
		assert(m.calls == 2 && m.roster.size() == 1 && m.roster[0].firstFail == 0.f && m.gaveUp.empty());
		m.occupied.clear(); m.PostUpdate(0.5f); assert(m.live == std::vector<int>{ 5 } && !m.pending);
	}
	// Lifecycle reset (eAE_resetBegin) drops the old deadline but keeps the queued bots.
	{
		FakeManager m; m.occupied = { 7 }; m.Queue(7);
		m.PostUpdate(50.f); m.ResetBegin();
		m.occupied.clear(); m.PostUpdate(50.1f);
		assert(m.calls == 2 && m.live == std::vector<int>{ 7 });
	}
	// Disconnect clears both roster and schedule.
	{
		FakeManager m; m.occupied = { 1 }; m.Queue(1); m.PostUpdate(10.f); m.Disconnect();
		assert(m.roster.empty() && !m.pending && m.schedule.lastAt < 0.f && m.schedule.nextAt == 0.f);
	}
	// Occupied-name retry: paced at kRestoreRetryStep, succeeds once the name is free.
	{
		FakeManager m; m.occupied = { 3 }; m.Queue(3); m.Queue(9);
		m.PostUpdate(100.f); assert(m.live == std::vector<int>{ 9 } && m.roster.size() == 1);
		m.PostUpdate(100.2f); assert(m.calls == 1);                       // paced
		m.occupied.clear(); m.PostUpdate(100.5f);
		assert(m.calls == 2 && m.live == (std::vector<int>{ 9, 3 }) && !m.pending);
	}
	// Success first time: one call, nothing left.
	{
		FakeManager m; m.Queue(2); m.PostUpdate(1.f);
		assert(m.calls == 1 && m.live == std::vector<int>{ 2 } && m.roster.empty() && !m.pending);
	}
	// Timeout: name never frees -> given up (logged) after the window, restore no longer pending.
	{
		FakeManager m; m.occupied = { 4 }; m.Queue(4);
		for (float t = 100.f; t <= 111.f; t += kRestoreRetryStep) m.PostUpdate(t);
		assert(m.gaveUp == std::vector<int>{ 4 } && m.roster.empty() && !m.pending && m.live.empty());
	}

	std::puts("BotRemovalPolicyTests OK");
	return 0;
}
