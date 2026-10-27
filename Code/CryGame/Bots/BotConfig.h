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
 int highGround=0;    // wb_bot_high_ground (owner: nobody goes to the roofs)
 int proFire=0;       // wb_bot_pro_fire (owner playtest 4: Elite stops shooting)
 int stateCommit=0;    // wb_bot_state_commit (DS-034)
 int netExt=-1;        // wb_net_ext: -1 auto, 0 vanilla wire, 1 force (BotNetCompat.h)
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
 int tactics=0;        // fragmovie patterns: hold angles from cover, jump-strafe, loot after kills (BotTactics.h), default OFF
 int pathProgress=0;   // weapon-goal progress by remaining navmesh path, not straight distance (snipers), default OFF
 int jumpSanity=1;     // no random travel Strength hops, no obstacle jumps on navmesh paths (owner 25.10), default ON (D-016)
 int moveTraceHz=4;    // C-084 client MOVE TRACE rate for the local human (1..30), default 4
 int threatScan=0;    // owner 25.10 roof campers: short glances at high sniper spots ahead (BotThreatScan.h), default OFF
 int spectatorAim=0;   // DS-013 FP spectator of a bot: camera uses the same aim as the tracer, no second smoothing, default OFF
 int difficulty=0;     // DS-014 difficulty ladder (BotDifficulty.h): reaction/turn/aim/head/fire gate/range/senses per skill, default OFF
 int finish=0;         // DS-015 finish the damaged target, no instant switch on a one-scan LOS loss, default OFF
 int firstPrimary=0;   // DS-044: spawn pistol -> nearest reachable rifle by walk length before roam/fight, default OFF
 int withdrawCover=0;  // DS-044: low HP withdraw to the nearest LOS-breaking cover spot, not 4 m straight back, default OFF
 int finishLock=0;     // DS-044: keep a <=25 HP target until dead/lost > 1 s unless a new enemy within 8 m is hitting us, default OFF
 int sniperReach=0;    // DS-046: sniper-mind upgrade searches the roof (32 m vertical box) and only DSG1/Gauss first, default OFF
 int probeRunner=0;    // DS-047: engine adapter for BotProbeRunner.h (wb_probe_load/start/...), default OFF
 int corners=1;        // navmesh corners v2.1: aim outside the turn, pass when reached/crossed, slow into sharp turns (BotCorners.h), default ON (D-016)
 static BotConfig& Get(){static BotConfig config;return config;}
 void Register(IConsole& console);
 void Unregister(IConsole& console);
};
}
