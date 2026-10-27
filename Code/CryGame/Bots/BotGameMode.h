// DS-050 #2: one game-mode policy for InstantAction and PowerStruggle. Replaces the scattered
// `strcmp("PowerStruggle")` / `IsPowerStruggle` branches with a policy object. Engine-free (pure string in,
// policy out) and unit-tested in Tests/BotGameModeTests.cpp.
#pragma once
#include "CryGame/Bots/BotModePlan.h"

namespace WarsBots {

enum class BotGameModeKind { Unknown = 0, InstantAction, PowerStruggle };

struct BotGameModePolicy
{
    BotGameModeKind kind = BotGameModeKind::Unknown;
    bool botRules = false;           // InstantAction or PowerStruggle
    bool teamBeforeSpawn = false;    // PS-0: SetTeam/spawn group before registration
    bool teamSpawn = false;          // PS: GetSpawnLocationTeam instead of the IA spawn picker
    bool waveRevive = false;         // PS: the Lua wave owns the revive; 30 s fallback only
    bool captureGoals = false;       // PS-1: wb_ps_capture roam goal
    bool reviveBuy = false;          // PS-2: wb_ps_buy revive purchase
    bool attachParticipant = false;  // PS: WarsBotsPS register/attach
    bool spawnGroups = false;        // rules use spawn groups (PS)
};

inline BotGameModePolicy PolicyForGameRulesClass(const char* className)
{
    BotGameModePolicy p;
    if (!className) return p;
    if (!std::strcmp(className, "InstantAction"))
    {
        p.kind = BotGameModeKind::InstantAction;
        p.botRules = true;
        return p;
    }
    if (!std::strcmp(className, "PowerStruggle"))
    {
        p.kind = BotGameModeKind::PowerStruggle;
        p.botRules = true;
        p.teamBeforeSpawn = true;
        p.teamSpawn = true;
        p.waveRevive = true;
        p.captureGoals = true;
        p.reviveBuy = true;
        p.attachParticipant = true;
        p.spawnGroups = true;
        return p;
    }
    return p;
}

class IBotGameMode
{
public:
    virtual ~IBotGameMode() {}
    virtual BotGameModeKind Kind() const = 0;
    virtual const BotGameModePolicy& Policy() const = 0;
};

class BotGameModeStatic final : public IBotGameMode
{
public:
    explicit BotGameModeStatic(const BotGameModePolicy& policy) : m_policy(policy) {}
    BotGameModeKind Kind() const override { return m_policy.kind; }
    const BotGameModePolicy& Policy() const override { return m_policy; }
private:
    BotGameModePolicy m_policy;
};

inline const IBotGameMode& BotGameModeFor(const char* className)
{
    static const BotGameModeStatic unknown(PolicyForGameRulesClass(nullptr));
    static const BotGameModeStatic instantAction(PolicyForGameRulesClass("InstantAction"));
    static const BotGameModeStatic powerStruggle(PolicyForGameRulesClass("PowerStruggle"));
    if (className && !std::strcmp(className, "InstantAction")) return instantAction;
    if (className && !std::strcmp(className, "PowerStruggle")) return powerStruggle;
    (void)unknown;
    return unknown;
}

} // namespace WarsBots
