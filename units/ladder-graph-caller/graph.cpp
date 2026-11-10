
#define WARSBOTS_EXPERIMENTAL_SPAWNER 1
#define WB_LOG(...) ((void)0)
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdint>
using string=std::string;using uint8=uint8_t;
struct Env{bool bServer=true;}env,*gEnv=&env;
namespace WarsBots{struct BotConfig{int ladder=1;static BotConfig&Get(){static BotConfig c;return c;}};}
struct Manager{bool owns=true;bool Owns(unsigned){return owns;}};
struct Game{Manager mgr;Manager&GetBotManager(){return mgr;}}game,*g_pGame=&game;
struct IAnimationGraphState{
 using InputID=uint8;string value="combat";bool active=false,def=false,valid=true,reject=false;int writes=0,clears=0;
 InputID GetInputId(const char*){return 3;}const char*GetInputName(InputID){return valid?"Stance":nullptr;}
 void GetInput(InputID,char*buf){std::strcpy(buf,value.c_str());}
 bool IsDefaultInputValue(InputID){return def;}bool GetAnimationActivation(){return active;}
 void SetAnimationActivation(bool v){active=v;++writes;}
 bool SetInput(InputID,const char*v){++writes;if(reject)return false;value=v;def=false;return true;}
 void ClearInput(InputID){++writes;++clears;value="combat";def=true;}
};
struct IAnimatedCharacter{IAnimationGraphState*graph=nullptr;IAnimationGraphState*GetAnimationGraphState(){return graph;}};
struct Flag{bool v=true;bool Value(){return v;}};
struct CPlayer{
 enum{eLS_Climb=1,eLS_ExitTop=2,eLS_Exit=3};bool bot=true,frozen=false;int health=100,physicalStance=0;
 struct{Flag isOnLadder;bool isRagDoll=false;}m_stats;
 IAnimationGraphState graph;IAnimatedCharacter ac;IAnimatedCharacter*m_pAnimatedCharacter=&ac;
 IAnimationGraphState*m_warsBotLadderGraph=nullptr;
 bool m_warsBotLadderGraphOverride=false,m_warsBotLadderGraphWasActive=false,m_warsBotLadderGraphStanceDefault=true;
 uint8 m_warsBotLadderGraphStanceId=0;string m_warsBotLadderGraphStance;
 CPlayer(){ac.graph=&graph;}
 bool IsWarsBotActor(){return bot;}unsigned GetEntityId(){return 7;}int GetHealth(){return health;}bool IsFrozen(){return frozen;}
 void ResetWarsBotLadderExitRetry(){} void ClearWarsBotLadderExitRetry(){} // isolated graph snapshot test; actual retry lifecycle has its own caller
 void SetWarsBotLadderExitPhysics(bool,bool){} bool m_warsBotLadderExitTargetValid=false; bool PrepareWarsBotLadderGraph();void RestoreWarsBotLadderGraph();void NativePrefix(int);
};
bool CPlayer::PrepareWarsBotLadderGraph()
{
	if (!gEnv || !gEnv->bServer || !g_pGame || !IsWarsBotActor() ||
		!g_pGame->GetBotManager().Owns(GetEntityId()) || WarsBots::BotConfig::Get().ladder!=1 ||
		!m_stats.isOnLadder.Value() || GetHealth()<=0 || IsFrozen() || m_stats.isRagDoll) return false;
	IAnimationGraphState* graph=m_pAnimatedCharacter ? m_pAnimatedCharacter->GetAnimationGraphState() : nullptr;
	if (!graph) { WB_LOG(2,"[WarsBots] LADDER GRAPH refused actor=%u reason=no_graph.",GetEntityId()); return false; }
	const IAnimationGraphState::InputID stance=graph->GetInputId("Stance");
	const char* name=graph->GetInputName(stance);
	if (!name || std::strcmp(name,"Stance"))
	{ WB_LOG(2,"[WarsBots] LADDER GRAPH refused actor=%u reason=stance_input id=%u name=%s.",GetEntityId(),unsigned(stance),name?name:"<none>"); return false; }
	if (m_warsBotLadderGraphOverride && m_warsBotLadderGraph!=graph) RestoreWarsBotLadderGraph();
	if (!m_warsBotLadderGraphOverride)
	{
		ResetWarsBotLadderExitRetry(); // a new native route starts with a fresh one-shot exit retry
		char previous[256]={}; graph->GetInput(stance,previous);
		m_warsBotLadderGraph=graph; m_warsBotLadderGraphStanceId=stance;
		m_warsBotLadderGraphStance=previous;
		m_warsBotLadderGraphStanceDefault=graph->IsDefaultInputValue(stance);
		m_warsBotLadderGraphWasActive=graph->GetAnimationActivation();
		m_warsBotLadderGraphOverride=true;
		WB_LOG(2,"[WarsBots] LADDER GRAPH prepare actor=%u activeBefore=%d stanceBefore=%s defaultBefore=%d.",
			GetEntityId(),m_warsBotLadderGraphWasActive?1:0,previous,m_warsBotLadderGraphStanceDefault?1:0);
	}
	// Player's physical relaxed stance is not configured. Set the animation input only: the installed
	// native top-exit state inherits RelaxedNW, while normal bot movement requests the combat stance.
	if (!graph->GetAnimationActivation()) graph->SetAnimationActivation(true);
	if (!graph->SetInput(stance,"relaxed"))
	{ WB_LOG(2,"[WarsBots] LADDER GRAPH refused actor=%u reason=set_stance.",GetEntityId()); RestoreWarsBotLadderGraph(); return false; }
	return graph->GetAnimationActivation();
}
void CPlayer::RestoreWarsBotLadderGraph()
{
	m_warsBotLadderExitTargetValid=false;
	SetWarsBotLadderExitPhysics(false,false); // release our collider layer even if the graph was replaced
	ClearWarsBotLadderExitRetry(); // also consume orphan latches before the early return
	if (!m_warsBotLadderGraphOverride) return;
	m_warsBotLadderGraphOverride=false; // consume even if teardown has replaced the graph or entity ownership
	IAnimationGraphState* graph=m_pAnimatedCharacter ? m_pAnimatedCharacter->GetAnimationGraphState() : nullptr;
	if (gEnv && gEnv->bServer && g_pGame && IsWarsBotActor() &&
		g_pGame->GetBotManager().Owns(GetEntityId()) && graph && graph==m_warsBotLadderGraph)
	{
		if (m_warsBotLadderGraphStanceDefault || m_warsBotLadderGraphStance.empty())
			graph->ClearInput(m_warsBotLadderGraphStanceId);
		else graph->SetInput(m_warsBotLadderGraphStanceId,m_warsBotLadderGraphStance.c_str());
		graph->SetAnimationActivation(m_warsBotLadderGraphWasActive);
		WB_LOG(2,"[WarsBots] LADDER GRAPH restored actor=%u active=%d.",GetEntityId(),m_warsBotLadderGraphWasActive?1:0);
	}
	m_warsBotLadderGraph=nullptr; m_warsBotLadderGraphStance.clear();
}
void CPlayer::NativePrefix(int eLS){

#if defined(WARSBOTS_EXPERIMENTAL_SPAWNER)
	if (eLS==eLS_Climb || eLS==eLS_ExitTop) PrepareWarsBotLadderGraph();
#endif

}

