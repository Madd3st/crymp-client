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
	BotJump obstacleJump = BotJump::None;
	void Sample(CPlayer& actor, float now);
};
}
