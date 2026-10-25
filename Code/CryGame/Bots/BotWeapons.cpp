#include "PortIncludes.h"
#include "CryGame/Bots/BotManager.h"
#include "CryGame/Game.h"
#include "CryGame/Bots/BotLog.h"
#include "CryGame/Bots/BotWeapons.h"
#include "CryGame/Actors/Actor.h"
#include "CryGame/Items/Item.h"
#include "CryGame/Game.h"
#include "CryCommon/CryAction/IItemSystem.h"
#include "CryCommon/CryAction/IWeapon.h"
#include "CryGame/Bots/BotConfig.h"
#include "CryGame/Bots/BotWeaponRank.h"
#include "CryGame/Bots/BotNavMesh.h"
#include "CryGame/Bots/BotNavMeshCore.h"
#include "CryGame/Bots/BotWeaponHandling.h"
#include "CryGame/Items/Weapons/Weapon.h"

namespace WarsBots
{
namespace
{
bool Suitable(IItem* item)
{
	if (!item || !item->GetIWeapon()) return false;
	const char* name = item->GetEntity()->GetClass()->GetName();
	return !strcmp(name,"SCAR") || !strcmp(name,"FY71") || !strcmp(name,"SMG") ||
		!strcmp(name,"Shotgun") || !strcmp(name,"SOCOM") || !strcmp(name,"DSG1") ||
		!strcmp(name,"GaussRifle") || !strcmp(name,"Hurricane");
}
IItemSystem* Items() { return g_pGame->GetIGameFramework()->GetIItemSystem(); }
}

IWeapon* BotWeapons::Current(CActor& actor)
{
	IItem* item = actor.GetCurrentItem();
	return Suitable(item) && item->GetOwnerId() == actor.GetEntityId() ? item->GetIWeapon() : nullptr;
}

bool BotWeapons::HasAmmunition(CActor& actor, IWeapon& weapon)
{
	IFireMode* mode = weapon.GetFireMode(weapon.GetCurrentFireMode());
	return mode && mode->GetAmmoType() && (mode->GetAmmoCount() > 0 ||
		(actor.GetInventory() && actor.GetInventory()->GetAmmoCount(mode->GetAmmoType()) > 0));
}

bool BotWeapons::IsPrimary(IItem* item)
{
	return Suitable(item) && strcmp(item->GetEntity()->GetClass()->GetName(),"SOCOM") != 0;
}

bool BotWeapons::HasPrimary(CActor& actor)
{
	auto* inventory = actor.GetInventory();
	if (!inventory) return false;
	for (int i=0; i<inventory->GetCount(); ++i)
	{
		IItem* item = Items()->GetItem(inventory->GetItem(i));
		if (IsPrimary(item) && HasAmmunition(actor, *item->GetIWeapon())) return true;
	}
	return false;
}

bool BotWeapons::SelectAvailable(CActor& actor)
{
	IItem* currentItem = actor.GetCurrentItem();
	auto* current = Current(actor);
	if (current && IsPrimary(currentItem) && HasAmmunition(actor, *current)) return true;
	auto* inventory = actor.GetInventory();
	if (!inventory) return false;
	// Prefer a primary weapon with ammunition over the pistol, then anything usable.
	for (int pass = 0; pass < 2; ++pass)
		for (int i=0; i<inventory->GetCount(); ++i)
		{
			IItem* item = Items()->GetItem(inventory->GetItem(i));
			if (!Suitable(item) || (pass == 0 && !IsPrimary(item)) || !item->CanSelect() || !HasAmmunition(actor, *item->GetIWeapon()))
				continue;
			if (item != currentItem) Items()->SetActorItem(&actor, item->GetEntityId(), false);
			return Current(actor) != nullptr;
		}
	return current && HasAmmunition(actor, *current);
}

EntityId BotWeapons::FindNearby(CActor& actor, float radius, IEntityClass* previousClass, unsigned life, bool primaryOnly,
	const EntityId* skip, int skipCount, float typicalRange)
{
	EntityId result = 0;
	float best = 1e10f;
	// Proximity query instead of iterating every entity: matters with 10-16 bots searching.
	const Vec3 origin = actor.GetEntity()->GetWorldPos();
    const BotNav& nav=g_pGame->GetBotManager().Nav();
    const int from=nav.GoalFilterEnabled()?nav.NodeAt(origin):-1;
    unsigned rejected=0;int first=-1;EntityId firstItem=0;
    BotGoalProjection firstTrace;Vec3 firstGoal(0,0,0);
	NavMeshCore* mesh = CurrentNavMesh();
	const float meshFrom[3] = {origin.x, origin.y, origin.z};
	float meshSnap[3];
	if (mesh && !mesh->Project(meshFrom, meshSnap)) mesh = nullptr; // we stand off the mesh: no filtering, old behaviour
	int meshRejected = 0;
	HeldWeapon held[8]; int heldCount = 0;
	const bool ranked = BotConfig::Get().weaponRank == 1;
	if (ranked) heldCount = Held(actor, held, 8);
	SEntityProximityQuery query;
	query.box = AABB(origin - Vec3(radius, radius, 12.0f), origin + Vec3(radius, radius, 12.0f));
	gEnv->pEntitySystem->QueryProximity(query);
	for (int q = 0; q < query.nCount; ++q)
	{
		IEntity* entity = query.pEntities[q];
		if (!entity) continue;
		if (entity->IsHidden()) continue;
		bool skipped = false;
		for (int k = 0; k < skipCount && !skipped; ++k) skipped = skip[k] == entity->GetId();
		if (skipped) continue;
		float distance = (entity->GetWorldPos() - actor.GetEntity()->GetWorldPos()).GetLengthSquared();
		if (distance > radius*radius) continue;
		IItem* item = Items()->GetItem(entity->GetId());
		if (Suitable(item) && (!primaryOnly || IsPrimary(item)) && !item->GetOwnerId() && static_cast<CItem*>(item)->CanPickUp(actor.GetEntityId()))
		{
            if (mesh) // P0: no walk path on the navmesh (roof/catwalk without stairs) -> do not even try
            {
                const Vec3 w=entity->GetWorldPos(); const float q[3]={w.x,w.y,w.z};
                if (!mesh->Reachable(meshFrom,q)) { ++meshRejected; continue; }
            }
            BotGoalProjection trace;
            if(nav.KnownUnreachable(from,entity->GetWorldPos(),trace)) {++rejected;if(first<0){first=trace.to;firstItem=entity->GetId();firstTrace=trace;firstGoal=entity->GetWorldPos();}continue;}
			// Prefer a different class, but keep travel cost significant. Stable within a life.
			unsigned hash = entity->GetId()*1664525u + life*1013904223u;
			hash ^= hash >> 16;
			float score = sqrtf(distance) + (entity->GetClass()==previousClass ? 6.0f : 0.0f) + (hash%1000)*0.005f;
			if (ranked)
			{
				// C-022: value of this pickup over what we hold, traded against the walk (BotWeaponRank.h).
				const float value = PickupValue(entity->GetClass()->GetName(), held, heldCount, typicalRange);
				if (value <= 0.01f) continue; // not worth a detour (e.g. another SMG while holding a SCAR)
				score = PickupCost(sqrtf(distance), value) + (hash%1000)*0.005f;
			}
			if (score < best) { best = score; result = entity->GetId(); }
		}
	}
    if(meshRejected) WB_LOG(2,"[WarsBots] GOAL SKIP navmesh kind=weapon actor=%u skipped=%d.",actor.GetEntityId(),meshRejected);
    if(rejected) WB_LOG(2,"[WarsBots] GOAL SKIP unreachable kind=weapon actor=%u id=%u fromScc=%d toScc=%d skipped=%u goal=(%.3f,%.3f,%.3f) toNode=%d toZ=%.3f candidates=%d reachableCandidates=%d",actor.GetEntityId(),firstItem,nav.Component(from),nav.Component(first),rejected,firstGoal.x,firstGoal.y,firstGoal.z,firstTrace.to,firstTrace.toZ,firstTrace.candidates,firstTrace.reachableCandidates);
	return result;
}

bool BotWeapons::AvailableForPickup(CActor& actor, EntityId id)
{
	IItem* item = Items()->GetItem(id);
	return Suitable(item) && !item->GetEntity()->IsHidden() && !item->GetOwnerId() &&
		static_cast<CItem*>(item)->CanPickUp(actor.GetEntityId());
}

bool BotWeapons::PickUp(CActor& actor, EntityId id)
{
	IItem* item = Items()->GetItem(id);
	if (!Suitable(item) || item->GetOwnerId() || !static_cast<CItem*>(item)->CanPickUp(actor.GetEntityId())) return false;
	// Items lie on crates/shelves: reach is 2 m horizontally and up to 2.2 m vertically.
	const Vec3 delta = item->GetEntity()->GetWorldPos() - actor.GetEntity()->GetWorldPos();
	if (delta.GetLengthSquared2D() > 4.0f || fabs_tpl(delta.z) > 2.2f) return false;
	item->PickUp(actor.GetEntityId(), false, true, false);
	item = Items()->GetItem(id);
	const bool acquired = item && item->GetOwnerId() == actor.GetEntityId() && actor.GetInventory() &&
		actor.GetInventory()->FindItem(id) >= 0;
	WB_LOG(2, "[WarsBots] REAL PICKUP actor=%u item=%u acquired=%d.", actor.GetEntityId(), id, acquired ? 1 : 0);
    // Bots use IItem::PickUp directly, bypassing CActor's eGE_ItemPickedUp wrapper.
    if(acquired)g_pGame->GetBotManager().Demo().Pickup(actor.GetEntityId(),id);
	// Switch to what we just picked up (SelectAvailable prefers primaries anyway).
	if (acquired && BotConfig::Get().weaponRank == 1) SelectForRange(actor, 20.0f); // C-022: best primary, not the first one
	return acquired && SelectAvailable(actor);
}

bool BotWeapons::GiveTestSCAR(CActor& actor)
{
	EntityId id = Items()->GiveItem(&actor, "SCAR", false, true, false);
	IItem* item = Items()->GetItem(id);
	if (!item || item->GetOwnerId() != actor.GetEntityId() || !actor.GetInventory())
	{
		WB_LOG(2, "[WarsBots] TEST GRANT SCAR failed actor=%u item=%u.", actor.GetEntityId(), id);
		return false;
	}
	IWeapon* weapon = item->GetIWeapon();
	IFireMode* mode = weapon ? weapon->GetFireMode(weapon->GetCurrentFireMode()) : nullptr;
	if (!mode || !mode->GetAmmoType() || mode->GetClipSize() <= 0) return false;
	actor.GetInventory()->SetAmmoCapacity(mode->GetAmmoType(), max(180, actor.GetInventory()->GetAmmoCapacity(mode->GetAmmoType())));
	actor.GetInventory()->SetAmmoCount(mode->GetAmmoType(), 180);
	weapon->SetAmmoCount(mode->GetAmmoType(), mode->GetClipSize());
	Items()->SetActorItem(&actor, id, false);
	WB_LOG(2, "[WarsBots] TEST GRANT SCAR actor=%u item=%u clip=%d reserve=180 (not a map pickup).",
		actor.GetEntityId(), id, mode->GetAmmoCount());
	return Current(actor) != nullptr;
}

int BotWeapons::Held(CActor& actor, HeldWeapon* out, int max, EntityId* ids)
{
	auto* inventory = actor.GetInventory();
	if (!inventory) return 0;
	int n = 0;
	for (int i = 0; i < inventory->GetCount() && n < max; ++i)
	{
		IItem* item = Items()->GetItem(inventory->GetItem(i));
		if (!Suitable(item)) continue;
		out[n].cls = item->GetEntity()->GetClass()->GetName();
		out[n].hasAmmo = HasAmmunition(actor, *item->GetIWeapon());
		if (ids) ids[n] = item->GetEntityId();
		++n;
	}
	return n;
}

float BotWeapons::HeldValue(CActor& actor, float distance)
{
	HeldWeapon held[8]; const int n = Held(actor, held, 8);
	float best = 0.f;
	for (int i = 0; i < n; ++i) best = max(best, FightValue(held[i].cls, distance, held[i].hasAmmo));
	return best;
}

bool BotWeapons::SelectForRange(CActor& actor, float distance)
{
	// C-022: among primaries with ammunition pick the best for this distance; the pistol stays a fallback that
	// SelectAvailable already handles, so the two never fight over the current item.
	HeldWeapon held[8]; EntityId ids[8]; const int n = Held(actor, held, 8, ids);
	HeldWeapon primary[8]; EntityId primaryIds[8]; int m = 0, current = -1;
	IItem* currentItem = actor.GetCurrentItem();
	for (int i = 0; i < n; ++i)
	{
		if (!held[i].hasAmmo || !strcmp(held[i].cls, "SOCOM")) continue;
		if (currentItem && currentItem->GetEntityId() == ids[i]) current = m;
		primary[m] = held[i]; primaryIds[m] = ids[i]; ++m;
	}
	const int best = BestToHold(primary, m, distance, current);
	if (best < 0 || best == current) return false;
	IItem* item = Items()->GetItem(primaryIds[best]);
	if (!item || !item->CanSelect()) return false;
	Items()->SetActorItem(&actor, primaryIds[best], false);
	WB_LOG(2, "[WarsBots] WEAPON RANK actor=%u select=%s distance=%.1f.", actor.GetEntityId(), primary[best].cls, distance);
	return true;
}

bool BotWeapons::ReloadIfNeeded(CActor& actor, IWeapon& weapon)
{
	if (weapon.IsReloading()) return true;
	IFireMode* mode = weapon.GetFireMode(weapon.GetCurrentFireMode());
	if (mode && mode->GetAmmoCount() <= 0 && actor.GetInventory() &&
		actor.GetInventory()->GetAmmoCount(mode->GetAmmoType()) > 0 && weapon.CanReload())
	{
		weapon.Reload(false);
		return true;
	}
	return false;
}

void BotWeapons::Handle(CActor& actor, float targetDistance, bool cloaked)
{
	IItem* item=actor.GetCurrentItem();
	IWeapon* weapon=item ? item->GetIWeapon() : nullptr;
	if (!weapon) return;
	CWeapon* w=static_cast<CWeapon*>(weapon);
	const char* cls=item->GetEntity()->GetClass()->GetName();
	if (w->IsLamAttached())
	{
		const bool want=WantLaser(cloaked);
		if (w->IsLamLaserActivated()!=want) w->ActivateLamLaser(want,true); // aiRequest: owner is not a player
	}
	static EntityId s_logged[64]={}; static unsigned s_next=0;   // one diagnostic line per weapon entity
	bool seen=false; for (EntityId id:s_logged) seen|=id==item->GetEntityId();
	if (!seen)
	{
		s_logged[s_next++%64]=item->GetEntityId();
		WB_LOG(2,"[WarsBots] WEAPON HANDLING actor=%u weapon=%s lam=%d zoomDistance=%.0f.",actor.GetEntityId(),cls,w->IsLamAttached()?1:0,ZoomDistance(cls));
	}
	const bool zoomed=weapon->IsZoomed();
	const bool want=WantZoom(cls,targetDistance,zoomed || weapon->IsZooming());
	if (want && !zoomed && !weapon->IsZooming() && weapon->CanZoom()) weapon->StartZoom(actor.GetEntityId(),1);
	else if (!want && (zoomed || weapon->IsZooming())) weapon->StopZoom(actor.GetEntityId());
}
}
