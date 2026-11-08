// DS-048 #3: engine side of the PS-1 capture goal. Supplies the live capture points (entity id, owner team,
// value, position) to the engine-free BotPSCapture planner.
#pragma once
#include "CryGame/Bots/BotPSCapture.h"
#include "CryGame/Bots/BotNavMeshCore.h"
#include <vector>

class CGameRules;

namespace WarsBots {
struct TeamOrder;
// SPEC-005 stage 1 (wb_bot_ps_squads 1): the commander task of this bot, or null (dead, not in a team, no navmesh).
// Ticks the team commander (4 Hz, plans every 5 s and on events); BotPSCapture.cpp.
const TeamOrder* PSTeamOrder(unsigned actor, int team, CGameRules& rules);
void PSCaptureSnapshot(std::vector<PsCapturePoint>& out, CGameRules& rules,const float* from, const NavQueryBounds* startBounds = nullptr);
}
