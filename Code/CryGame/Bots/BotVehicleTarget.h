// WarsBots: an enemy in a vehicle as a target (wb_bot_vehicle_targets). Engine-free rules; BotTargeting/BotCombat apply them.
// Owner playtest 07.11: bots never shot at the player in a vehicle - BotTargeting::Classify put every passenger out of
// scope (Relation::Invalid) and a closed hull hides the passenger entity, so the sight ray could not reach it either.
#pragma once

namespace WarsBots
{
// Out of targeting scope. OFF (default) keeps the old rule: hidden entities and every vehicle passenger. ON: an enemy
// passenger stays a target even when the closed hull hides it; allies in vehicles and other hidden entities stay out.
inline bool VehicleOutOfScope(bool hidden, bool inVehicle, bool enemy, bool vehicleTargets)
{
	if (inVehicle) return !(vehicleTargets && enemy);
	return hidden;
}

// Where to aim at a passenger: the hull centre when the hull hides the passenger (tank, APC, closed cabin), the
// passenger itself on an open seat (jeep, boat, gunner position).
inline bool VehicleAimAtHull(bool inVehicle, bool hidden, bool vehicleTargets)
{ return vehicleTargets && inVehicle && hidden; }

// A sight or fire ray that stops on the target's own vehicle reaches the target (the hull is what gets hit).
inline bool VehicleRayReaches(unsigned hitEntity, unsigned targetVehicle, bool vehicleTargets)
{ return vehicleTargets && targetVehicle != 0 && hitEntity == targetVehicle; }
}
