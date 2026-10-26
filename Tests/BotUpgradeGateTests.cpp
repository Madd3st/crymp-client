// DS-038 tests: the upgrade gate and the sniper pickup choice (engine-free).
#include "CryGame/Bots/BotUpgradeGate.h"
#include "CryGame/Bots/BotWeaponRank.h"
#include <cassert>
#include <cstdio>

using namespace WarsBots;

int main()
{
    // Gate: spawn pistol -> search; a rifle with no sniper mind -> stop; a marksman with a rifle -> keep looking.
    assert(ShouldSearchUpgrade(false, true, false, false, false));
    assert(ShouldSearchUpgrade(false, false, true, false, false));
    assert(!ShouldSearchUpgrade(true, true, false, false, false));
    assert(!ShouldSearchUpgrade(true, false, true, false, false));
    assert(ShouldSearchUpgrade(true, true, true, false, false));
    assert(!ShouldSearchUpgrade(true, true, true, false, true));   // visible enemy
    assert(ShouldSearchUpgrade(true, true, true, true, true));     // arbiter forced it

    // Choice: at 60 m an FY71 is worth .546, DSG1 .900, SCAR .717. DSG1 is 24 m away, SCAR 41.7 m.
    HeldWeapon held[] = {{"FY71", true}};
    const float dsg = PickupValue("DSG1", held, 1, 60.f);
    const float scar = PickupValue("SCAR", held, 1, 60.f);
    const float gauss = PickupValue("GaussRifle", held, 1, 60.f);
    assert(dsg > 0.30f && dsg < 0.40f);
    assert(scar > 0.10f && scar < 0.25f);
    assert(gauss > 0.40f && gauss < 0.50f);
    const float costDsg = PickupCost(24.0f, dsg);
    const float costScar = PickupCost(41.7f, scar);
    const float costGauss = PickupCost(91.0f, gauss);
    assert(costDsg < costScar);       // the nearby marginal SCAR must not beat the sniper rifle
    assert(costDsg < costGauss);
    assert(PickupValue("Hurricane", held, 1, 60.f) <= 0.01f);

    std::printf("upgrade gate: all tests passed (DSG1 cost %.2f, SCAR %.2f)\n", costDsg, costScar);
    return 0;
}
