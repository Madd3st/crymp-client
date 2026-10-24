// WarsBots console backing store. All wb_* values bind directly; no tick-lagged mirror.
#pragma once
struct IConsole;
namespace WarsBots {
struct BotConfig {
 bool registered=false;
 int gateOpen=0;
 int cloakExperimental=0;
 int maxBots=6;
 int logLevel=1;
 int skill=1;
 int demoRecord=0;
 int demoHz=20;
 int progressExperiment=0;
 int trapRecovery=0;
 int physicsDiagnostics=0;
 int moveTrace=0;
 int navKneeFix=0;
 int spawnExclude195=1;
 int reviveGuard=1;
 int botPace=1;
 int reachableGoals=0;
 int hotspotAllNodes=0;
 int fireDiagnostics=0;
 int qaSocomOnly=0;
 int botLook=0;       // C-033 look policy (sprint C-047), default OFF
 int botTurnCap=0;    // C-044 per-frame view turn-rate cap, default OFF
 int weaponRank=0;    // C-022 weapon value for pickup/hold, default OFF
 int goalArbiter=0;   // C-032 goal arbiter with commitment/hysteresis, default OFF
 int botNavmesh=0;    // P0 Detour navmesh paths/reachability (Tools/RecastBake meshes), default OFF
 static BotConfig& Get(){static BotConfig config;return config;}
 void Register(IConsole& console);
 void Unregister(IConsole& console);
};
}
