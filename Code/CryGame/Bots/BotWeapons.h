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
struct BotWeapons
{
	static IWeapon* Current(CActor& actor);
	static bool SelectAvailable(CActor& actor);
	// Rifles/shotgun/SMG etc. with ammunition, i.e. anything better than the spawn pistol.
	static bool HasPrimary(CActor& actor);
	static bool IsPrimary(IItem* item);
	static EntityId FindNearby(CActor& actor, float radius, IEntityClass* previousClass, unsigned life, bool primaryOnly = false,
		const EntityId* skip = nullptr, int skipCount = 0);
	static bool PickUp(CActor& actor, EntityId item);
	static bool GiveTestSCAR(CActor& actor);
	static bool HasAmmunition(CActor& actor, IWeapon& weapon);
	static bool ReloadIfNeeded(CActor& actor, IWeapon& weapon);
};
}
