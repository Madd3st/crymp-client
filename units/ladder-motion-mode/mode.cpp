
#include <cstdio>
enum {eACF_PerAnimGraph=4, eACF_AlwaysPhysics=2};
struct Coherent { bool v=true; bool Value() const{return v;} };
struct Env {bool bServer=true;} env,*gEnv=&env;
struct Manager {bool owns=true;bool Owns(unsigned){return owns;}};
struct Game {Manager mgr;Manager& GetBotManager(){return mgr;}} game,*g_pGame=&game;
namespace WarsBots {struct BotConfig {int ladder=1;static BotConfig& Get(){static BotConfig c;return c;}};}
struct Player {
 bool m_warsBotPose=true,bot=true,missing=false;
 bool NeedsWarsBotLadderPhysicsExit(){return missing;}
 struct Stats {Coherent isOnLadder;bool isExitingLadder=true;} m_stats;
 bool IsWarsBotActor(){return bot;} unsigned GetEntityId(){return 7;}
 int Mode(){struct {int flags=eACF_PerAnimGraph;} params;

			// C154: the native top exit is animation controlled (LeaveLadder). Let its graph move the actor;
			// forcing AlwaysPhysics here suppresses that exit motion. All ordinary bot motion stays physics driven.
			const bool warsBotNativeLadderExit = gEnv->bServer && g_pGame && IsWarsBotActor()
				&& g_pGame->GetBotManager().Owns(GetEntityId()) && WarsBots::BotConfig::Get().ladder==1
				&& m_stats.isOnLadder.Value() && m_stats.isExitingLadder;
			if (m_warsBotPose && (!warsBotNativeLadderExit || NeedsWarsBotLadderPhysicsExit()))
			{
				params.flags &= ~eACF_PerAnimGraph;
				params.flags |= eACF_AlwaysPhysics;
			}

 return params.flags;}
};
int main(){
 Player p;
 if(p.Mode()!=eACF_PerAnimGraph){std::puts("FAIL: owned native exit lost graph motion");return 1;}
 p.missing=true;if(p.Mode()!=eACF_AlwaysPhysics)return 10;p.missing=false;
 p.m_stats.isExitingLadder=false;if(p.Mode()!=eACF_AlwaysPhysics)return 2;
 p.m_stats.isExitingLadder=true;p.m_stats.isOnLadder.v=false;if(p.Mode()!=eACF_AlwaysPhysics)return 3;
 p.m_stats.isOnLadder.v=true;WarsBots::BotConfig::Get().ladder=0;if(p.Mode()!=eACF_AlwaysPhysics)return 4;
 WarsBots::BotConfig::Get().ladder=1;game.mgr.owns=false;if(p.Mode()!=eACF_AlwaysPhysics)return 5;
 game.mgr.owns=true;p.bot=false;if(p.Mode()!=eACF_AlwaysPhysics)return 6;
 p.bot=true;env.bServer=false;if(p.Mode()!=eACF_AlwaysPhysics)return 7;
 env.bServer=true;g_pGame=nullptr;if(p.Mode()!=eACF_AlwaysPhysics)return 8;
 g_pGame=&game;p.m_warsBotPose=false;if(p.Mode()!=eACF_PerAnimGraph)return 9;
 std::puts("Real ladder exit movement mode PASS");return 0;
}
