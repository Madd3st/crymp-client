#include "CryGame/Bots/BotConfig.h"
#include "CryGame/Bots/BotPace.h"
#include <cassert>
using namespace WarsBots;
int main(){
 auto& config=BotConfig::Get();assert(!config.registered && !BotPaceEnabled());
 assert(config.botPace==1 && config.spawnExclude195==1 && config.reviveGuard==1);
 assert(config.demoRecord==0 && config.demoHz==20 && config.gateOpen==0 && config.logLevel==1 && config.maxBots==6 && config.skill==1);
 assert(config.botLook==0 && config.botTurnCap==0 && config.weaponRank==0 && config.goalArbiter==0);
 assert(config.botNavmesh==0 && config.botPerception==0 && config.aimModel==0 && config.ladder==0);
 auto* const stable=&config;int* binding=&config.botPace;
 config.registered=true;assert(BotPaceEnabled());*binding=0;assert(!BotPaceEnabled());*binding=2;assert(!BotPaceEnabled());*binding=1;assert(BotPaceEnabled());
 config.registered=false;assert(!BotPaceEnabled());assert(&BotConfig::Get()==stable);
}
