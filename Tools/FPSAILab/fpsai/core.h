// FPS AI Lab: engine-free tactical decision prototype for WarsBots.
// Original code written for WarsBots; no code copied from F.E.A.R., Quake III, YaPB, Omni-bot or cppGOAP.
// Everything a decision may read is in Observation, which holds only what the bot itself could
// perceive or what BotNav/CryMP could confirm. There is no field for an unseen enemy's position.
#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

namespace fpsai {

struct V3 {
	float x = 0, y = 0, z = 0;
	V3() = default;
	V3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
	V3 operator-(const V3& o) const { return { x - o.x, y - o.y, z - o.z }; }
	V3 operator+(const V3& o) const { return { x + o.x, y + o.y, z + o.z }; }
	V3 operator*(float s) const { return { x * s, y * s, z * s }; }
	float Len() const { return std::sqrt(x * x + y * y + z * z); }
	float Len2D() const { return std::sqrt(x * x + y * y); }
};

enum class Weapon : uint8_t { None, Pistol, Rifle, Shotgun, Sniper };

// A destination the navigation layer has evaluated. `verified` means BotNav returned a route
// (or the point is within direct reach); unverified options must not be planned against.
struct NavOption {
	bool exists = false;
	bool verified = false;
	V3 pos;
	float routeCost = 0;      // metres along the route
	float exposure = 1;       // 0 = hidden from the threat position, 1 = fully exposed (ray probe)
	uint32_t id = 0;          // item/cover identity, for failure memory
};

struct Observation {
	float time = 0;
	V3 self;
	float health = 100;       // 0..100
	float energy = 200;       // nanosuit 0..200
	Weapon weapon = Weapon::Pistol;
	bool hasPrimary = false;
	bool altWeaponLoaded = false; // another carried weapon has ammo in clip
	int clip = 12, clipMax = 12, reserve = 36;
	bool reloading = false;

	// Perception. lastSeen is a remembered observation, never refreshed without line of sight.
	bool enemyVisible = false;
	bool enemyKnown = false;
	uint32_t enemyId = 0;
	V3 enemyLastSeen;
	float enemyLastSeenAge = 1e9f;
	int visibleEnemies = 0;
	float lastDamageAge = 1e9f; // seconds since this bot last lost health

	// Navigation / environment answers (all from the bot's own queries).
	NavOption cover;          // best verified cover against enemyLastSeen
	NavOption weaponItem;     // best primary weapon pickup candidate
	NavOption flank;          // a verified alternative firing position
	NavOption roam;           // next roaming waypoint from the existing patrol logic
	bool routeToLastSeen = false;
	float positionExposure = 1; // exposure of current position to enemyLastSeen
	float stallSeconds = 0;     // seconds requesting movement with < 0.5 m displacement

	float EnemyDistance() const { return enemyKnown ? (enemyLastSeen - self).Len() : 1e9f; }
};

// Symbolic facts the planner reasons over. Derived from Observation, never written by actions
// except as *expected* effects inside a plan.
enum Fact : uint32_t {
	F_HasWeapon      = 1u << 0,
	F_WeaponLoaded   = 1u << 1,
	F_EnemyVisible   = 1u << 2,
	F_EnemyKnown     = 1u << 3,
	F_InCover        = 1u << 4,
	F_CoverAvailable = 1u << 5,
	F_LowHealth      = 1u << 6,
	F_UnderFire      = 1u << 7,
	F_HasPrimary     = 1u << 8,
	F_UpgradeReach   = 1u << 9,
	F_AtLastSeen     = 1u << 10,
	F_RouteLastSeen  = 1u << 11,
	F_EnemyDead      = 1u << 12,  // goal-only symbol: "the threat is dealt with"
	F_Safe           = 1u << 13,  // goal-only: not exposed while hurt
	F_Unstuck        = 1u << 14,  // goal-only
	F_Stuck          = 1u << 15,
	F_FlankAvailable = 1u << 16,
	F_AltLoaded      = 1u << 17,
	F_SearchedArea   = 1u << 18,  // goal-only
	F_RoamAvailable  = 1u << 19,
};
using Facts = uint32_t;

struct Tuning {
	float lowHealth = 35;
	float underFireWindow = 1.5f;
	float coverExposure = 0.35f;
	float atPointRadius = 3.0f;
	float stuckSeconds = 6.0f;
	float forgetSeconds = 12.0f;  // stop pursuing a last-seen point older than this
};

inline Facts DeriveFacts(const Observation& o, const Tuning& t) {
	Facts f = 0;
	if (o.weapon != Weapon::None) f |= F_HasWeapon;
	if (o.weapon != Weapon::None && o.clip > 0 && !o.reloading) f |= F_WeaponLoaded;
	if (o.enemyVisible) f |= F_EnemyVisible;
	if (o.enemyKnown && o.enemyLastSeenAge < t.forgetSeconds) f |= F_EnemyKnown;
	if (o.enemyKnown && o.positionExposure <= t.coverExposure) f |= F_InCover;
	if (o.cover.exists && o.cover.verified && o.cover.exposure <= t.coverExposure) f |= F_CoverAvailable;
	if (o.health <= t.lowHealth) f |= F_LowHealth;
	if (o.lastDamageAge < t.underFireWindow) f |= F_UnderFire;
	if (o.hasPrimary) f |= F_HasPrimary;
	if (o.weaponItem.exists && o.weaponItem.verified) f |= F_UpgradeReach;
	if (o.enemyKnown && (o.enemyLastSeen - o.self).Len2D() < t.atPointRadius) f |= F_AtLastSeen;
	if (o.routeToLastSeen) f |= F_RouteLastSeen;
	if (o.stallSeconds >= t.stuckSeconds) f |= F_Stuck;
	if (o.flank.exists && o.flank.verified) f |= F_FlankAvailable;
	if (o.altWeaponLoaded) f |= F_AltLoaded;
	if (o.roam.exists && o.roam.verified) f |= F_RoamAvailable;
	return f;
}

inline std::string FactsToString(Facts f) {
	static const char* names[] = { "HasWeapon","Loaded","EnemyVisible","EnemyKnown","InCover","CoverAvail",
		"LowHealth","UnderFire","HasPrimary","UpgradeReach","AtLastSeen","RouteLastSeen","EnemyDead","Safe",
		"Unstuck","Stuck","FlankAvail","AltLoaded","Searched","RoamAvail" };
	std::string s;
	for (int i = 0; i < 20; ++i) if (f & (1u << i)) { if (!s.empty()) s += '|'; s += names[i]; }
	return s.empty() ? "-" : s;
}

} // namespace fpsai
