// C-136: PS revive buy chooser — shotgun/SMG + laser + ammo (owner rule).
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
    CHECK(!ChoosePsBuy(In(100, 0, 20, true)).buyPrimary);          // alive: no revive buy
    CHECK(!ChoosePsBuy(In(100, 0, 20, false, true)).buyPrimary);   // already has a primary
    PsBuyPlan p = ChoosePsBuy(In(100, 0, 20));                       // universal, 100 PP: SMG + laser, no PP left
    CHECK(p.buyPrimary && !std::strcmp(p.primary, "smg") && p.buyLaser && !std::strcmp(p.laser, "lam") && !p.buyAmmo);
    p = ChoosePsBuy(In(100, 1, 20));                                 // assault: shotgun + laser + shells
    CHECK(!std::strcmp(p.primary, "shotgun") && p.buyLaser && p.buyAmmo && !std::strcmp(p.ammo, "shotgunshell"));
    p = ChoosePsBuy(In(100, 0, 8));                                  // short range -> shotgun
    CHECK(!std::strcmp(p.primary, "shotgun"));
    p = ChoosePsBuy(In(60, 0, 20));                                  // SMG unaffordable -> shotgun, no laser
    CHECK(!std::strcmp(p.primary, "shotgun") && !p.buyLaser && p.buyAmmo);
    p = ChoosePsBuy(In(120, 2, 40));                                 // marksman: SMG + laser + bullets
    CHECK(!std::strcmp(p.primary, "smg") && p.buyLaser && p.buyAmmo && !std::strcmp(p.ammo, "smgbullet"));
    std::puts("BotPSBuyTests OK");
    return 0;
}
