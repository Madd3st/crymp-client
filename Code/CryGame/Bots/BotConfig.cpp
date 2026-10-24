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
 {"wb_move_trace",&BotConfig::moveTrace,"Opt-in4Hz human/bot motion trace, frame-latched jumps/landings; IA only, default OFF,200000line cap."},
 {"wb_physics_diagnostics",&BotConfig::physicsDiagnostics,"Opt-in read-only body snapshots after create/revive, +1s, +5s; local IA only, default OFF."},
 {"wb_trap_recovery",&BotConfig::trapRecovery,"Experimental local IA displacement watchdog: 0 OFF, 1 staged recovery incl native self-kill after 45s; QA only until 10min soak."},
 {"wb_progress_experiment",&BotConfig::progressExperiment,"Experimental stationary-combat recovery: 0 OFF (default), 1 ON; local IA bots only."},
 {"wb_qa_socom_only",&BotConfig::qaSocomOnly,"Local IA test only: suppress weapon upgrades/fallback, retain native SOCOM/ammo; default OFF."},
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
