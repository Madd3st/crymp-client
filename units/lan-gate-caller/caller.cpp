
#include <cstdio>
#include <cstring>
#include <string>
#include <map>
struct ICVar { std::string s; int i = 0; const char* GetString() { return s.c_str(); } int GetIVal() { return i; } };
struct IConsole { std::map<std::string, ICVar> v; ICVar* GetCVar(const char* n) { auto it = v.find(n); return it == v.end() ? nullptr : &it->second; } };
struct SEnv { IConsole* pConsole = nullptr; bool bServer = true, bMultiplayer = true, bEditor = false; };
SEnv g_env; SEnv* gEnv = &g_env;
struct ILevelSystem { bool IsLevelLoaded() { return true; } };
struct IGameRulesSystem { void* GetCurrentGameRules() { return this; } };
struct IGameFramework { ILevelSystem l; IGameRulesSystem r; ILevelSystem* GetILevelSystem() { return &l; } IGameRulesSystem* GetIGameRulesSystem() { return &r; } };
struct IActor;
struct Vec3 { float x, y, z; };
namespace WarsBots { struct BotConfig { int lanTest = 0; static BotConfig& Get() { static BotConfig c; return c; } }; }
#define WB_LOG(...) ((void)0)
#include "CryGame/Bots/BotModePlan.h"
namespace WarsBots {
class CryMPEngineAdapter {
public:
    // allowLan: wb_lan_test may widen the bind to a private LAN address (bots only); the export gate passes false.
    static bool LocalIAGate(IGameFramework* framework, bool levelReady, int optIn, bool diagnostic, bool allowPowerStruggle = false, bool allowPublic = false, bool allowLan = true);
    // Read-only exports (wb_geom_export): the same offline loopback gate, but a PowerStruggle level may be loaded with
    // its own rules (X-066: PS maps fail to start under InstantAction). Bots keep LocalIAGate, IA only.
    static bool LocalExportGate(IGameFramework* framework, bool levelReady, int optIn, bool diagnostic)
    { return LocalIAGate(framework, levelReady, optIn, diagnostic, true, false, false); }   // loopback only, never wb_lan_test
    static IActor* CreateServerActor(IGameFramework& framework, const Vec3& position, const char* name);
};
bool CryMPEngineAdapter::LocalIAGate(IGameFramework* framework, bool levelReady, int optIn, bool diagnostic, bool allowPowerStruggle, bool allowPublic, bool allowLan)
{
    if (!gEnv || !gEnv->pConsole) return false;
    ICVar* bind = gEnv->pConsole->GetCVar("sv_bind");
    ICVar* lan = gEnv->pConsole->GetCVar("sv_lanonly");
    ICVar* mode = gEnv->pConsole->GetCVar("sv_gamerules");
    const bool loaded = framework && framework->GetILevelSystem()->IsLevelLoaded();
    const bool rules = framework && framework->GetIGameRulesSystem()->GetCurrentGameRules();
    // DS-037 PS-0 #1: a public server cannot use sv_bind=127.0.0.1/sv_lanonly=1. The owner opts in explicitly.
    // Owner 06.11: wb_lan_test 1 also accepts a private LAN bind (10/8, 172.16/12, 192.168/16), still with sv_lanonly=1.
    // Not for the export gate (allowLan=false): its contract is offline loopback.
    const bool localBind = LocalBindOk(bind ? bind->GetString() : nullptr, lan ? lan->GetIVal() : 0, allowLan ? BotConfig::Get().lanTest : 0);
    const bool bindOk = GateBindOk(allowPublic, localBind);
    const bool allowed = framework && gEnv->bServer && gEnv->bMultiplayer && !gEnv->bEditor &&
        levelReady && loaded && rules && optIn==1 && bindOk && mode && (!strcmp(mode->GetString(),"InstantAction") ||
        (allowPowerStruggle && !strcmp(mode->GetString(),"PowerStruggle")));
    if (!allowed && diagnostic)
        WB_LOG(0, "[WarsBots/CryMP] GATE closed: server=%d multiplayer=%d editor=%d levelReady=%d loaded=%d rules=%d optIn=%d bind=%s lan=%d mode=%s.",
            gEnv->bServer, gEnv->bMultiplayer, gEnv->bEditor, levelReady, loaded, rules, optIn,
            bind?bind->GetString():"<missing>", lan?lan->GetIVal():-1, mode?mode->GetString():"<missing>");
    return allowed;
}
}
using namespace WarsBots;
static IConsole con; static IGameFramework fw;
static void Set(const char* bind, int lan, const char* mode, int lanTest)
{ con.v["sv_bind"].s = bind; con.v["sv_lanonly"].i = lan; con.v["sv_gamerules"].s = mode; BotConfig::Get().lanTest = lanTest; }
// the bot callers in BotManager.cpp: LocalIAGate(m_framework, m_levelReady, m_config.gateOpen, diag, true)
static bool Bots() { return CryMPEngineAdapter::LocalIAGate(&fw, true, 1, false, true); }
static bool Export() { return CryMPEngineAdapter::LocalExportGate(&fw, true, 1, false); }
int main()
{
  g_env.pConsole = &con;
  Set("127.0.0.1", 1, "PowerStruggle", 0); if (!Bots() || !Export()) return 1;          // loopback as before
  Set("192.168.77.114", 1, "PowerStruggle", 0); if (Bots() || Export()) return 2;       // LAN needs the opt-in
  Set("192.168.77.114", 1, "PowerStruggle", 1); if (!Bots()) return 3;                  // opt-in: bots on the LAN bind
  if (Export()) return 4;                                                                 // the export gate stays loopback
  Set("127.0.0.1", 1, "PowerStruggle", 1); if (!Bots() || !Export()) return 5;
  Set("192.168.77.114", 0, "PowerStruggle", 1); if (Bots()) return 6;                   // never without sv_lanonly=1
  const char* bad[] = {"0.0.0.0", "8.8.8.8", "010.0.0.1", "172.016.0.1", "192.168.01.1", "lan.local", "192.168.77.114:64087"};
  for (const char* b : bad) { Set(b, 1, "PowerStruggle", 1); if (Bots() || Export()) { std::printf("accepted %s\n", b); return 7; } }
  Set("10.1.2.3", 1, "InstantAction", 1); if (!Bots()) return 8;
  Set("10.1.2.3", 1, "TeamInstantAction", 1); if (Bots()) return 9;                     // rules filter unchanged
  g_env.bServer = false; Set("192.168.77.114", 1, "PowerStruggle", 1); if (Bots()) return 10;
  std::puts("Actual LocalIAGate/LocalExportGate caller PASS"); return 0;
}
