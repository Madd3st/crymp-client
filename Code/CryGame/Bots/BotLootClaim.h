// Shore playtest 03.11: every bot that shot the victim went for its dropped gun - 4 US bots for one item, 3 for
// another, all on the slope where the fight was. The first bot to pick a drop claims it for a while; the others skip it.
// Ids and time are plain numbers (EntityId, game timer seconds). Pure logic; Tests/BotLootClaimTests.cpp.
#pragma once
#include <vector>

namespace WarsBots {

class BotLootClaims
{
public:
	// True when the item is free, expired or already ours; then it is (re)claimed until now + ttl.
	bool TryClaim(unsigned item, unsigned actor, float now, float ttl = 10.f)
	{
		for (Claim& c : m_claims)
		{
			if (c.item != item) continue;
			// now < at: the game timer went back (new level) - an old claim must not block a reused id
			if (c.actor != actor && now >= c.at && now < c.at + c.ttl) return false;
			c = Claim{item, actor, now, ttl};
			return true;
		}
		Prune(now);
		m_claims.push_back(Claim{item, actor, now, ttl});
		return true;
	}
	void Clear() { m_claims.clear(); }
	size_t Size() const { return m_claims.size(); }

private:
	struct Claim { unsigned item, actor; float at, ttl; };
	void Prune(float now)
	{
		size_t keep = 0;
		for (const Claim& c : m_claims)
			if (now >= c.at && now < c.at + c.ttl) m_claims[keep++] = c;
		m_claims.resize(keep);
	}
	std::vector<Claim> m_claims;
};

} // namespace WarsBots
