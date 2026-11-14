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
//   no link ban is permanent with wb_bot_ladder_ban 1;
// - Codex 15.11 #52 (re-review): a shared ban never outlives its expiry record. A game clock that goes back (same-map
//   reset, timer reset) makes every shared ban due at once; the level-end cleanup lifts every shared ban on the mesh
//   before the book is emptied (LiftAllBans), because the mesh survives a reset of the same map;
// - owner 15.11: a shared ban is applied only if no live bot that reaches a capture point loses all of them without
//   the link (the only exit, e.g. the Shore submarine ladder 2116.1/1880.6/90.25). Then it stays a cost for the
//   failing bot only (ApplyLinkFailure, NavMeshCore::BanLinkKeepingExits).
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
	bool keptExit = false;   // owner 15.11: a shared ban was due but would strand a live bot -> cost only
};

class LadderBanBook
{
public:
	LadderBanDecision Fail(unsigned long long link, int bot, float now)
	{
		Rebase(now);
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
	// A clock rollback (now earlier than the last call) makes every ban due now.
	std::vector<unsigned long long> TakeExpired(float now)
	{
		Rebase(now);
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
	// The shared ban was not applied on the mesh (it would strand a live bot): forget it, the failures stay counted.
	void Revoke(unsigned long long link)
	{
		m_bans.erase(std::remove_if(m_bans.begin(), m_bans.end(), [link](const Ban& b) { return b.link == link; }), m_bans.end());
	}
	// Every shared ban, removed from the book (level end / reset): the caller lifts them on the mesh.
	std::vector<unsigned long long> TakeAll()
	{
		std::vector<unsigned long long> out;
		for (const Ban& b : m_bans) out.push_back(b.link);
		Clear();
		return out;
	}
	void Clear() { m_fails.clear(); m_bans.clear(); m_seen = false; m_last = 0.f; }

private:
	void Rebase(float now)
	{
		if (m_seen && now < m_last)
		{   // the clock went back: old times mean nothing now, every shared ban is due
			m_fails.clear();
			for (Ban& b : m_bans) b.until = now;
		}
		m_seen = true; m_last = now;
	}

	struct Failure { unsigned long long link; int bot; float at; };
	struct Ban { unsigned long long link; float until; };
	std::vector<Failure> m_fails;
	std::vector<Ban> m_bans;
	bool m_seen = false; float m_last = 0.f;
};

// The caller's side of the book (BotBrain and the tests run the same mesh calls). Mesh: BanLink, UnbanLink,
// BanLinkKeepingExits (NavMeshCore).
template <class Mesh>
LadderBanDecision ApplyLinkFailure(LadderBanBook& book, Mesh& mesh, unsigned long long link, int bot, float now,
	const float* starts, int startCount, const float* goals, int goalCount)
{
	LadderBanDecision d = book.Fail(link, bot, now);
	if (d.scope != LadderBanScope::Level) return d;
	if (startCount > 0 && goalCount > 0 && starts && goals)
	{
		if (mesh.BanLinkKeepingExits(link, starts, startCount, goals, goalCount)) return d;
		book.Revoke(link);   // the only exit: no shared ban, only this bot's cost
		d.scope = LadderBanScope::Bot; d.until = now + kLadderBotAvoidFor; d.keptExit = true;
		return d;
	}
	mesh.BanLink(link);   // no capture points known (not PS): the plain shared ban
	return d;
}
template <class Mesh>
int LiftExpiredBans(LadderBanBook& book, Mesh& mesh, float now, std::vector<unsigned long long>* lifted = nullptr)
{
	const std::vector<unsigned long long> refs = book.TakeExpired(now);
	for (unsigned long long ref : refs) mesh.UnbanLink(ref);
	if (lifted) *lifted = refs;
	return int(refs.size());
}
template <class Mesh>
int LiftAllBans(LadderBanBook& book, Mesh* mesh)
{
	const std::vector<unsigned long long> refs = book.TakeAll();
	if (mesh) for (unsigned long long ref : refs) mesh->UnbanLink(ref);
	return int(refs.size());
}
}
