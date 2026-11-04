// One bot per dropped weapon (Shore 03.11: 4 US bots for item 130919).
#include "CryGame/Bots/BotLootClaim.h"
#include <cassert>
#include <cstdio>
using namespace WarsBots;

int main()
{
	BotLootClaims claims;
	// The first shooter claims the drop, the other three skip it.
	assert(claims.TryClaim(130919, 6, 100.f));
	assert(!claims.TryClaim(130919, 8, 100.5f));
	assert(!claims.TryClaim(130919, 10, 101.f));
	assert(!claims.TryClaim(130919, 12, 109.9f));
	// The claimant may renew; another drop is free.
	assert(claims.TryClaim(130919, 6, 105.f));
	assert(claims.TryClaim(130933, 8, 105.f));
	// After the claim expires anyone may take it.
	assert(claims.TryClaim(130919, 10, 115.1f));
	// Game timer back (new level, reused id): the old claim does not block.
	assert(claims.TryClaim(130933, 14, 3.f));
	// One claim per bot: a new item frees the old one, death frees it at once.
	claims.Clear();
	assert(claims.TryClaim(500, 3, 10.f));
	assert(claims.TryClaim(501, 3, 11.f));
	assert(claims.TryClaim(500, 5, 11.f));
	assert(!claims.TryClaim(501, 5, 12.f));
	claims.Release(3);
	assert(claims.TryClaim(501, 5, 12.f));
	assert(claims.Size() == 1);
	// Expired claims are pruned when new items come in.
	claims.Clear();
	for (unsigned i = 0; i < 50; ++i) assert(claims.TryClaim(1000 + i, 1, float(i) * 20.f));
	assert(claims.Size() == 1);
	std::puts("BotLootClaimTests OK");
	return 0;
}
