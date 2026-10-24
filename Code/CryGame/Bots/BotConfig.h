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
 float reviveZTest=-2.f;
 int reviveHooksTest=0;
 int reachableGoals=0;
 int sinkEscape=0;
 int hotspotAllNodes=0;
 int fireDiagnostics=0;
 int qaSocomOnly=0;
 static BotConfig& Get(){static BotConfig config;return config;}
 void Register(IConsole& console);
 void Unregister(IConsole& console);
};
}
