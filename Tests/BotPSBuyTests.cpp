// D-032/C-136: PS revive buy chooser — FY71 first, shotgun/SMG below 125 PP, + laser + ammo (owner rules).
#include "CryGame/Bots/BotPSBuy.h"
#include <cstdio>
#include <cstring>
#define CHECK(x) do { if (!(x)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)
using namespace WarsBots;
static PsBuyInput In(int pp, int style, float range, bool alive = false, bool primary = false)
{ PsBuyInput i; i.pp = pp; i.style = style; i.preferredRange = range; i.alive = alive; i.hasPrimary = primary; return i; }
int main()
{
    CHECK(!ChoosePsBuy(In(0, 0, 20)).buyPrimary);
    CHECK(!ChoosePsBuy(In(49, 0, 20)).buyPrimary);
    CHECK(ChoosePsBuy(In(100, 0, 20, true)).buyPrimary);           // C-139: alive buy (buy zone checked in Lua)
    CHECK(!ChoosePsBuy(In(100, 0, 20, false, true)).buyPrimary);   // already has a primary
    PsBuyPlan p = ChoosePsBuy(In(100, 0, 20));                       // universal, 100 PP: SMG + laser, no PP left
    CHECK(p.buyPrimary && !std::strcmp(p.primary, "smg") && p.buyLaser && !std::strcmp(p.laser, "lam") && !p.buyAmmo);
    p = ChoosePsBuy(In(100, 1, 20));                                 // assault: shotgun + laser + shells
    CHECK(!std::strcmp(p.primary, "shotgun") && p.buyLaser && p.buyAmmo && !std::strcmp(p.ammo, "shotgunshell"));
    p = ChoosePsBuy(In(100, 0, 8));                                  // short range -> shotgun
    CHECK(!std::strcmp(p.primary, "shotgun"));
    p = ChoosePsBuy(In(60, 0, 20));                                  // SMG unaffordable -> shotgun, no laser
    CHECK(!std::strcmp(p.primary, "shotgun") && !p.buyLaser && p.buyAmmo);
    p = ChoosePsBuy(In(120, 2, 40));                                 // marksman, 5 PP short: SMG + laser + bullets
    CHECK(!std::strcmp(p.primary, "smg") && p.buyLaser && p.buyAmmo && !std::strcmp(p.ammo, "smgbullet"));
    p = ChoosePsBuy(In(125, 2, 40));                                 // D-032: FY71 at exactly its price, nothing left
    CHECK(p.buyPrimary && !std::strcmp(p.primary, "fy71") && p.primaryPrice == 125 && !p.buyLaser && !p.buyAmmo);
    p = ChoosePsBuy(In(155, 0, 20));                                 // FY71 + laser + its own ammo
    CHECK(!std::strcmp(p.primary, "fy71") && p.buyLaser && p.buyAmmo && !std::strcmp(p.ammo, "fybullet"));
    CHECK(!std::strcmp(ChoosePsBuy(In(125, 1, 20)).primary, "fy71"));   // close style: FY71 too
    CHECK(!std::strcmp(ChoosePsBuy(In(125, 0, 8)).primary, "fy71"));    // short range: FY71 too
    CHECK(!std::strcmp(ChoosePsBuy(In(124, 1, 20)).primary, "shotgun")); // close style below the price
    CHECK(!ChoosePsBuy(In(200, 0, 20, false, true)).buyPrimary);    // one primary per life, even with FY71 money
    std::puts("BotPSBuyTests OK");
    return 0;
}
