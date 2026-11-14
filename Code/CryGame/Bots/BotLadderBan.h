// SPEC-009 (owner 10.11/11.11, wb_bot_ladder_ban 1): a ladder failure no longer bans the link for the whole level at
// once. Window 38e63072: the first `LADDER failed no_grab` (16.05 s) banned the only Shore submarine ladder for every
// bot; the mid deck became a separate component and 251 of 282 NAV fallbacks followed.
//
// Rule (engine-free, testable):
// - a failure is recorded per link with the bot and the world time;
// - within kLadderBanWindow s: failures by >= 2 different bots ban the link for everyone (scope Level) for
//   kLadderLevelBanFor s; the caller bans the poly and lifts it at expiry. Repeats by ONE bot never ban it for the
//   team (owner rule, Codex 15.11 #52: one bot must not close the only exit for everyone);
// - otherwise only the failing bot avoids the link for kLadderBotAvoidFor s (scope Bot): the caller raises that link's
//   path cost for this bot only (another route if one exists; the only exit is retried, never lost for the team);
// - the same book and timings serve ladder AND jump/strength/drop links (BotBrain::CancelLadder, BotBrain::ApplyLinkExecutor):
//   no link ban is permanent with wb_bot_ladder_ban 1.
#pragma once
#include <algorithm>
#include <vector>

namespace WarsBots
{
constexpr float kLadderBanWindow = 120.f;     // s: failures counted together
constexpr float kLadderBotAvoidFor = 30.f;    // s: personal avoidance after a failure
constexpr float kLadderLevelBanFor = 120.f;   // s: level-wide ban, then lifted
constexpr float kLadderAvoidCost = 50.f;      // path cost factor of the avoided link for that bot

enum class LadderBanScope { Bot = 0, Level };

struct LadderBanDecision
{
	LadderBanScope scope = LadderBanScope::Bot;
	float until = 0.f;   // world time the avoidance / ban ends
	int fails = 0;       // failures of this link inside the window, including this one
	int bots = 0;        // different bots among them
};

class LadderBanBook
{
public:
	LadderBanDecision Fail(unsigned long long link, int bot, float now)
	{
		m_fails.erase(std::remove_if(m_fails.begin(), m_fails.end(),
			[now](const Failure& f) { return now - f.at >= kLadderBanWindow || now < f.at; }), m_fails.end());
		m_fails.push_back({link, bot, now});
		LadderBanDecision d;
		std::vector<int> seen;
		for (const Failure& f : m_fails)
			if (f.link == link)
			{
				++d.fails;
				if (std::find(seen.begin(), seen.end(), f.bot) == seen.end()) seen.push_back(f.bot);
			}
		d.bots = int(seen.size());
		const bool level = d.bots >= 2;   // distinct bots only
		d.scope = level ? LadderBanScope::Level : LadderBanScope::Bot;
		d.until = now + (level ? kLadderLevelBanFor : kLadderBotAvoidFor);
		if (level)
		{
			bool known = false;
			for (Ban& b : m_bans) if (b.link == link) { b.until = d.until; known = true; }
			if (!known) m_bans.push_back({link, d.until});
		}
		return d;
	}
	// Level bans whose time is over; removed from the book. The caller lifts them on the mesh.
	std::vector<unsigned long long> TakeExpired(float now)
	{
		std::vector<unsigned long long> out;
		for (size_t i = 0; i < m_bans.size();)
			if (now >= m_bans[i].until) { out.push_back(m_bans[i].link); m_bans.erase(m_bans.begin() + long(i)); }
			else ++i;
		return out;
	}
	bool LevelBanned(unsigned long long link) const
	{
		for (const Ban& b : m_bans) if (b.link == link) return true;
		return false;
	}
	void Clear() { m_fails.clear(); m_bans.clear(); }

private:
	struct Failure { unsigned long long link; int bot; float at; };
	struct Ban { unsigned long long link; float until; };
	std::vector<Failure> m_fails;
	std::vector<Ban> m_bans;
};
}
