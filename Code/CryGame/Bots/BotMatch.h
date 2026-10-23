#pragma once
#include "CryCommon/CryEntitySystem/IEntity.h"
#include <vector>
class CGameRules;
struct IActor;
namespace WarsBots {
// Match identity is an EntityId. It is never an entry in the connection registry.
class BotMatch {
public:
    static bool IsBot(const CGameRules& rules, EntityId id);
    static void Collect(const CGameRules& rules, std::vector<EntityId>& bots);
    static bool Register(CGameRules& rules, IActor& actor);
    static void Unregister(EntityId id);
    static void LogScores(EntityId target, EntityId shooter);
};
}
