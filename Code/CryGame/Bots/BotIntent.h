#pragma once
#include "CryCommon/CryMath/Cry_Math.h"
#include "CryCommon/CryAISystem/IAgent.h"
#include "CryCommon/CryCore/CrySizer.h"
struct IGameFramework;
#include "CryGame/Actors/Player/NanoSuit.h"

namespace WarsBots
{
enum class BotGait { Walk, Run, Sprint };
enum class BotJump { None, Normal, Strength };
enum class BotCombatIntent { None, Aim, Fire };

// A value describing a decision; no physics or entity mutations here.
struct BotIntent
{
	bool move = false;
	bool strafe = false; // Face aim independently of world-space movement.
	Vec3 destination = Vec3(0,0,0), aim = Vec3(0,0,0);
	BotGait gait = BotGait::Run;
	BotJump jump = BotJump::None;
	EStance stance = STANCE_STAND;
	ENanoMode suitMode = NANOMODE_DEFENSE;
	BotCombatIntent combat = BotCombatIntent::None;
	bool linkJump = false; // C-036: jump on a baked navmesh link (arc checked offline) - skip the local JumpSpace probe
	bool meshPath = false;
	// T-1: source line (BotBrain.cpp) of the last write per channel this tick; 0 = default
	struct Why { short suitMode = 0, jump = 0, gait = 0, aim = 0, move = 0; } why; // destination is a Detour corner (wb_bot_navmesh): stairs are walkable, skip the knee ray
	void MoveTo(const Vec3& point, const Vec3& look) { move = true; destination = point; aim = look; }
	void AimAt(const Vec3& look) { move = false; aim = look; combat = BotCombatIntent::Aim; }
};
}
