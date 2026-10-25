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
 int botPerception=0; // P2 C-037 view cone, time to notice, cloak (BotPerception.h), default OFF
 int aimModel=0;     // P2 C-038 human aim: saccade + tracking + filtered noise (BotAimModel.h), default OFF
 int suitPolicy=0;   // roadmap 12: one suit decision per tick (BotSuitPolicy.h), default OFF
 int clientSmooth=0; // O-025 client replica: smooth replicated direction/aim steps (BotReplicaSmooth.h), default OFF
 int botNavmesh=0;
 int jumpLinks=0;
 int weaponHandling=0; // owner 25.10: LAM laser on (off in cloak) + zoom at distance (BotWeaponHandling.h), default OFF
 int nameProfile=0;
 int vantage=0;
 int tactics=0;        // fragmovie patterns: hold angles from cover, jump-strafe, loot after kills (BotTactics.h), default OFF        // owner 25.10: snipers (Marksman/Defender with DSG1/Gauss) hold baked sniper spots (BotVantage.h), default OFF    // owner 25.10: bot names show skill/style, e.g. "WarsBot_02 [Normal/Assault]", default OFF    // C-036 off-mesh drop/jump/Strength links + executor (BotJumpLink.h), needs meshes_links, default OFF    // P0 Detour navmesh paths/reachability (Tools/RecastBake meshes), default OFF
 static BotConfig& Get(){static BotConfig config;return config;}
 void Register(IConsole& console);
 void Unregister(IConsole& console);
};
}
