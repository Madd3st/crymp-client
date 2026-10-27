#include "PortIncludes.h"
#include "BotConfig.h"
#include "CryCommon/CrySystem/IConsole.h"
namespace WarsBots {
namespace {
struct IntBinding {const char* name;int BotConfig::*field;const char* help;};
const IntBinding kInts[]={
 {"wb_bot_high_ground",&BotConfig::highGround,"Owner playtests 3-4 (nobody wants to go to the roofs; he fights from height): in a quiet moment a Veteran (25 %) or Elite (45 %) every 20 s takes a high spot with a view (tactical_spots flag 4) with any rifle, holds and fights from it (BotVantage), for up to 70 s. Loads <Level>.spots.txt. 0 OFF default. Exit: promote after a batch shows time on high ground and kills from height up, stalls within noise."},
 {"wb_bot_pro_fire",&BotConfig::proFire,"Owner playtest 4 (Elites sometimes stop shooting): backing off at close range and the low-HP withdraw keep firing at a visible target (they only aimed), a visible target within fire range drops cloak at once, and mid-fight weapon re-selects need a clear gain (margin 0.3, every 4 s). 0 OFF default. Exit: promote after a batch shows Elite no-fire stretches down, kills up."},
 {"wb_bot_state_commit",&BotConfig::stateCommit,"DS-034 (owner playtest 3: bots twitch): roam writes its state once per tick after the weapon-goal decision (was SEARCHING then MOVING_TO_WEAPON every tick, 89 A-B-A flips per bot-minute) and a visible target keeps ATTACKING for >= 0.5 s. 0 OFF default. Exit: promote after a batch shows flips down and kills/stalls within noise."},
 {"wb_net_ext",&BotConfig::netExt,"Owner 26.10 (Malformed packet on public servers): WarsBots player-stream fields only between WarsBots ends. -1 auto (default: server on; client on only for a loopback/LAN server address), 0 vanilla wire, 1 force. Set before connecting. Exit: keep until T-3 adds a real handshake."},
 {"wb_demo_record",&BotConfig::demoRecord,"Opt-in server wbdemo/2 recorder; exit: diagnostic-only, default OFF"},
 {"wb_demo_hz",&BotConfig::demoHz,"Diagnostic demo sample rate10..60; default20"},
 {"wb_probe_local_test",&BotConfig::gateOpen,"Opt in to WarsBots; requires sv_bind=127.0.0.1, sv_lanonly=1 and InstantAction."},
 {"wb_probe_runner",&BotConfig::probeRunner,"DS-047: engine adapter for BotProbeRunner.h, default OFF. When 1: wb_probe_load file, wb_probe_start, wb_probe_runner_status, wb_probe_stop, wb_probe_run id seconds. WarsBots log lines feed the scenario metrics; the result JSON is written next to the bot log and PROBE RESULT is logged."},
 {"wb_bot_cloak_experimental",&BotConfig::cloakExperimental,"Experimental bot cloak, defaults OFF"},
 {"wb_hotspot_all_nodes",&BotConfig::hotspotAllNodes,"Experimental complete hotspot scan: 0 legacy stride2 default, 1 all nodes; QA until 10min soak."},
 {"wb_reachable_goals",&BotConfig::reachableGoals,"Opt-in graph reachability filter for weapon/hotspot goals; unknown projection kept, default OFF."},
 {"wb_bot_pace",&BotConfig::botPace,"C018 native pace: stance speed,sprint40/20,Speed130/60,Strength70; default ON; set0 for legacy pace."},
 {"wb_revive_guard",&BotConfig::reviveGuard,"C014 candidate safety:2s post-revive check, max2 native retries; no kill/score change."},
 {"wb_spawn_exclude195",&BotConfig::spawnExclude195,"C014 candidate safety: automatic bot spawn selection excludes195; humans/manual debug unaffected."},
 {"wb_nav_knee_fix",&BotConfig::navKneeFix,"Export-only audit requested variant:0 OFF,1 knee0.8,2 conditional step; latched by wb_nav_audit; never normal AI."},
 {"wb_move_trace",&BotConfig::moveTrace,"Diagnostic-only opt-in trace:1=4Hz humans/server bots;2=also per-frame client remote bots. IA/default0,200000line cap; entity/view in PostUpdate, not raw packets."},
 {"wb_physics_diagnostics",&BotConfig::physicsDiagnostics,"Opt-in read-only body snapshots after create/revive, +1s, +5s; local IA only, default OFF."},
 {"wb_trap_recovery",&BotConfig::trapRecovery,"Experimental local IA displacement watchdog: 0 OFF, 1 staged recovery incl native self-kill after 45s; QA only until 10min soak."},
 {"wb_progress_experiment",&BotConfig::progressExperiment,"Experimental stationary-combat recovery: 0 OFF (default), 1 ON; local IA bots only."},
 {"wb_qa_socom_only",&BotConfig::qaSocomOnly,"Local IA test only: suppress weapon upgrades/fallback, retain native SOCOM/ammo; default OFF."},
 {"wb_bot_look",&BotConfig::botLook,"C-033 look policy: without a visible target look ~3 m ahead along the path; points of interest only with a clear eye ray. 0 OFF default. Exit: promote only after owner playtest; retire gate/legacy path when validated."},
 {"wb_bot_turn_cap",&BotConfig::botTurnCap,"C-044 per-frame view turn-rate cap by skill (beginner 400 .. elite 900 deg/s) instead of 10 Hz look snaps. 0 OFF default. Exit: promote only after owner playtest; retire gate/legacy path when validated."},
 {"wb_bot_weapon_rank",&BotConfig::weaponRank,"C-022 weapon value by range: pickups by value/distance, hold the best weapon for the fight distance. 0 OFF default. Exit: promote only after owner playtest; retire gate/legacy path when validated."},
 {"wb_bot_arbiter",&BotConfig::goalArbiter,"C-032 goal arbiter: fight/hunt/weapon/roam with commitment windows and hysteresis. 0 OFF default. Exit: promote only after owner playtest; retire gate/legacy path when validated."},
 {"wb_bot_perception",&BotConfig::botPerception,"C-037 perception: new targets need the 110 deg view cone and time to notice (skill reaction); cloak hides beyond 12 m unless firing. 0 OFF default. Exit: promote only after owner playtest; retire gate/legacy path when validated."},
 {"wb_bot_aim_model",&BotConfig::aimModel,"C-038 aim model: flick (Fitts) + tracking with lead + filtered hand noise by skill, replaces the low-pass + sine aim. 0 OFF default. Exit: promote only after owner playtest; retire gate/legacy path when validated."},
 {"wb_bot_suit_policy",&BotConfig::suitPolicy,"One nanosuit decision per tick: falling/under fire/fight Armor, Strength jumps with energy, Speed travel 130/60, stealth Cloak only if cloak is enabled. 0 OFF default."},
 {"wb_bot_client_smooth",&BotConfig::clientSmooth,"O-025 CLIENT cvar: remote WarsBot replicas turn in curves - replicated move direction (80 ms) and look point (50 ms) are filtered per frame instead of stepping at the server's 10 Hz; jumps >120 deg snap. Server unaffected, no network change. 0 OFF default."},
 {"wb_bot_navmesh",&BotConfig::botNavmesh,"P0 Detour navmesh from %LOCALAPPDATA%/WarsBots/Navmesh/<Level>.navmesh for paths, weapon/roam reachability; BotNav graph fallback. 0 OFF default. Exit: promote only after owner playtest; retire gate/legacy path when validated."},
 {"wb_bot_jump_links",&BotConfig::jumpLinks,"C-036 navmesh paths may use baked drop / jump / Strength-jump links (needs <Level>.navmesh + <Level>.links.txt from Tools/RecastBake/meshes_links); BotJumpLink executes them, a failed link is banned for the level. Lifts/ladders never. Requires wb_bot_navmesh 1. 0 OFF default."},
 {"wb_bot_weapon_handling",&BotConfig::weaponHandling,"Owner 25.10: LAM laser always on when attached (off while cloaked), iron sight/scope zoom for targets beyond a per-weapon distance (SCAR/FY71 20 m, SMG 15, DSG1/Gauss 8; never shotgun/minigun/pistol). 0 OFF default."},
 {"wb_bot_name_profile",&BotConfig::nameProfile,"Owner 25.10: new bots are named with skill and style, e.g. WarsBot_02 [Normal/Assault] (spectator, kill feed, scoreboard). Applies to bots added after the change. 0 OFF default."},
 {"wb_bot_vantage",&BotConfig::vantage,"Owner 25.10: Marksman/Defender bots holding a DSG1 or Gauss take, hold and rotate baked sniper spots (<Level>.spots.txt from Tools/RecastBake/tactical_spots, next to the navmesh); Marksmen value sniper rifles as pickups. 0 OFF default."},
 {"wb_bot_tactics",&BotConfig::tactics,"Fragmovie patterns: a target that broke line of sight is held from a nearby cover spot that still sees it (needs <Level>.spots.txt), duels with jump-strafe by skill/style, dropped weapons near a kill are looted. 0 OFF default."},
 {"wb_bot_path_progress",&BotConfig::pathProgress,"A weapon goal counts as progressing while the remaining navmesh path shrinks (not the straight distance): routes that first lead away - Steelmill DSG1 up the stairs - were dropped as unreachable after 8 s, so no bot ever held a sniper rifle. Requires wb_bot_navmesh 1. 0 OFF default."},
 {"wb_bot_jump_sanity",&BotConfig::jumpSanity,"Owner playtest 25.10 (364 Strength jumps, 58 on baked links): no random Strength hop while travelling, and no obstacle-ray jumps while following a navmesh path (stair steps read as walls). Link jumps and trap-recovery jumps stay. 1 ON default (D-016: two paired 15x660 runs + owner playtest 25.10)."},
 {"wb_move_trace_hz",&BotConfig::moveTraceHz,"C-084: sampling rate of the client MOVE TRACE for the local human (owner circle/wall jumps need >= 20). Server and bot rows stay at 4 Hz. Rows also carry stance now. 4 default."},
 {"wb_bot_threat_scan",&BotConfig::threatScan,"Owner 25.10 (camped roofs with a sniper rifle, no bot ever looked up): without a visible target a bot glances for 0.6 s at a high sniper spot (tactical_spots flag 4) 10-70 m ahead and at least 2.5 m above its eye, only with a clear eye ray; every 8/5/3.5 s for Normal/Veteran/Elite, never for Beginner. Needs wb_bot_look 1, wb_bot_vantage 1 (loads the spots) and <Level>.spots.txt. 0 OFF default."},
 {"wb_bot_spectator_aim",&BotConfig::spectatorAim,"DS-013 / owner 25.10 (bot aim late and crooked in first-person spectator): the spectator camera follows the same replicated aim as the tracer instead of a second 42/s lerp on top of BotReplicaSmooth. Client-side, visual only. 0 OFF default."},
 {"wb_bot_difficulty",&BotConfig::difficulty,"DS-014 / owner 25.10 (difficulties felt the same): one table per skill sets reaction (0.9/0.5/0.28/0.08 s), view turn cap (250/500/850/1200 deg/s), aim error (2.5/1.2/0.6/0.1 deg), head zone (0/8/35/95 %), fire gate (20/10/5/2 deg), engage/fire range (x0.55/0.85/1.15/1.45) and senses (50/65/80/100 m, cone 90-120). Elite = honest head aimbot: LOS rules unchanged. Applied when a bot is added. Cone and hearing act only with wb_bot_perception 1 (the owner preset has it). 0 OFF default."},
 {"wb_bot_finish",&BotConfig::finish,"DS-015 / owner 25.10 (left an enemy at 4 HP): a target this bot has visibly damaged (>= 30 HP in the last 5 s) is kept while seen within 2 s unless the bot is low and someone else shoots it; any remembered target survives a one-scan line-of-sight loss for 1 s instead of an instant switch. 0 OFF default."},
 {"wb_bot_first_primary",&BotConfig::firstPrimary,"DS-044 #1 (audit: first primary p50 33.5 s, SOCOM held 35.7 %): on spawn go for the nearest reachable rifle before roam/fight unless an enemy is within 10 m; candidates are scored by navmesh walk length (PickupCost), not straight distance. 0 OFF default. Exit: first-primary p50 < 15 s, SOCOM time < 15 %."},
 {"wb_bot_withdraw_cover",&BotConfig::withdrawCover,"DS-044 #2 (audit: 80 % of withdraws die within 2.5 s): low HP backs off to the nearest cover spot (tactical_spots flag 1/2, PickCover) that breaks LOS to the threat and is reachable, keeps firing while moving, Armor; abort and fight if no cover within 8 m. Needs wb_bot_tactics 1 + <Level>.spots.txt. 0 OFF default. Exit: withdraw deaths < 50 %."},
 {"wb_bot_finish_lock",&BotConfig::finishLock,"DS-044 #3 (audit: 22.4 % of target choices switch away from <=25 HP): keep a target at <=25 HP until it is dead or lost for > 1 s, unless a new enemy within 8 m is the one hitting us. 0 OFF default. Exit: switch-away from <=25 HP < 5 %."},
 {"wb_bot_sniper_reach",&BotConfig::sniperReach,"DS-046 (C-117 X-157: 0/3 DSG1, 0/3 Gauss): a sniper-minded marksman searches the roof with a 32 m vertical proximity box (DSG1 is +20.3 m, Gauss +13.0 m above the bot; the old +-12 m box never listed them) and tries only DSG1/Gauss until one is held; the weapon goal gets 20 s progress / 90 s total instead of 8/60. 0 OFF default. Exit: DSG1/Gauss pickup > 2/3."},
 {"wb_bot_corners",&BotConfig::corners,"Navmesh path corners v2 (BotCorners.h): each corner is aimed at 0.15 m to the outside of the turn (mesh erosion 0.3 m vs player capsule 0.4 m), passed only when reached (0.35 m) or crossed with the next leg in straight reach, and sharp turns drop sprint/Speed within 2.5 m. Targets the Steelmill stair-mouth and spawn-176 wedges. Requires wb_bot_navmesh 1. 1 ON default (D-016: two paired 15x660 runs + owner playtest 25.10)."},
 {"wb_fire_diagnostics",&BotConfig::fireDiagnostics,"Opt-in shot/hit diagnostics; at most 20000 records per level; default OFF."},
 {"wb_log",&BotConfig::logLevel,"WarsBots console log: 0 essential, 1 +kills/revives (default), 2 +telemetry. Telemetry always goes to the log file."},
 {"wb_maxbots",&BotConfig::maxBots,"Maximum simultaneous WarsBots (1..16, default 6)"},
 {"wb_skill",&BotConfig::skill,"Default skill for wb_add: 0 beginner, 1 normal, 2 veteran, 3 elite"},
};
}
void BotConfig::Register(IConsole& console){
 if(registered)return;
 *this=BotConfig{};
 for(const auto& b:kInts)console.Register(b.name,&(this->*b.field),this->*b.field,0,b.help);
 registered=true;
}
void BotConfig::Unregister(IConsole& console){
 if(!registered)return;registered=false;
 for(const auto& b:kInts)console.UnregisterVariable(b.name,true);
}
}
