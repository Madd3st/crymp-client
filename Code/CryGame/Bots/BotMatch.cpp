#include "PortIncludes.h"
#include "BotMatch.h"
#include "CryGame/Game.h"
#include "CryGame/GameRules.h"
#include "CryGame/Actors/Player/Player.h"
#include "CryGame/Bots/BotManager.h"
#include "CryCommon/CryScriptSystem/IScriptSystem.h"
#include <algorithm>
#include <cstring>
namespace WarsBots {
bool BotMatch::IsBot(const CGameRules& rules, EntityId id)
{
    if (!id || strcmp(rules.GetEntity()->GetClass()->GetName(),"InstantAction")) return false;
    IActor* actor=g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(id);
    if (!actor || strcmp(actor->GetEntity()->GetClass()->GetName(),"Player") ||
        !static_cast<CPlayer*>(actor)->IsWarsBotActor()) return false;
    return !gEnv->bServer || g_pGame->GetBotManager().Owns(id);
}
void BotMatch::Collect(const CGameRules& rules, std::vector<EntityId>& bots)
{
    bots.clear();
    if (strcmp(rules.GetEntity()->GetClass()->GetName(),"InstantAction")) return;
    IActorIteratorPtr it=g_pGame->GetIGameFramework()->GetIActorSystem()->CreateActorIterator();
    while (IActor* actor=it->Next()) if (IsBot(rules,actor->GetEntityId())) bots.push_back(actor->GetEntityId());
}
bool BotMatch::Register(CGameRules& rules, IActor& actor)
{
    if (!gEnv->bServer || !IsBot(rules,actor.GetEntityId())) return false;
    IScriptTable* rulesTable=rules.GetEntity()->GetScriptTable();
    IScriptTable* actorTable=actor.GetEntity()->GetScriptTable();
    if (!rulesTable || !actorTable) return false;
    bool groups=false;
    rulesTable->GetValue("USE_SPAWN_GROUPS",groups);
    if (groups) { CryLogAlways("[WarsBots] IA registration refused: spawn-group channel callbacks unsupported."); return false; }
    bool attached=false;
    rulesTable->GetValue("warsBotsIAAttached",attached);
    if (!attached)
    {
        if (!gEnv->pScriptSystem->ExecuteFile("Scripts/WarsBots/InstantAction.lua")) return false;
        SmartScriptTable hooks;
        // CallMethod pushes the table as self: WarsBotsIA:Attach(rules) semantics preserved.
        if (!gEnv->pScriptSystem->GetGlobalValue("WarsBotsIA",hooks) || !hooks.GetPtr() ||
            !Script::CallMethod(hooks.GetPtr(),"Attach",rulesTable)) return false;
        rulesTable->GetValue("warsBotsIAAttached",attached);
        if (!attached) return false;
    }
    // Reset first: a failed call must not leave a half-registered participant flag.
    if (!Script::CallMethod(rulesTable,"ResetScore",ScriptHandle(actor.GetEntityId()))) return false;
    actorTable->SetValue("warsBotParticipant",true);
    // Ping is a display field, not evidence of a connection.
    int pingKey=0;
    if (rulesTable->GetValue("SCORE_PING_KEY",pingKey)) rules.SetSynchedEntityValue(actor.GetEntityId(),TSynchedKey(pingKey),0);
    CryLogAlways("[WarsBots] IA participant registered EntityId=%u channelId=0 nativeIsPlayer=0 matchCount=%d.",
        actor.GetEntityId(),rules.GetMatchParticipantCount(true));
    return true;
}
void BotMatch::Unregister(EntityId id)
{
    if (!gEnv || !gEnv->bServer || !g_pGame || !id) return;
    CGameRules* rules=g_pGame->GetGameRules();
    if (!rules || strcmp(rules->GetEntity()->GetClass()->GetName(),"InstantAction")) return;
    IScriptTable* table=rules->GetEntity()->GetScriptTable();
    if (!table) return;
    for (const char* name : {"SCORE_KILLS_KEY","SCORE_DEATHS_KEY","SCORE_HEADSHOTS_KEY","SCORE_PING_KEY"})
    {
        int key=0;
        if (table->GetValue(name,key)) rules->ClearSynchedEntityValue(id,TSynchedKey(key));
    }
    if (IEntity* entity=gEnv->pEntitySystem->GetEntity(id))
        if (IScriptTable* actorTable=entity->GetScriptTable()) actorTable->SetToNull("warsBotParticipant");
    CryLogAlways("[WarsBots] IA participant removed EntityId=%u (scores cleared, no channel removed).",id);
}
void BotMatch::LogScores(EntityId target, EntityId shooter)
{
    IActor* victim=g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(target);
    if (!victim || victim->GetHealth()>0) return;
    CGameRules* rules=g_pGame->GetGameRules();
    if (!rules || (!IsBot(*rules,target) && !IsBot(*rules,shooter))) return;
    for (EntityId id : {target,shooter})
    {
        if (!id) continue;
        int kills=0,deaths=0;
        rules->GetSynchedEntityValue(id,TSynchedKey(100),kills);
        rules->GetSynchedEntityValue(id,TSynchedKey(101),deaths);
        CryLogAlways("[WarsBots] IA SCORE EntityId=%u kills=%d deaths=%d.",id,kills,deaths);
    }
}
}
