#include "CryGame/Bots/BotWeaponRank.h"
#include <cassert>
#include <cstdio>
using namespace WarsBots;

int main()
{
	// 1. Mid range: SCAR beats SOCOM by far; the pistol is a fallback.
	assert(FightValue("SCAR", 20.f) > 2.f * FightValue("SOCOM", 20.f));
	// 2. Point blank: shotgun is the best; sniper rifles are clumsy; far: Gauss/DSG1 lead, shotgun useless.
	assert(FightValue("Shotgun", 3.f) > FightValue("SCAR", 3.f) - 0.05f);
	assert(FightValue("DSG1", 3.f) < FightValue("SMG", 3.f));
	assert(FightValue("GaussRifle", 80.f) > FightValue("SCAR", 80.f));
	assert(FightValue("Shotgun", 40.f) < 0.15f);
	// 3. Hold choice: SCAR over SOCOM at 15 m; SOCOM only when the SCAR is empty.
	{
		HeldWeapon inv[] = {{"SOCOM", true}, {"SCAR", true}};
		assert(BestToHold(inv, 2, 15.f) == 1);
		inv[1].hasAmmo = false;
		assert(BestToHold(inv, 2, 15.f) == 0);
		HeldWeapon none[] = {{"SCAR", false}};
		assert(BestToHold(none, 1, 15.f) == -1);
	}
	// 4. No marginal switching: holding FY71 at 20 m, SCAR is barely better -> keep FY71.
	{
		HeldWeapon inv[] = {{"FY71", true}, {"SCAR", true}};
		assert(BestToHold(inv, 2, 20.f, 0) == 0);
	}
	// 5. Pickups: with only a pistol a SCAR 25 m away beats a SOCOM 5 m away; with a SCAR in hand another SMG is not
	//    worth a detour.
	{
		HeldWeapon pistol[] = {{"SOCOM", true}};
		const float scar = PickupCost(25.f, PickupValue("SCAR", pistol, 1));
		const float socom = PickupCost(5.f, PickupValue("SOCOM", pistol, 1));
		assert(scar < socom);
		HeldWeapon rifle[] = {{"SOCOM", true}, {"SCAR", true}};
		assert(PickupValue("SMG", rifle, 2) < 0.01f);
		assert(PickupCost(10.f, PickupValue("SMG", rifle, 2)) > 1e8f);
	}
	std::puts("BotWeaponRankTests OK");
	return 0;
}
