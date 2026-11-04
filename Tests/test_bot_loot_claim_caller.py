"""Loot claims (wb_bot_tactics, PR #17 Codex review): execute the REAL BotBrain claim methods with the real
BotLootClaims registry, and check the lifecycle call sites in the real sources.

Executed (extracted from Code/CryGame/Bots/BotBrain.cpp, compiled with mocks of the engine around them):
  LootClaims, BotBrain::ClaimUpgrade, ReleaseClaims, ClearAllClaims, SetUpgradeTarget, UpgradeCandidate.
Wiring checked in the real text (the methods are too engine-bound to execute here):
  UpdateDead releases on the transition to DEAD, Stop releases before its running check, ResetLife releases,
  BotManager::EndLevel clears the shared registry, ReleaseClaims does not depend on wb_bot_tactics.
"""
from pathlib import Path
import argparse
import subprocess


def method(text, signature):
    start = text.index(signature)
    left = text.index('{', start)
    depth = 0
    for i in range(left, len(text)):
        depth += (text[i] == '{') - (text[i] == '}')
        if not depth:
            return text[start:i + 1]
    raise ValueError('Unclosed method: ' + signature)


def block(text, head):
    """The braced block that follows `head` inside text."""
    return method(text, head)


CPP = r"""
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
@LOOT_CLAIMS@
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
@METHODS@
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
"""


def main():
    root = Path(__file__).resolve().parents[1]
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', type=Path, default=root / 'units/loot-claim-caller')
    args = ap.parse_args()
    brain = (root / 'Code/CryGame/Bots/BotBrain.cpp').read_text(encoding='utf-8-sig')
    manager = (root / 'Code/CryGame/Bots/BotManager.cpp').read_text(encoding='utf-8-sig')
    out = args.out.resolve()
    out.relative_to(root)
    out.mkdir(parents=True, exist_ok=True)

    # Lifecycle wiring in the real sources.
    errors = []
    dead = method(brain, 'bool BotBrain::UpdateDead(')
    if 'ReleaseClaims();' not in block(dead, 'if (m_state != State::DEAD)'):
        errors.append('UpdateDead does not release the claim on the transition to DEAD')
    stop = method(brain, 'void BotBrain::Stop(')
    if 'ReleaseClaims();' not in stop or stop.index('ReleaseClaims();') > stop.index('if (!m_running) return;'):
        errors.append('Stop does not release the claim before its running check')
    if 'ReleaseClaims();' not in method(brain, 'void BotBrain::ResetLife('):
        errors.append('ResetLife does not release the claim')
    if 'BotBrain::ClearAllClaims();' not in method(manager, 'void BotManager::EndLevel('):
        errors.append('BotManager::EndLevel does not clear the shared claim registry')
    release = method(brain, 'void BotBrain::ReleaseClaims(')
    if 'tactics' in release:
        errors.append('ReleaseClaims depends on wb_bot_tactics; a claim survives switching it off')
    for e in errors:
        print('FAIL wiring: ' + e)
    if errors:
        raise SystemExit(2)

    loot = method(brain, 'BotLootClaims& LootClaims()')
    methods = '\n'.join(method(brain, s) for s in (
        'EntityId BotBrain::UpgradeCandidate(', 'void BotBrain::SetUpgradeTarget(', 'bool BotBrain::ClaimUpgrade(',
        'void BotBrain::ReleaseClaims(', 'void BotBrain::ClearAllClaims('))
    cpp = CPP.replace('@LOOT_CLAIMS@', loot).replace('@METHODS@', methods)
    unit = out / 'caller.cpp'
    exe = out / 'caller.exe'
    unit.write_text(cpp, encoding='utf-8')
    compiled = subprocess.run(['cl', '/nologo', '/std:c++17', '/EHsc', '/I', str(root / 'Code'), str(unit),
                               '/Fo:' + str(out) + '/', '/Fe:' + str(exe)], capture_output=True)
    print((compiled.stdout + compiled.stderr).decode(errors='replace'))
    if compiled.returncode:
        raise SystemExit(compiled.returncode)
    result = subprocess.run([str(exe)], capture_output=True)
    print((result.stdout + result.stderr).decode(errors='replace'))
    raise SystemExit(result.returncode)


if __name__ == '__main__':
    main()
