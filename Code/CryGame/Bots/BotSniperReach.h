// DS-046: sniper reach helpers. The weapon proximity query is a 3D box; a roof item 20 m above the bot was
// outside the old +-12 m vertical box, so DSG1/Gauss were never even considered. Same module carries the
// Strength-link rise limit (the 1355 -> 1351.9 link asks for +4.30 m, above the ~3.62 m jump rise).
#pragma once
#include <cstring>

namespace WarsBots {

inline bool IsSniperClass(const char* cls)
{
    if (!cls) return false;
    return !std::strcmp(cls, "DSG1") || !std::strcmp(cls, "GaussRifle");
}

// Half-height of the weapon proximity box. Sniper searches (typical range > 30 m) need the roof above the bot.
inline float SniperSearchHalfHeight(bool enabled, bool sniperMind)
{
    return (enabled && sniperMind) ? 32.0f : 12.0f;
}

// Max Strength-jump rise at the native 9.6 m/s take-off and Crysis gravity (~12.7 m/s2): 3.62 m observed,
// 3.72 m allows the landing tolerance. Links above this are not executable and must be rejected, not retried.
inline float MaxStrengthLinkRise()
{
    return 3.72f;
}

} // namespace WarsBots
