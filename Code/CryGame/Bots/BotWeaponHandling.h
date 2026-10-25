// Weapon handling (owner playtest 25.10, wb_bot_weapon_handling, default OFF): what a good Crysis player does with the
// gun besides pulling the trigger.
//   * LAM laser: always on when the weapon has one, off while cloaked (the beam gives the position away). The spread
//     bonus comes from the attached LAM itself (SCAR: hip min spread 1.0 -> 0.25, moving x1.0 -> x0.2; GameData.pak
//     Weapons/Multiplayer/SCAR.xml), the laser is the visible part humans expect.
//   * Zoom (iron sight / scope) for distant targets: ironsight min spread x0.0067 but moving spread x2 and slower
//     movement, so only beyond a per-weapon distance, with hysteresis; never for shotgun / minigun / pistol.
// Pure logic, no engine types; Tests/BotWeaponHandlingTests.cpp.
#pragma once
#include <cstring>

namespace WarsBots {

// Distance (m) from which a weapon is fired zoomed; 0 = never zoom.
inline float ZoomDistance(const char* cls)
{
	if (!cls) return 0.f;
	struct Z { const char* c; float d; };
	static const Z t[] = { {"SCAR", 20.f}, {"FY71", 20.f}, {"SMG", 15.f}, {"DSG1", 8.f}, {"GaussRifle", 8.f} };
	for (const Z& z : t) if (!std::strcmp(z.c, cls)) return z.d;
	return 0.f;
}

// Zoom decision with hysteresis (in above d, out below 0.7 d); no target / target distance < 0 -> unzoom.
inline bool WantZoom(const char* cls, float targetDistance, bool zoomedNow)
{
	const float d = ZoomDistance(cls);
	if (d <= 0.f || targetDistance < 0.f) return false;
	return zoomedNow ? targetDistance >= d * 0.7f : targetDistance >= d;
}

inline bool WantLaser(bool cloaked) { return !cloaked; }

} // namespace WarsBots
