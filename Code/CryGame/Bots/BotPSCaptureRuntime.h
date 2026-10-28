// DS-048 #3: engine side of the PS-1 capture goal. Supplies the live capture points (entity id, owner team,
// value, position) to the engine-free BotPSCapture planner.
#pragma once
#include "CryGame/Bots/BotPSCapture.h"
#include "CryGame/Bots/BotNavMeshCore.h"
#include <vector>

class CGameRules;

namespace WarsBots {
void PSCaptureSnapshot(std::vector<PsCapturePoint>& out, CGameRules& rules,const float* from, const NavQueryBounds* startBounds = nullptr);
}
