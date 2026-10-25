#include "CryGame/Bots/BotWeaponHandling.h"
#include <cassert>
#include <cstdio>
using namespace WarsBots;
int main()
{
	assert(!WantZoom("SCAR", 15.f, false) && WantZoom("SCAR", 25.f, false));
	assert(WantZoom("SCAR", 15.f, true) && !WantZoom("SCAR", 12.f, true));      // hysteresis 0.7
	assert(!WantZoom("Shotgun", 50.f, false) && !WantZoom("Hurricane", 50.f, false) && !WantZoom("SOCOM", 50.f, false));
	assert(WantZoom("DSG1", 10.f, false) && !WantZoom("SCAR", -1.f, true));    // no target -> out
	assert(WantLaser(false) && !WantLaser(true));
	std::puts("BotWeaponHandlingTests OK");
	return 0;
}
