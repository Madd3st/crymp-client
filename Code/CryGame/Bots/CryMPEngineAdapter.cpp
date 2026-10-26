#include "PortIncludes.h"
#include "CryGame/Bots/BotLog.h"
#include "CryMPEngineAdapter.h"
#include "CryCommon/CryAction/ILevelSystem.h"
#include "CryCommon/CryAction/IGameRulesSystem.h"
#include <cmath>
#include <cstring>
namespace WarsBots {
bool CryMPEngineAdapter::LocalIAGate(IGameFramework* framework, bool levelReady, int optIn, bool diagnostic, bool allowPowerStruggle)
{
    if (!gEnv || !gEnv->pConsole) return false;
    ICVar* bind = gEnv->pConsole->GetCVar("sv_bind");
    ICVar* lan = gEnv->pConsole->GetCVar("sv_lanonly");
    ICVar* mode = gEnv->pConsole->GetCVar("sv_gamerules");
    const bool loaded = framework && framework->GetILevelSystem()->IsLevelLoaded();
    const bool rules = framework && framework->GetIGameRulesSystem()->GetCurrentGameRules();
    const bool allowed = framework && gEnv->bServer && gEnv->bMultiplayer && !gEnv->bEditor &&
        levelReady && loaded && rules && optIn==1 && bind && !strcmp(bind->GetString(),"127.0.0.1") &&
        lan && lan->GetIVal()==1 && mode && (!strcmp(mode->GetString(),"InstantAction") ||
        (allowPowerStruggle && !strcmp(mode->GetString(),"PowerStruggle")));
    if (!allowed && diagnostic)
        WB_LOG(0, "[WarsBots/CryMP] GATE closed: server=%d multiplayer=%d editor=%d levelReady=%d loaded=%d rules=%d optIn=%d bind=%s lan=%d mode=%s.",
            gEnv->bServer, gEnv->bMultiplayer, gEnv->bEditor, levelReady, loaded, rules, optIn,
            bind?bind->GetString():"<missing>", lan?lan->GetIVal():-1, mode?mode->GetString():"<missing>");
    return allowed;
}
IActor* CryMPEngineAdapter::CreateServerActor(IGameFramework& framework, const Vec3& position, const char* name)
{
    if (!gEnv || !gEnv->bServer || !gEnv->bMultiplayer || !name || !*name || !std::isfinite(position.x) ||
        !std::isfinite(position.y) || !std::isfinite(position.z)) return nullptr;
    // ActorSystem::CreateActor safely tolerates GetChannel(0)==nullptr.
    // CActor::Init returns false if BindToNetwork fails: never bind twice.
    IActor* actor = framework.GetIActorSystem()->CreateActor(0,name,"Player",position,Quat(IDENTITY),Vec3(1,1,1));
    WB_LOG(2, "[WarsBots/CryMP] CreateActor name=%s EntityId=%u BindToNetwork=%s (CActor::Init prerequisite).",
        name, actor?actor->GetEntityId():0, actor?"true":"not confirmed");
    return actor;
}
}
