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
		// Codex 15.11 #52 repro (link=1, bot=4, t=10/20/30): one bot can never close the link for everyone,
		// however often it fails; its personal avoidance just moves on.
		for (float t : {10.f, 20.f, 30.f, 40.f, 50.f})
		{
			const LadderBanDecision d = book.Fail(1, 4, t);
			assert(d.scope == LadderBanScope::Bot && d.bots == 1 && d.until == t + kLadderBotAvoidFor);
		}
		assert(!book.LevelBanned(1) && book.TakeExpired(1e6f).empty());
		// the second DISTINCT bot inside the window -> shared ban for 120 s
		const LadderBanDecision d = book.Fail(1, 7, 60.f);
		assert(d.scope == LadderBanScope::Level && d.bots == 2 && d.until == 60.f + kLadderLevelBanFor && book.LevelBanned(1));
	}
	{
		// Codex 15.11 #52: jump/strength/drop links use the same book (BotBrain::ApplyLinkExecutor) - no permanent jump ban.
		LadderBanBook book;
		const unsigned long long jump = 0x3000;
		assert(book.Fail(jump, 2, 5.f).scope == LadderBanScope::Bot);     // first bot: personal avoidance only
		assert(book.Fail(jump, 2, 6.f).scope == LadderBanScope::Bot);     // the same bot again: still personal
		const LadderBanDecision d = book.Fail(jump, 9, 7.f);              // a second bot: everyone, 120 s
		assert(d.scope == LadderBanScope::Level && d.until == 7.f + 120.f);
		assert(book.TakeExpired(126.9f).empty() && book.LevelBanned(jump));
		const auto lifted = book.TakeExpired(127.f);
		assert(lifted.size() == 1 && lifted[0] == jump && !book.LevelBanned(jump));
		// after the expiry the old failures are out of the window: a retry failure is personal again
		assert(book.Fail(jump, 2, 128.f).scope == LadderBanScope::Bot);
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
	static_assert(kLadderLevelBanFor == 120.f, "owner rule: shared ban lasts 120 s (ladder and jump)");
	std::puts("BotLadderBanTests OK");
	return 0;
}
