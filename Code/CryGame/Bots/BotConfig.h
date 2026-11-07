// WarsBots console backing store. All wb_* values bind directly; no tick-lagged mirror.
#pragma once
struct IConsole;
namespace WarsBots {
struct BotConfig {
 bool registered=false;
 int gateOpen=0;
 int vehicleTargets=0; // owner playtest 07.11: wb_bot_vehicle_targets - an enemy in a vehicle is a target (BotVehicleTarget.h), default OFF
 int lanTest=0;        // owner 06.11: wb_lan_test - the local gate also accepts a private LAN bind, default OFF
 int maxBots=6;
 int logLevel=1;
 int skill=1;
 int demoRecord=0;
 int demoHz=20;
 int trapRecovery=0;
 int physicsDiagnostics=0;
 int moveTrace=0;
 int spawnExclude195=1;
 int reviveGuard=1;
 int botPace=1;
 int reachableGoals=0;
 int hotspotAllNodes=0;
 int fireDiagnostics=0;
 int qaSocomOnly=0;
 int botLook=1;       // C-033 look policy (sprint C-047), default ON (D-036)
 int botTurnCap=1;    // C-044 per-frame view turn-rate cap, default ON (D-036)
 int highGround=1;    // wb_bot_high_ground (owner: nobody goes to the roofs), default ON (D-036)
 int proFire=1;       // wb_bot_pro_fire (owner playtest 4: Elite stops shooting), default ON (D-036)
 int stateCommit=1;    // wb_bot_state_commit (DS-034), default ON (D-036)
 int netExt=-1;        // wb_net_ext: -1 auto, 0 vanilla wire, 1 force (BotNetCompat.h)
 int weaponRank=1;    // C-022 weapon value for pickup/hold, default ON (D-036)
 int goalArbiter=0;   // C-032 goal arbiter with commitment/hysteresis, default OFF
 int botPerception=1; // P2 C-037 view cone, time to notice, cloak (BotPerception.h), default ON (D-036)
 int aimModel=1;     // P2 C-038 human aim: saccade + tracking + filtered noise (BotAimModel.h), default ON (D-036)
 int suitPolicy=1;   // roadmap 12: one suit decision per tick (BotSuitPolicy.h), default ON (D-036)
 int clientSmooth=1; // O-025 client replica: smooth replicated direction/aim steps (BotReplicaSmooth.h), default ON (D-036)
 int botNavmesh=1; // default ON (D-036)
 int jumpLinks=1; // default ON (D-036)
 int ladder=0;        // C154/DS-107: real UP-only long-shaft ladder link (NavLinkLadder), native exit, default OFF
 int weaponHandling=1; // owner 25.10: LAM laser on (off in cloak) + zoom at distance (BotWeaponHandling.h), default ON (D-036)
 int nameProfile=0;
 int vantage=0;
 int tactics=1;        // fragmovie patterns: hold angles from cover, jump-strafe, loot after kills (BotTactics.h), default ON (D-036)
 int pathProgress=0;   // weapon-goal progress by remaining navmesh path, not straight distance (snipers), default OFF
 int unstick=0;       // SPEC-004 stuck sensor by remaining path, exit steps, Detour climb/memory/spread cost (BotUnstick.h), default OFF
 int jumpSanity=1;     // no random travel Strength hops, no obstacle jumps on navmesh paths (owner 25.10), default ON (D-016)
 int moveTraceHz=4;    // C-084 client MOVE TRACE rate for the local human (1..30), default 4
 int threatScan=1;    // owner 25.10 roof campers: short glances at high sniper spots ahead (BotThreatScan.h), default ON (D-036)
 int spectatorAim=1;   // DS-013 FP spectator of a bot: camera uses the same aim as the tracer, no second smoothing, default ON (D-036)
 int difficulty=1;     // DS-014 difficulty ladder (BotDifficulty.h): reaction/turn/aim/head/fire gate/range/senses per skill, default ON (D-036)
 int finish=1;         // DS-015 finish the damaged target, no instant switch on a one-scan LOS loss, default ON (D-036)
 int firstPrimary=1;   // DS-044: spawn pistol -> nearest reachable rifle by walk length before roam/fight, default ON (D-036)
 int withdrawCover=0;  // DS-044: low HP withdraw to the nearest LOS-breaking cover spot, not 4 m straight back, default OFF
 int sniperReach=0;    // DS-046: sniper-mind upgrade searches the roof (32 m vertical box) and only DSG1/Gauss first, default OFF
 int probeRunner=0;    // DS-047: engine adapter for BotProbeRunner.h (wb_probe_load/start/...), default OFF
 int linkFailFast=1;   // DS-048: blocked jump link fails fast + goal does not age while a link runs, default ON (D-036)
 int psCapture=0;      // DS-048: PS-1 capture goal (nearest uncaptured point, hold, next), default OFF
 int psBuy=0;         // DS-049: PS-2 revive buy (one primary by style/range, cheap ammo) via server-side Lua, default OFF
 int psDoors=0;       // DS-052/DS-054: PS bots open (never close) a closed door ahead within 5 m via the stock server Door:Open, default OFF
 int psVehicles=0;    // DS-052: PS bots enter a free car driver seat within 10 m and exit after 3 s, default OFF
 int psEvents=0;      // DS-052: Lua PS EVENT log lines (capture/purchase/vehicle/hq_hit), default OFF
 int corners=1;        // navmesh corners v2.1: aim outside the turn, pass when reached/crossed, slow into sharp turns (BotCorners.h), default ON (D-016)
 static BotConfig& Get(){static BotConfig config;return config;}
 void Register(IConsole& console);
 void Unregister(IConsole& console);
};
}
