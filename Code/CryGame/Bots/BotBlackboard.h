#pragma once
#include "CryGame/Bots/BotIntent.h"
#include "CryCommon/CryEntitySystem/IEntity.h"
class CPlayer;
namespace WarsBots
{
struct BotBlackboard
{
	int health = 0;
	Vec3 position = Vec3(0,0,0), velocity = Vec3(0,0,0), lastEnemyPosition = Vec3(0,0,0);
	EntityId enemy = 0;
	float distance = 0, energy = 0, groundTime = 0;
	float nextJump = 0, nextSuitChange = 0, damagedUntil = 0;
	ENanoMode mode = NANOMODE_DEFENSE;
	bool suitActive = false, grounded = false, moving = false, blocked = false, visible = false;
	bool canMove = false;
	// DS-070: one gated water policy. `swimming` records the native ShouldSwim() state; `waterMovement` is the only
	// exception used by movement, trap recovery and native oxygen, and it is false unless BotWaterAllowed().
	bool waterAllowed = false, swimming = false, waterMovement = false;
	BotJump obstacleJump = BotJump::None;
	void Sample(CPlayer& actor, float now);
};
}
