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
    const char* primary = nullptr;   // buyList id (macs/fy71/shotgun/dsg1/gauss)
    int primaryPrice = 0;
    const char* ammo = nullptr;      // buyList ammo id for the primary
    int ammoPrice = 0;
    bool buyPrimary = false;
    bool buyAmmo = false;
    bool armorMode = true;           // PS has no armor item; the free suit mode is set on revive
};

// SNIPER (DSG1 200) for a marksman/sniper-mind; SCAR for a rifle fight; shotgun under 12 m or when PP is low;
// FY71 as the mid fallback. Ammo is bought only when at least 40 PP remain after it ("when cheap").
inline PsBuyPlan ChoosePsBuy(const PsBuyInput& in)
{
    PsBuyPlan p;
    if (in.alive || in.hasPrimary || in.pp < 50) return p;   // revive-buy only, one primary per life
    const bool sniper = in.style == 2 || in.preferredRange > 30.f;
    const bool close = in.preferredRange < 12.f;
    if (sniper)
    {
        if (in.pp >= 200) { p.primary = "dsg1"; p.primaryPrice = 200; p.ammo = "sniperbullet"; p.ammoPrice = 10; }
        else if (in.pp >= 125) { p.primary = "fy71"; p.primaryPrice = 125; p.ammo = "fybullet"; p.ammoPrice = 5; }
        else { p.primary = "shotgun"; p.primaryPrice = 50; p.ammo = "shotgunshell"; p.ammoPrice = 5; }
    }
    else if (close || in.pp < 125)
    {
        p.primary = "shotgun"; p.primaryPrice = 50; p.ammo = "shotgunshell"; p.ammoPrice = 5;
    }
    else if (in.pp >= 150)
    {
        p.primary = "macs"; p.primaryPrice = 150; p.ammo = "bullet"; p.ammoPrice = 5;
    }
    else
    {
        p.primary = "fy71"; p.primaryPrice = 125; p.ammo = "fybullet"; p.ammoPrice = 5;
    }
    p.buyPrimary = p.primary && in.pp >= p.primaryPrice;
    const int left = in.pp - (p.buyPrimary ? p.primaryPrice : 0);
    p.buyAmmo = p.buyPrimary && p.ammo && left >= p.ammoPrice + 40;
    return p;
}

} // namespace WarsBots
