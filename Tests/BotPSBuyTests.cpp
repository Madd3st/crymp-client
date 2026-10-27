// DS-049 #3: PS revive buy chooser tests (engine-free).
#include "CryGame/Bots/BotPSBuy.h"
#include <cstdio>
#include <cstring>

using namespace WarsBots;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static PsBuyInput In(int pp, int style, float range, int skill = 2)
{
    PsBuyInput i; i.pp = pp; i.style = style; i.preferredRange = range; i.skill = skill; return i;
}

int main()
{
    CHECK(!ChoosePsBuy(In(0, 1, 20)).buyPrimary);
    CHECK(!ChoosePsBuy(In(49, 1, 20)).buyPrimary);

    PsBuyPlan p = ChoosePsBuy(In(50, 1, 20));
    CHECK(p.buyPrimary && !std::strcmp(p.primary, "shotgun") && p.primaryPrice == 50);
    CHECK(!p.buyAmmo);                    // 0 PP left

    p = ChoosePsBuy(In(125, 1, 20));
    CHECK(!std::strcmp(p.primary, "fy71") && !p.buyAmmo);

    p = ChoosePsBuy(In(200, 1, 20));
    CHECK(!std::strcmp(p.primary, "macs") && p.primaryPrice == 150);
    CHECK(p.buyAmmo && !std::strcmp(p.ammo, "bullet"));   // 50 left, 5 ammo + 40 reserve

    p = ChoosePsBuy(In(200, 2, 40));
    CHECK(!std::strcmp(p.primary, "dsg1") && p.primaryPrice == 200 && !p.buyAmmo);

    p = ChoosePsBuy(In(300, 2, 40));
    CHECK(!std::strcmp(p.primary, "dsg1") && p.buyAmmo && !std::strcmp(p.ammo, "sniperbullet"));

    p = ChoosePsBuy(In(160, 2, 40));
    CHECK(!std::strcmp(p.primary, "fy71") && !p.buyAmmo);   // 35 left < ammo+40

    p = ChoosePsBuy(In(400, 3, 9));
    CHECK(!std::strcmp(p.primary, "shotgun") && p.buyAmmo);

    PsBuyInput owned = In(400, 1, 20); owned.hasPrimary = true;
    CHECK(!ChoosePsBuy(owned).buyPrimary);
    PsBuyInput alive = In(400, 1, 20); alive.alive = true;
    CHECK(!ChoosePsBuy(alive).buyPrimary);

    if (failures) { std::printf("ps_buy: %d FAILURES\n", failures); return 1; }
    std::printf("ps_buy: all tests passed\n");
    return 0;
}
