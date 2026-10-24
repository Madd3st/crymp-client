// Perceived-point tracking and strategic aim zone; no hidden enemy state.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstring>
#include "CryGame/Bots/BotProfile.h"
namespace WarsBots {
struct BotAimControl {
    static float ErrorRadius(const BotProfile& p,float distance) {
        return std::min(p.aimErrorMax,std::max(p.aimErrorFloor,distance*p.aimErrorPerMetre));
    }
    static float Alpha(const BotProfile& p,float dt) {
        dt=std::max(0.f,std::min(.2f,dt));
        return dt/(p.trackingLag+dt);
    }
    static bool PreferHead(const BotProfile& p,const char* weapon,float distance,unsigned sample) {
        if(distance<4 || (weapon && !std::strcmp(weapon,"Shotgun"))) return false;
        const bool scoped=weapon && (!std::strcmp(weapon,"DSG1") || !std::strcmp(weapon,"GaussRifle"));
        const float chance=std::min(.3f,p.headPreference*(scoped?1.5f:1.f));
        return float(sample%1000u)/1000.f<chance;
    }
};
}