int main(){
 {CPlayer p;p.NativePrefix(CPlayer::eLS_Climb);
  if(!p.graph.active||p.graph.value!="relaxed"||!p.m_warsBotLadderGraphOverride){std::puts("FAIL native prefix does not prepare relaxed active graph");return 1;}
  if(p.physicalStance!=0)return 2;
  p.NativePrefix(CPlayer::eLS_ExitTop);WarsBots::BotConfig::Get().ladder=0;p.RestoreWarsBotLadderGraph();
  if(p.graph.active||p.graph.value!="combat"||p.m_warsBotLadderGraphOverride)return 3;
  int n=p.graph.writes;p.RestoreWarsBotLadderGraph();if(p.graph.writes!=n)return 4;WarsBots::BotConfig::Get().ladder=1;
 }
 {CPlayer p;p.graph.active=true;p.graph.value="crouch";p.NativePrefix(CPlayer::eLS_Climb);p.RestoreWarsBotLadderGraph();if(!p.graph.active||p.graph.value!="crouch")return 5;}
 {CPlayer p;p.graph.def=true;p.NativePrefix(CPlayer::eLS_Climb);p.RestoreWarsBotLadderGraph();if(p.graph.clears!=1||!p.graph.def)return 6;}
 {CPlayer p;p.graph.valid=false;p.NativePrefix(CPlayer::eLS_Climb);if(p.graph.writes||p.m_warsBotLadderGraphOverride)return 7;}
 {CPlayer p;p.graph.reject=true;p.NativePrefix(CPlayer::eLS_Climb);if(p.graph.active||p.graph.value!="combat"||p.m_warsBotLadderGraphOverride)return 8;}
 {CPlayer p;p.NativePrefix(CPlayer::eLS_Climb);IAnimationGraphState replacement;p.ac.graph=&replacement;p.RestoreWarsBotLadderGraph();if(replacement.writes||p.m_warsBotLadderGraphOverride)return 9;}
 {CPlayer p;p.NativePrefix(CPlayer::eLS_Climb);game.mgr.owns=false;int n=p.graph.writes;p.RestoreWarsBotLadderGraph();if(p.graph.writes!=n||p.m_warsBotLadderGraphOverride)return 10;game.mgr.owns=true;}
 {CPlayer p;p.NativePrefix(CPlayer::eLS_Exit);if(p.graph.writes)return 11;}
 for(int k=0;k<8;++k){CPlayer p;
  if(k==0)p.bot=false;if(k==1)env.bServer=false;if(k==2)game.mgr.owns=false;
  if(k==3)WarsBots::BotConfig::Get().ladder=0;if(k==4)p.health=0;if(k==5)p.frozen=true;
  if(k==6)p.m_stats.isRagDoll=true;if(k==7)p.m_stats.isOnLadder.v=false;
  p.NativePrefix(CPlayer::eLS_Climb);if(p.graph.writes||p.m_warsBotLadderGraphOverride)return 12+k;
  env.bServer=true;game.mgr.owns=true;WarsBots::BotConfig::Get().ladder=1;
 }
 {CPlayer p;p.ac.graph=nullptr;p.NativePrefix(CPlayer::eLS_Climb);if(p.m_warsBotLadderGraphOverride)return 20;}
 std::puts("Actual native graph prefix, scope and restoration controls PASS");return 0;
}
