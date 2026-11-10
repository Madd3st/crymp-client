// SPEC-009 (wb_bot_ladder_ban): one bot's ladder failure no longer bans the link for every bot of the level.
// Window 38e63072: the first no_grab (16.05 s) banned the only Shore submarine ladder; 251/282 NAV fallbacks followed.
#include "CryGame/Bots/BotLadderBan.h"
#include <cassert>
#include <cstdio>

using namespace WarsBots;

int main()
{
	const unsigned long long sub = 0x1000, other = 0x2000;
	{
		LadderBanBook book;
		// 38e63072 case: the first failure of bot 4 -> only that bot avoids the ladder, for 30 s
		LadderBanDecision d = book.Fail(sub, 4, 16.05f);
		assert(d.scope == LadderBanScope::Bot && d.fails == 1 && d.bots == 1);
		assert(d.until == 16.05f + kLadderBotAvoidFor);
		assert(!book.LevelBanned(sub) && book.TakeExpired(1000.f).empty());
		// a failure on another ladder is counted apart
		d = book.Fail(other, 5, 20.f);
		assert(d.scope == LadderBanScope::Bot && d.fails == 1);
		// a second bot fails on the same ladder inside 120 s -> everyone, for 120 s
		d = book.Fail(sub, 7, 60.f);
		assert(d.scope == LadderBanScope::Level && d.fails == 2 && d.bots == 2 && d.until == 60.f + kLadderLevelBanFor);
		assert(book.LevelBanned(sub) && !book.LevelBanned(other));
		// the level ban is lifted when its time is over, once
		assert(book.TakeExpired(179.9f).empty());
		const auto lifted = book.TakeExpired(180.f);
		assert(lifted.size() == 1 && lifted[0] == sub && !book.LevelBanned(sub));
		assert(book.TakeExpired(500.f).empty());
	}
	{
		LadderBanBook book;
		// the same bot failing again: personal avoidance, until the third failure in the window
		assert(book.Fail(sub, 4, 10.f).scope == LadderBanScope::Bot);
		assert(book.Fail(sub, 4, 50.f).scope == LadderBanScope::Bot);
		const LadderBanDecision d = book.Fail(sub, 4, 90.f);
		assert(d.scope == LadderBanScope::Level && d.fails == kLadderBanLevelFails && d.bots == 1);
	}
	{
		LadderBanBook book;
		// failures further apart than the window never add up
		assert(book.Fail(sub, 4, 10.f).scope == LadderBanScope::Bot);
		const LadderBanDecision d = book.Fail(sub, 7, 10.f + kLadderBanWindow);
		assert(d.scope == LadderBanScope::Bot && d.fails == 1);
		// a clock that went back (new level without Clear) drops the old failures
		assert(book.Fail(sub, 8, 5.f).fails == 1);
	}
	{
		LadderBanBook book;
		book.Fail(sub, 4, 10.f); book.Fail(sub, 7, 11.f);
		assert(book.LevelBanned(sub));
		// a new level ban on a banned link extends it instead of adding a second entry
		book.Fail(sub, 9, 100.f);
		assert(book.TakeExpired(131.f).empty());
		assert(book.TakeExpired(220.f).size() == 1);
		book.Fail(sub, 4, 300.f); book.Fail(sub, 7, 301.f);
		book.Clear();
		assert(!book.LevelBanned(sub) && book.TakeExpired(1e6f).empty() && book.Fail(sub, 4, 302.f).fails == 1);
	}
	static_assert(kLadderBotAvoidFor < kLadderBanWindow, "personal avoidance ends inside the counting window");
	static_assert(kLadderAvoidCost > 1.f, "avoidance raises the cost");
	std::puts("BotLadderBanTests OK");
	return 0;
}
