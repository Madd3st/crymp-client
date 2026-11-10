
#include "CryGame/Bots/BotLootClaim.h"
#include <cstdint>
#include <cstdio>
#include <vector>
#define WB_LOG(...) ((void)0)
using EntityId = uint32_t;
struct ITimer { float t = 100.f; float GetCurrTime() const { return t; } };
struct SSystemGlobalEnvironment { ITimer* pTimer; };
ITimer g_timer; SSystemGlobalEnvironment g_env{&g_timer}; SSystemGlobalEnvironment* gEnv = &g_env;
struct CPlayer {};
namespace WarsBots {
struct BotConfig { int tactics = 1, vantage = 0; static BotConfig& Get() { static BotConfig c; return c; } };
// Items in order of preference; the real FindNearby returns the best item that is not in skip.
std::vector<EntityId> g_items;
struct BotWeapons {
	static EntityId FindNearby(CPlayer&, float, void*, unsigned, bool, const EntityId* skip, int skipCount, float, bool, bool)
	{
		for (EntityId id : g_items)
		{
			bool skipped = false;
			for (int i = 0; i < skipCount; ++i) if (skip[i] == id) skipped = true;
			if (!skipped) return id;
		}
		return 0;
	}
};
namespace {
BotLootClaims& LootClaims() { static BotLootClaims claims; return claims; }
}
class BotBrain {
public:
	struct BadItem { EntityId id; float until; };
	std::vector<BadItem> m_badItems;
	float m_now = 0, m_upgradeBest = 1e9f, m_upgradeBestAt = 0, m_upgradeSince = 0;
	unsigned m_upgradePathSig = 0, m_life = 1;
	EntityId m_upgradeTarget = 0, m_upgradeTracked = 0;
	int m_number = 1;
	float PickupRange() const { return 20.f; }
	EntityId UpgradeCandidate(CPlayer& actor, bool anyHeld, bool pathCost = false, bool sniperOnly = false);
	void SetUpgradeTarget(EntityId id);
	bool ClaimUpgrade(EntityId id);
	void ReleaseClaims();
	static void ClearAllClaims();
};
EntityId BotBrain::UpgradeCandidate(CPlayer& actor,bool anyHeld,bool pathCost,bool sniperOnly)
{
	(void)anyHeld; // with wb_bot_weapon_rank FindNearby keeps only real upgrades over what is held
	EntityId skip[16]; int skipCount = 0;
	for (const BadItem& b : m_badItems) if (b.id && m_now < b.until) skip[skipCount++] = b.id;
	// wb_bot_tactics: an item another bot walks to is not a candidate - its second-best gun is (PR #17 review)
	if (BotConfig::Get().tactics==1)
	{
		unsigned held[8];
		const size_t n=LootClaims().HeldByOthers(unsigned(m_number),gEnv->pTimer->GetCurrTime(),held,8);
		for (size_t i=0;i<n;++i) skip[skipCount++]=held[i];
	}
	// owner 25.10 (wb_bot_vantage): Marksmen judge pickups at sniper range, so DSG1/Gauss become real upgrades
	const float typical=PickupRange();
	// c066: no bot ever reached DSG1 (22 m up) / Gauss (tower) within 45 m -> snipers look across the map
	return BotWeapons::FindNearby(actor, typical>30.0f ? 200.0f : 45.0f, nullptr, m_life, true, skip, skipCount, typical, pathCost, sniperOnly);
}
void BotBrain::SetUpgradeTarget(EntityId id)
{
	if (m_upgradeTarget!=id)
		WB_LOG(2,"[WarsBots] #%d WEAPON GOAL previous=%u target=%u event=%s.",m_number,m_upgradeTarget,id,id?"acquire":"clear");
	if (m_upgradeTarget && !id) ReleaseClaims(); // given up, picked up or worthless: free it for the others now
	m_upgradeTarget = id;
	m_upgradeTracked = 0; m_upgradeBest = 1e9f; m_upgradeBestAt = 0; m_upgradePathSig = 0;
	m_upgradeSince = 0; // re-anchored on the first tick that actually pursues the goal
}
bool BotBrain::ClaimUpgrade(EntityId id)
{
	if (BotConfig::Get().tactics!=1 || !id) return true;
	if (LootClaims().TryClaim(id,unsigned(m_number),gEnv->pTimer->GetCurrTime())) return true;
	WB_LOG(2,"[WarsBots] #%d LOOT skip item=%u reason=claimed.",m_number,id);
	return false;
}
void BotBrain::ReleaseClaims() { LootClaims().Release(unsigned(m_number)); }
void BotBrain::ClearAllClaims() { LootClaims().Clear(); }
} // namespace WarsBots
using namespace WarsBots;

