// DS-049 #3: PS-2 revive buying. Engine-free chooser: one primary by style/skill/range preference, then cheap ammo.
// Prices/ids are the stock PowerStruggleBuying buyList (SCAR 150, FY71 125, shotgun 50, DSG1 200, Gauss 600;
// ammo 5/10, Gauss 50). The engine adapter calls the server-side Lua `WarsBotsBuy` only, never SvBuy/RMI.
#pragma once
#include <cstring>

namespace WarsBots {

struct PsBuyInput
{
    int pp = 0;
    int skill = 1;            // 0 beginner .. 3 elite
    int style = 0;            // BotStyle enum: 0 universal, 1 assault, 2 marksman, 3 flanker, 4 defender, 5 hunter
    float preferredRange = 20.f;
    bool hasPrimary = false;
    bool alive = false;
};

struct PsBuyPlan
{
    const char* primary = nullptr;   // buyList id: shotgun / smg
    int primaryPrice = 0;
    const char* laser = nullptr;     // "lam" (LAMRifle attachment)
    int laserPrice = 0;
    const char* ammo = nullptr;      // ammoList id for the primary
    int ammoPrice = 0;
    bool buyPrimary = false;
    bool buyLaser = false;
    bool buyAmmo = false;
    bool armorMode = true;           // PS has no armor item; the free suit mode is set on revive
};

// C-136 (owner, ESL experience 27.10): the first money always goes to a shotgun or an SMG with a laser, plus ammo;
// nobody stays with the pistol. Shotgun for close/aggressive styles (assault, flanker, hunter) or short preferred
// range, SMG otherwise; SMG falls back to the shotgun when PP is short. Laser if 25 PP remain, ammo with the rest.
inline PsBuyPlan ChoosePsBuy(const PsBuyInput& in)
{
    PsBuyPlan p;
    if (in.hasPrimary || in.pp < 50) return p;   // one primary per life; C-139: alive in a buy zone too
    const bool close = in.style == 1 || in.style == 3 || in.style == 5 || in.preferredRange < 12.f;
    if (!close && in.pp >= 75) { p.primary = "smg"; p.primaryPrice = 75; p.ammo = "smgbullet"; p.ammoPrice = 5; }
    else { p.primary = "shotgun"; p.primaryPrice = 50; p.ammo = "shotgunshell"; p.ammoPrice = 5; }
    p.buyPrimary = in.pp >= p.primaryPrice;
    int left = in.pp - (p.buyPrimary ? p.primaryPrice : 0);
    p.laser = "lam"; p.laserPrice = 25;
    p.buyLaser = p.buyPrimary && left >= p.laserPrice;
    if (p.buyLaser) left -= p.laserPrice;
    p.buyAmmo = p.buyPrimary && left >= p.ammoPrice;
    return p;
}

} // namespace WarsBots
