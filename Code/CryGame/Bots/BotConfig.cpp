#include "PortIncludes.h"
#include "BotConfig.h"
#include "CryCommon/CrySystem/IConsole.h"
namespace WarsBots {
namespace {
struct IntBinding {const char* name;int BotConfig::*field;const char* help;};
const IntBinding kInts[]={
 {"wb_demo_record",&BotConfig::demoRecord,"Opt-in server wbdemo/2 recorder; exit: diagnostic-only, default OFF"},
 {"wb_demo_hz",&BotConfig::demoHz,"Diagnostic demo sample rate10..60; default20"},
 {"wb_probe_local_test",&BotConfig::gateOpen,"Opt in to WarsBots; requires sv_bind=127.0.0.1, sv_lanonly=1 and InstantAction."},
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
 {"wb_bot_corners",&BotConfig::corners,"Navmesh path corners: a corner within 1.5 m is passed only when the navmesh shows a straight walk to the next one (no turn cut into the wall it goes around), and corners near a mesh edge move 0.15 m inward (mesh erosion 0.3 m vs player capsule 0.4 m). Fixes the Steelmill stair-mouth wedge (c068 trap). Requires wb_bot_navmesh 1. 0 OFF default."},
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
