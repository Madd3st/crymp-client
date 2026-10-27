// All pickup, selection and firing go through the SDK item/weapon interfaces.
#pragma once
#include "CryCommon/CryMath/Cry_Math.h"
#include "CryCommon/CryEntitySystem/IEntity.h"
class CActor;
struct IEntityClass;
struct IWeapon;
struct IItem;
namespace WarsBots
{
struct HeldWeapon;
struct BotWeapons
{
	static IWeapon* Current(CActor& actor);
	static bool SelectAvailable(CActor& actor);
	// Rifles/shotgun/SMG etc. with ammunition, i.e. anything better than the spawn pistol.
	static bool HasPrimary(CActor& actor);
	static bool IsPrimary(IItem* item);
	static EntityId FindNearby(CActor& actor, float radius, IEntityClass* previousClass, unsigned life, bool primaryOnly = false,
		const EntityId* skip = nullptr, int skipCount = 0, float typicalRange = 20.f, bool pathCost = false);
	static bool AvailableForPickup(CActor& actor, EntityId item);
	static bool PickUp(CActor& actor, EntityId item, float holdRange = 20.0f); // holdRange: C-104 snipers judge at 60 m
	static bool GiveTestSCAR(CActor& actor);
	static bool HasAmmunition(CActor& actor, IWeapon& weapon);
	static bool ReloadIfNeeded(CActor& actor, IWeapon& weapon);
	static bool TacticalReload(CActor& actor, float belowFraction); // between fights, clip < fraction -> reload
	// C-022 weapon rank (BotWeaponRank.h): suitable inventory items, best fight value held, range-based selection.
	static int Held(CActor& actor, HeldWeapon* out, int max, EntityId* ids = nullptr);
	static float HeldValue(CActor& actor, float distance);
	static bool SelectForRange(CActor& actor, float distance, float switchMargin = 0.12f);
	// Owner playtest 25.10 (wb_bot_weapon_handling): LAM laser on unless cloaked, zoom for distant targets
	// (BotWeaponHandling.h). targetDistance < 0 = no target to shoot at.
	static void Handle(CActor& actor, float targetDistance, bool cloaked);
	// wb_bot_tactics: a suitable dropped weapon (not the pistol, no owner, pick-up-able) within `radius` of `where`.
	static EntityId FindDrop(CActor& actor, const Vec3& where, float radius);
};
}
