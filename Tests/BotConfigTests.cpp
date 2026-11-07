#include "CryGame/Bots/BotConfig.h"
#include "CryGame/Bots/BotPace.h"
#include <cassert>
using namespace WarsBots;
int main(){
 auto& config=BotConfig::Get();assert(!config.registered && !BotPaceEnabled());
 assert(config.botPace==1 && config.spawnExclude195==1 && config.reviveGuard==1);
 assert(config.lanTest==0);
 assert(config.demoRecord==0 && config.demoHz==20 && config.gateOpen==0 && config.logLevel==1 && config.maxBots==6 && config.skill==1);
 // D-036: the 20 accepted flags are ON, state_commit = 1 (not 2); already-ON pace/jump_sanity/corners stay ON.
 assert(config.highGround==1 && config.proFire==1 && config.stateCommit==1 && config.linkFailFast==1 && config.botLook==1);
 assert(config.botTurnCap==1 && config.weaponRank==1 && config.botPerception==1 && config.aimModel==1 && config.suitPolicy==1);
 assert(config.clientSmooth==1 && config.botNavmesh==1 && config.jumpLinks==1 && config.weaponHandling==1 && config.tactics==1);
 assert(config.threatScan==1 && config.spectatorAim==1 && config.difficulty==1 && config.finish==1 && config.firstPrimary==1);
 assert(config.jumpSanity==1 && config.corners==1);
 // D-038: trial and keep-debug stay OFF; PS flags unchanged.
 assert(config.ladder==0 && config.vantage==0 && config.pathProgress==0 && config.sniperReach==0);
 assert(config.goalArbiter==0 && config.nameProfile==0 && config.withdrawCover==0);
 assert(config.psCapture==0 && config.psBuy==0 && config.psDoors==0 && config.psVehicles==0 && config.trapRecovery==0);
 auto* const stable=&config;int* binding=&config.botPace;
 config.registered=true;assert(BotPaceEnabled());*binding=0;assert(!BotPaceEnabled());*binding=2;assert(!BotPaceEnabled());*binding=1;assert(BotPaceEnabled());
 config.registered=false;assert(!BotPaceEnabled());assert(&BotConfig::Get()==stable);
}
