#include "CryGame/Bots/BotVehicleTarget.h"
#include <cassert>
#include <cstdio>
using namespace WarsBots;

int main()
{
	// OFF: exactly the old Classify rule (hidden or passenger -> out of scope)
	for (int h = 0; h < 2; ++h) for (int e = 0; e < 2; ++e)
	{
		assert(VehicleOutOfScope(h != 0, true, e != 0, false));
		assert(VehicleOutOfScope(h != 0, false, e != 0, false) == (h != 0));
		assert(!VehicleAimAtHull(true, h != 0, false));
	}
	assert(!VehicleRayReaches(7, 7, false));
	// ON: owner playtest 07.11 - the player in a TAC tank (hidden inside the hull) is a target
	assert(!VehicleOutOfScope(true, true, true, true));
	assert(!VehicleOutOfScope(false, true, true, true));      // open seat
	assert(VehicleOutOfScope(true, true, false, true));       // ally in a vehicle: unchanged
	assert(VehicleOutOfScope(false, true, false, true));
	assert(VehicleOutOfScope(true, false, true, true));       // hidden on foot (not spawned, cutscene): unchanged
	assert(!VehicleOutOfScope(false, false, true, true));
	assert(VehicleAimAtHull(true, true, true) && !VehicleAimAtHull(true, false, true) && !VehicleAimAtHull(false, true, true));
	assert(VehicleRayReaches(7, 7, true));
	assert(!VehicleRayReaches(8, 7, true) && !VehicleRayReaches(0, 0, true));   // other entity, static geometry (0)
	std::puts("BotVehicleTarget OK");
	return 0;
}
