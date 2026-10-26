// Weapon ranking (C-022, D-010 Claude module): which weapon to HOLD for a fight at a given distance and which
// pickup is WORTH the walk. Owner (playtest 19:25): "SCAR is the best, the pistol only for close fights". Today
// pickups go to the nearest suitable item regardless of class (BotWeapons.cpp:109 `score = sqrt(distance) + ...`)
// and SOCOM made 61 % of kills in the 21:02 baseline demo.
// Values are a first table to tune with demos (kills by weapon, time to primary); class names as in Crysis Wars.
// Pure logic, no engine types; Tests/BotWeaponRankTests.cpp.
#pragma once
#include <cstring>
#include <cmath>

namespace WarsBots {

struct WeaponSpec {
	const char* cls;
	float base;        // overall strength, 0..1
	float minRange;    // m: below this the weapon is clumsy (scopes, rockets, splash)
	float bestLo, bestHi;  // m: sweet spot
	float maxRange;    // m: beyond this it is close to useless
	bool primary;      // counts as "a real gun" for the arbiter (not the spawn pistol)
};

inline const WeaponSpec* WeaponTable(int& n)
{
	static const WeaponSpec t[] = {
		{"SCAR",       1.00f, 0.f,  5.f,  45.f,  90.f, true},
		{"FY71",       0.95f, 0.f,  5.f,  40.f,  80.f, true},
		{"Hurricane",  0.80f, 0.f,  3.f,  20.f,  45.f, true},   // owner 25.10: 8 kills in 50 s, too strong as a pick
		{"GaussRifle", 1.00f, 8.f, 30.f, 200.f, 300.f, true},
		{"DSG1",       0.90f, 8.f, 35.f, 200.f, 300.f, true},
		{"SMG",        0.85f, 0.f,  2.f,  20.f,  40.f, true},
		{"Shotgun",    0.90f, 0.f,  0.f,   8.f,  18.f, true},
		{"SOCOM",      0.40f, 0.f,  0.f,  10.f,  30.f, false},
	};
	n = int(sizeof(t) / sizeof(t[0]));
	return t;
}

inline const WeaponSpec* FindWeapon(const char* cls)
{
	int n; const WeaponSpec* t = WeaponTable(n);
	for (int i = 0; i < n; ++i) if (cls && !std::strcmp(t[i].cls, cls)) return &t[i];
	return nullptr;
}

// Fight value of a weapon at distance d (m) with ammo (clip + reserve > 0): base x range factor, 0 when empty/unknown.
inline float FightValue(const char* cls, float d, bool hasAmmo = true)
{
	const WeaponSpec* w = FindWeapon(cls);
	if (!w || !hasAmmo) return 0.f;
	float f;
	if (d < w->minRange) f = 0.35f;                                         // scoped/splash at point blank
	else if (d < w->bestLo) f = 0.7f + 0.3f * (d - w->minRange) / (w->bestLo - w->minRange > 0.01f ? w->bestLo - w->minRange : 0.01f);
	else if (d <= w->bestHi) f = 1.f;
	else if (d < w->maxRange) f = 1.f - 0.85f * (d - w->bestHi) / (w->maxRange - w->bestHi);
	else f = 0.1f;
	return w->base * f;
}

struct HeldWeapon { const char* cls; bool hasAmmo; };

// Index of the weapon to hold for a fight at distance d; -1 if none has ammo. `current` gets a small bonus so the bot
// does not switch for a marginal gain (switching costs ~0.6-1 s in Crysis).
inline int BestToHold(const HeldWeapon* inv, int count, float d, int current = -1, float switchMargin = 0.12f)
{
	int best = -1; float bv = 0.f;
	for (int i = 0; i < count; ++i)
	{
		float v = FightValue(inv[i].cls, d, inv[i].hasAmmo);
		if (i == current && v > 0.f) v += switchMargin;
		if (v > bv) { bv = v; best = i; }
	}
	return best;
}

// Value of picking up `cls` given what the bot holds: improvement over the best held weapon at a typical fight
// distance (Steelmill/IA ~ 20 m), plus a bonus if the bot has no primary at all. 0 = not worth a detour.
inline float PickupValue(const char* cls, const HeldWeapon* inv, int count, float typicalRange = 20.f)
{
	const WeaponSpec* w = FindWeapon(cls);
	if (!w) return 0.f;
	float held = 0.f; bool hasPrimary = false;
	for (int i = 0; i < count; ++i)
	{
		const float v = FightValue(inv[i].cls, typicalRange, inv[i].hasAmmo);
		if (v > held) held = v;
		const WeaponSpec* h = FindWeapon(inv[i].cls);
		if (h && h->primary && inv[i].hasAmmo) hasPrimary = true;
	}
	float gain = FightValue(cls, typicalRange) - held;
	if (gain < 0.f) gain = 0.f;
	if (!hasPrimary && w->primary) gain += 0.5f;
	return gain;
}

// Pickup choice cost in metres-equivalent: lower is better. Replaces `sqrt(distance)` ordering: a SCAR 25 m away beats
// a SOCOM 5 m away when the bot holds only the pistol; nothing worth it -> huge cost.
inline float PickupCost(float distance, float value, float metresPerValue = 40.f)
{
	if (value <= 0.01f) return 1e9f;
	return distance - value * metresPerValue;
}

// Distance a bot judges "which gun to hold" at when it is NOT fighting (roam, after a pickup). C-100 probes: a
// Marksman that picked up a DSG1 kept the SCAR/FY71 in hand (SelectForRange(20 m) after every pickup), and the
// vantage spots only start when a sniper rifle is CURRENT -> no sniper ever held a roof. A sniper-minded bot
// (wb_bot_vantage + camper trait) thinks at sniper range while idle; in a fight the real distance decides.
inline float IdleHoldRange(bool sniperMind) { return sniperMind ? 60.f : 20.f; }

inline bool IsSniperRifle(const char* cls)
{ return cls && (!std::strcmp(cls, "DSG1") || !std::strcmp(cls, "GaussRifle")); }

} // namespace WarsBots
