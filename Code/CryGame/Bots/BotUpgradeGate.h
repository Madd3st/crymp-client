// DS-038: should this tick look for a better weapon? The old UpdateUpgrade gate only searched while the bot had
// no primary, so a marksman that picked up an FY71 at 1.3 s never considered the DSG1/Gauss again unless the goal
// arbiter happened to be enabled. A sniper-minded bot (vantage 1 + camper) must keep looking while it holds a rifle.
// Pure logic, no engine types; Tests/BotUpgradeGateTests.cpp.
#pragma once

namespace WarsBots {

inline bool ShouldSearchUpgrade(bool hasPrimary, bool ranked, bool sniperMind, bool force, bool enemyVisible)
{
    if (force) return true;                 // the arbiter already chose a weapon tick
    if (enemyVisible) return false;         // no shopping in a visible fight
    if (!hasPrimary) return true;           // spawn pistol -> any primary
    return ranked && sniperMind;            // a marksman with a rifle still wants the sniper rifle
}

} // namespace WarsBots