static int fails = 0;
#define CHECK(c, what) do { if (!(c)) { std::printf("FAIL: %s\n", what); ++fails; } } while (0)

int main()
{
	CPlayer actor;
	BotBrain a, b; a.m_number = 1; b.m_number = 2;
	BotConfig& cfg = BotConfig::Get();
	g_items = {77, 88};

	// ON: the first claimer wins, the other is rejected and its weapon search picks the second-best gun.
	cfg.tactics = 1; g_timer.t = 100.f;
	CHECK(a.ClaimUpgrade(77), "first claim");
	CHECK(!b.ClaimUpgrade(77), "other bot's claim is rejected");
	CHECK(b.UpgradeCandidate(actor, false) == 88, "search skips the claimed item and picks the second gun");
	CHECK(a.UpgradeCandidate(actor, false) == 77, "own claim is not skipped");

	// Renewed while walking: claim at 108 holds to 118, not 110.
	g_timer.t = 108.f; CHECK(a.ClaimUpgrade(77), "renew own claim");
	g_timer.t = 115.f; CHECK(!b.ClaimUpgrade(77), "renewed claim still blocks after the first TTL");
	g_timer.t = 118.5f; CHECK(b.ClaimUpgrade(77), "an unrenewed claim expires");
	b.ReleaseClaims();

	// Death (UpdateDead -> ReleaseClaims): the item is free at once, not at revive or TTL.
	g_timer.t = 200.f; CHECK(a.ClaimUpgrade(77), "claim before death");
	a.ReleaseClaims();
	CHECK(b.ClaimUpgrade(77), "free right after the claimer died");
	b.ReleaseClaims();

	// Giving the goal up (SetUpgradeTarget(0)) releases the claim.
	g_timer.t = 210.f; CHECK(a.ClaimUpgrade(77), "claim for the goal"); a.m_upgradeTarget = 77;
	a.SetUpgradeTarget(0);
	CHECK(b.ClaimUpgrade(77), "free after the goal was cleared");
	b.ReleaseClaims();

	// tactics switched OFF while a claim is held: Stop/death still release it.
	g_timer.t = 300.f; CHECK(a.ClaimUpgrade(77), "claim with tactics ON");
	cfg.tactics = 0; a.ReleaseClaims(); cfg.tactics = 1;
	CHECK(b.ClaimUpgrade(77), "released although tactics was OFF at cleanup");
	b.ReleaseClaims();

	// OFF: no claim is written and nothing is skipped (behaviour as before the feature).
	cfg.tactics = 1; g_timer.t = 400.f; CHECK(a.ClaimUpgrade(77), "claim before OFF");
	cfg.tactics = 0;
	CHECK(b.ClaimUpgrade(77), "OFF: ClaimUpgrade always true");
	CHECK(b.UpgradeCandidate(actor, false) == 77, "OFF: search does not skip claimed items");
	CHECK(b.ClaimUpgrade(99) && b.ClaimUpgrade(98), "OFF: claims of anything succeed");
	cfg.tactics = 1;
	a.ReleaseClaims(); b.ReleaseClaims();

	// New level with a monotonic timer: without the clear an old claim blocks a reused item id, with it not.
	g_timer.t = 500.f; CHECK(a.ClaimUpgrade(77), "claim on the old level");
	g_timer.t = 501.f; CHECK(!b.ClaimUpgrade(77), "precondition: a stale claim would block the reused id");
	BotBrain::ClearAllClaims();
	CHECK(b.ClaimUpgrade(77), "EndLevel clear frees a reused id while the timer keeps running");
	BotBrain::ClearAllClaims();

	// New level with the timer going back: the old claim does not block.
	g_timer.t = 600.f; CHECK(a.ClaimUpgrade(77), "claim before the timer reset");
	g_timer.t = 3.f; CHECK(b.ClaimUpgrade(77), "timer went back: old claim ignored");
	BotBrain::ClearAllClaims();

	// One claim per bot: a new item replaces the old one.
	g_timer.t = 700.f; CHECK(a.ClaimUpgrade(77) && a.ClaimUpgrade(88), "switch item");
	CHECK(b.ClaimUpgrade(77), "the previous item is free after the switch");
	CHECK(!b.ClaimUpgrade(88), "the new item is held");

	if (fails) return 1;
	std::puts("PASS real loot-claim callers: reject, second gun, renew, death, goal clear, tactics OFF at cleanup, OFF, level clear, timer back, one claim per bot");
	return 0;
}
