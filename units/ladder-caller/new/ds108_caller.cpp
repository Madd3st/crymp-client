
#define WARSBOTS_EXPERIMENTAL_SPAWNER 1
#include "CryGame/Bots/BotLadder.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <initializer_list>
using namespace WarsBots;
using EntityId = unsigned;

#define WB_LOG(...) ((void)0)
template<class T> T fabs_tpl(T v){return (T)std::fabs((double)v);}
enum EStance { STANCE_STAND = 0 };
enum ENanoMode { NANOMODE_DEFENSE = 3 };
enum class BotGait { Walk, Run, Sprint };
enum class BotJump { None, Normal, Strength };
enum class BotCombatIntent { None, Aim, Fire };

struct Vec3 { float x=0,y=0,z=0; Vec3(){} Vec3(float a,float b,float c):x(a),y(b),z(c){}
 Vec3 operator-(const Vec3&o)const{return Vec3(x-o.x,y-o.y,z-o.z);} Vec3 operator+(const Vec3&o)const{return Vec3(x+o.x,y+o.y,z+o.z);}
 float len()const{return std::sqrt(x*x+y*y+z*z);} float GetLengthSquared2D()const{return x*x+y*y;} };
struct JVec { float x=0,y=0,z=0; JVec(){} JVec(float a,float b,float c):x(a),y(b),z(c){} };
struct CoherentBool { bool v=false; bool Value()const{return v;} void operator=(bool b){v=b;} };
struct SPlayerStats {
 CoherentBool isOnLadder, isFrozen, inFreefall;
 bool isExitingLadder=false, isRagDoll=false, isRagdoll=false, inZeroG=false, isGrabbed=false;
 int spectatorMode=0, flyMode=0;
 Vec3 ladderTop, ladderBottom, ladderOrientation;
};
struct CAnimation { struct { int m_nAnimID[1]={0}; } m_LMG0; float m_fAnimTime=0.f; };
struct IAnimationSet { int GetAnimIDByName(const char*) {return 1;} const char* GetNameByAnimID(int) {return "mock";} };
struct ISkeletonAnim { int GetNumAnimsInFIFO(int) {return 0;} CAnimation& GetAnimFromFIFO(int,int) {static CAnimation a;return a;} };
struct ICharacterInstance { int GetFlags(){return 0;} IAnimationSet* GetIAnimationSet() {return nullptr;} ISkeletonAnim* GetISkeletonAnim() {return nullptr;} const char* GetFilePath() {return "mock";} };
struct Entity { Vec3 p; Vec3 GetWorldPos()const{return p;} ICharacterInstance* GetCharacter(int){return nullptr;} };
struct IAnimationGraphState {
 using InputID=unsigned;const char* out=nullptr;const char* QueryOutput(const char*){return out;}
 const char* GetCurrentStateName(){return "Mock";}
 InputID GetInputId(const char*n){return std::strcmp(n,"Signal")==0?1u:2u;}
 const char* GetInputName(InputID n){return n==1?"Signal":"PseudoSpeed";}
 void GetInput(InputID,char*b){std::strcpy(b,"none");}float GetInputAsFloat(InputID){return .5f;}
};
struct IAnimatedCharacter { IAnimationGraphState* ag=nullptr; IAnimationGraphState* GetAnimationGraphState(){return ag;} };
struct CPlayer;
struct CActor { CPlayer* p=nullptr; };
struct CPlayer : CActor {
 bool NeedsWarsBotLadderPhysicsExit() const {return false;}
 void SetWarsBotLadderExitPhysics(bool,bool) {}
 void RestoreWarsBotLadderGraph(){}
 bool TryWarsBotLadderExitRetry(IAnimationGraphState*){return false;} // isolated route tests; real retry has a dedicated caller
 enum ELadderActionType { eLAT_None, eLAT_ReachedEnd, eLAT_ExitTop, eLAT_Jump, eLAT_StrafeLeft, eLAT_StrafeRight, eLAT_Use, eLAT_Die };
 Entity e; SPlayerStats st; IAnimatedCharacter ac; IAnimationGraphState ag;
 CPlayer(){p=this;}
 SPlayerStats* GetPlayerStats(){return &st;}
 Entity* GetEntity(){return &e;}
 IAnimatedCharacter* GetAnimatedCharacter(){return &ac;}
 EntityId GetEntityId()const{return 1;}
 bool IsFrozen()const{return st.isFrozen.Value();}
 bool IsClient()const{return false;}
 bool IsWarsBotActor()const{return warsBot;}
 bool warsBot=true;
 bool ladderUsable=false; int isLadderCalls=0;
 int grabs=0, leaves=0, lastLeave=0;
 bool IsLadderUsable(){++isLadderCalls; return ladderUsable;}
 void RequestGrabOnLadder(ELadderActionType){++grabs; st.isOnLadder=true;}
 void RequestLeaveLadder(ELadderActionType r){++leaves; lastLeave=r; st.isOnLadder=false; st.isExitingLadder=false;}
 void SetWarsBotActions(bool,bool,int){}
 float GetStanceMaxSpeed(int)const{return 2.5f;}
 float CalculatePseudoSpeed(bool)const{return 1.0f;} // native value separately extracted by exit-retry caller
 void* GetLinkedEntity(){return nullptr;}
 void* GetLinkedVehicle(){return nullptr;}
};
struct NavPathLink { int corner=0, kind=0; float a[3]={},b[3]={}; unsigned long long ref=0; };
enum NavLinkKind { NavLinkDrop=0, NavLinkJump, NavLinkStrength, NavLinkLift, NavLinkLadder };
inline const char* NavLinkName(int k){static const char* n[]={"drop","jump","strength","lift","ladder"};return n[k];}
struct NavMeshCore { int bans=0, notes=0; bool BanLink(unsigned long long){++bans;return true;} bool NoteLinkFailure(unsigned long long){++notes;return true;} };
namespace WarsBots { struct BotConfig { int ladder=1, jumpLinks=1, navKneeFix=0; static BotConfig& Get(){static BotConfig c;return c;} }; }
using WarsBots::BotConfig;
struct CGameRules { using TPlayers=std::vector<int>; };
struct MockNav { int audit=-1, resets=0; void StartAudit(int v){audit=v;} void Reset(){++resets;} };
struct MockRules { void GetPlayers(CGameRules::TPlayers&){} };
struct IConsoleCmdArgs;
struct MockManager;
struct MockGame { MockManager* mgr=nullptr; MockRules rules; MockManager& GetBotManager(); MockRules* GetGameRules(){return &rules;} };
struct MockManager {
 bool owns=true; int count=0; MockNav m_nav;
 bool Owns(EntityId)const{return owns;} bool CanRunLocalIA()const{return true;} int Count()const{return count;}
 void NavAuditCommand(IConsoleCmdArgs*);
 static MockManager* self; static MockManager* Self(){return self;}
};
using BotManager = MockManager;
MockManager* MockManager::self=nullptr;
MockManager& MockGame::GetBotManager(){return *mgr;}
struct MockEnv { bool bServer=true; bool bClient=false; } g_env;
MockEnv* gEnv=&g_env;
MockGame g_game; MockGame* g_pGame=&g_game;
static bool ParseInt(const char* s,int& v){return s && std::sscanf(s,"%d",&v)==1;}

// Extracted actual WarsBotLadderServer helper.
static bool WarsBotLadderServer(const CPlayer& player)
{
	return gEnv && gEnv->bServer && g_pGame && player.IsWarsBotActor() && WarsBots::BotConfig::Get().ladder == 1 &&
		g_pGame->GetBotManager().Owns(player.GetEntityId());
}

#if 1
using WarsBots::NativeLadderExitPhysicalReady;
#else
static bool NativeLadderExitPhysicalReady(const LVec&,const LVec&,const LVec&){return true;}
#endif

struct BotBlackboard { Vec3 position, velocity; bool grounded=true, canMove=true, suitActive=true; int mode=3; float energy=100; };
struct BotCombat { void StopFire(){} void Reset(){} };
struct BotIntent {
 bool move=false; Vec3 destination, aim; BotGait gait=BotGait::Run; BotJump jump=BotJump::None;
 int suitMode=NANOMODE_DEFENSE; BotCombatIntent combat=BotCombatIntent::None;
 bool linkJump=false, meshPath=false; float arrive=0.8f;
 struct Why { short suitMode=0,jump=0,gait=0,aim=0,move=0; } why;
};
#define WBI(field,value) (m_intent.field=(value), m_intent.why.field=(short)__LINE__)

struct BotMovement {
 int moves=0; float lastSpeed=0; bool lastJump=false;
 bool Move(CPlayer&,const Vec3&,const Vec3&,float,float speed,EStance,float,bool jumpTraverse,bool,float,bool,float,bool)
 { ++moves; lastSpeed=speed; lastJump=jumpTraverse; return true; }
 void Aim(CPlayer&,const Vec3&){} void Look(CPlayer&,const Vec3&){} void Stop(CPlayer&){}
};
struct MockLoco {
 BotMovement m_movement;
 void Look(CPlayer&,const Vec3&){} void Stop(CPlayer&){} bool Execute(CPlayer&,const BotIntent&,BotBlackboard&,float,float){return true;}
 bool ExecuteLadder(CPlayer& actor, const Vec3& moveTo, const Vec3& aim, float dt, bool nativeAttached)
{
	const SPlayerStats* stats=actor.GetPlayerStats();
	if (!nativeAttached || !stats || !stats->isOnLadder.Value() || stats->isExitingLadder) return false;
	// The SafeStep exception exists only while the native ladder is attached. Approach and post-exit settle call
	// the ordinary safe executor instead.
	// PlayerMovementController divides desired speed by stance speed. Supply normal full forward input, not 1 m/s
	// (which would become a ~0.25 m/s climb and exhaust the route deadline before reaching the 9.5 m shaft top).
	// DS-124: the native non-sprint input value is CPlayer::CalculatePseudoSpeed(false) == 1.0
	// (Player.cpp:2503-2515), not BotMovement's 0.3/0.5 fallback. Match this graph input to the native value;
	// the bot still uses a move target rather than human delta input. This does not prove the exit root cause.
	const bool accepted = m_movement.Move(actor, moveTo, aim, dt, actor.GetStanceMaxSpeed(STANCE_STAND), STANCE_STAND, 1.5f,
		nativeAttached, false, actor.CalculatePseudoSpeed(false), false, 0.5f, false);
	actor.SetWarsBotActions(false, accepted, STANCE_STAND);
	if (!accepted) m_movement.Aim(actor, aim);
	return accepted;
}
};
struct Trap { void Reset(){} };
enum class LinkMove { Drop=0, Jump, Strength };
struct BotInstance { CActor a; CActor* GetActor(){return &a;} };
struct IConsoleCmdArgs { virtual ~IConsoleCmdArgs(){} virtual int GetArgCount()const=0; virtual const char* GetArg(int)const=0; };
struct MockLink { bool active=false; int starts=0; LinkMove move=LinkMove::Drop; bool Active()const{return active;}
 void Reset(){active=false;} void Start(float,LinkMove m,JVec,JVec){++starts;move=m;active=true;} };
struct MockBrain {
 BotLadder m_ladder; NavPathLink m_ladderNow, m_linkNow; bool m_ladderNativeLocked=false, m_ladderCancelPending=false;
 std::vector<Vec3> m_path; std::vector<NavPathLink> m_pathLinks; size_t m_pathIndex=0, m_cornerFor=size_t(-1);
 float m_nextRepath=0,m_minRepath=0,m_now=0,m_number=1; bool m_meshPathActive=true,m_verbose=false,m_running=true;
 BotIntent m_intent; BotBlackboard m_blackboard; MockLoco m_locomotion; BotCombat m_combat; MockLink m_link;
 Trap m_trap; float m_trapStepUntil=0; NavMeshCore mesh; NavPathLink cornerLink;
 int cancelRunningSeen=-1; bool lastCancelBan=false; int cancels=0;
 NavMeshCore* CurrentNavMesh(){return &mesh;}
 const NavPathLink* LinkAtCorner(size_t c){return c==0?&cornerLink:nullptr;}
 void CancelLadderBody(CPlayer& actor, const char* why, bool ban)
{
	SPlayerStats* stats=actor.GetPlayerStats();
	const bool nativeAttached=stats && (stats->isOnLadder.Value() || stats->isExitingLadder);
	const bool hadRoute=m_ladder.Active() || m_ladderNativeLocked || m_ladderNow.ref || m_ladderCancelPending;
	m_ladderCancelPending=false; // also consume a reset whose native revive already cleared ladder stats
	actor.RestoreWarsBotLadderGraph();
	if (!hadRoute && !nativeAttached) return;
	WB_LOG(1,"[WarsBots] #%d LADDER cancel reason=%s nativeOn=%d exiting=%d pos=(%.2f,%.2f,%.2f) actor=%u.",m_number,
		why,nativeAttached?1:0,(stats&&stats->isExitingLadder)?1:0,
		actor.GetEntity()->GetWorldPos().x,actor.GetEntity()->GetWorldPos().y,actor.GetEntity()->GetWorldPos().z,
		actor.GetEntityId());
	m_combat.StopFire();
	m_intent=BotIntent();
	if (nativeAttached) actor.RequestLeaveLadder(CPlayer::eLAT_ReachedEnd); // cleanup only, never route success
	m_locomotion.Stop(actor);
	actor.SetWarsBotActions(false,false,STANCE_STAND);
	if (ban)
	{
		NavMeshCore* mesh=CurrentNavMesh();
		if (mesh && m_ladderNow.ref) mesh->BanLink(m_ladderNow.ref);
	}
	m_path.clear(); m_pathLinks.clear(); m_pathIndex=0; m_cornerFor=size_t(-1); m_nextRepath=m_minRepath=0; m_meshPathActive=false;
	m_ladder.Reset(); m_ladderNativeLocked=false; m_ladderNow=NavPathLink();
}
 void CancelLadder(CPlayer& a,const char* w,bool b){cancelRunningSeen=m_running?1:0;lastCancelBan=b;++cancels;CancelLadderBody(a,w,b);}
 void ReleaseClaims(){} // Stop releases the loot claim first (PR #17); covered by test_bot_loot_claim_caller.py
 void ProbeBlock(CPlayer& actor,LadderInput& in,const Vec3& center){
 SPlayerStats* stats=actor.GetPlayerStats();
 const LadderPhase phase=m_ladder.Phase();
if ((phase==LadderPhase::Approach || phase==LadderPhase::Grab) && !in.onLadder && !m_ladderNativeLocked)
	{
		m_locomotion.Look(actor,center);
		// Validate in the actual grab phase, immediately before the native call. Lock only after attachment.
		in.canGrab=phase==LadderPhase::Grab && actor.IsLadderUsable();
		if (in.canGrab)
		{
			if (!stats || !m_ladder.SetNative(
				LVec{stats->ladderBottom.x,stats->ladderBottom.y,stats->ladderBottom.z},
				LVec{stats->ladderTop.x,stats->ladderTop.y,stats->ladderTop.z},
				LVec{stats->ladderOrientation.x,stats->ladderOrientation.y,stats->ladderOrientation.z}))
				m_ladder.RejectNative();
		}
	}
}

 void GrabBlock(CPlayer& actor,const LadderOutput& o,LadderInput& in){
 SPlayerStats* stats=actor.GetPlayerStats();
if (o.grab)
	{
		actor.RequestGrabOnLadder(CPlayer::eLAT_Use);
		m_ladderNativeLocked=stats && stats->isOnLadder.Value();
		in.onLadder=m_ladderNativeLocked;
	}
}

 void ApplyLinkStart(CPlayer& actor)
{
	if (m_ladder.Active()) return; // DS-107: the native ladder owns this leg
	NavMeshCore* mesh=CurrentNavMesh();
	const Vec3 feet=m_blackboard.position;
	if (!mesh) { m_link.Reset(); return; }
	if (!m_link.Active())
	{
		if (!m_meshPathActive || !m_intent.move || m_intent.combat==BotCombatIntent::Fire) return;
		const NavPathLink* l=LinkAtCorner(m_pathIndex);
		if (!l) return;
		if (l->kind!=NavLinkDrop && l->kind!=NavLinkJump && l->kind!=NavLinkStrength) return;
		const Vec3 a(l->a[0],l->a[1],l->a[2]);
		if ((a-feet).GetLengthSquared2D()>4.0f || fabs_tpl(a.z-feet.z)>2.0f) return;
		m_linkNow=*l;
		const LinkMove move=l->kind==NavLinkStrength?LinkMove::Strength:(l->kind==NavLinkJump?LinkMove::Jump:LinkMove::Drop);
		m_link.Start(m_now,move,JVec{l->a[0],l->a[1],l->a[2]},JVec{l->b[0],l->b[1],l->b[2]});
		WB_LOG(2,"[WarsBots] #%d LINK start kind=%s a=(%.2f,%.2f,%.2f) b=(%.2f,%.2f,%.2f) energy=%.0f actor=%u.",m_number,NavLinkName(l->kind),
			l->a[0],l->a[1],l->a[2],l->b[0],l->b[1],l->b[2],m_blackboard.energy,actor.GetEntityId());
	}

}
 void StopPrefix(BotInstance& bot)
{
	ReleaseClaims(); // stop/removal: before the running check, a stopped bot must never keep an item (PR #17 review)
	if (!m_running) return;
	m_running = false; // Cancel respawn/re-entry before any native ladder, entity or weapon callback.
	m_trap.Reset(); m_trapStepUntil=0;
	// DS-107: one owned cancellation path clears native ladder mode as a failure (never route success).
	if (CActor* actor = bot.GetActor()) CancelLadder(static_cast<CPlayer&>(*actor), "stop", false);
	else { m_ladder.Reset(); m_ladderNativeLocked = false; }
	m_combat.Reset();
}
};
struct MockMovement { SPlayerStats& m_stats; CPlayer& m_player; MockMovement(CPlayer& p):m_stats(p.st),m_player(p){} void ProcessWarsBotLadderPhysicsExit(CPlayer&,bool& landed){landed=false;}
void ExitBlock(CPlayer& player){
if (m_stats.isExitingLadder)
	{
		const bool warsBot = WarsBotLadderServer(player);
		if (gEnv->bClient || warsBot)
		{
			IAnimatedCharacter* pAC = m_player.GetAnimatedCharacter();
			IAnimationGraphState* pAGState = pAC ? pAC->GetAnimationGraphState() : NULL;
			const char* agOutputOnLadder = pAGState ? pAGState->QueryOutput("OnLadder") : NULL;
#if defined(WARSBOTS_EXPERIMENTAL_SPAWNER)
			// DS-124: copy the scalar output and the state text BEFORE any graph write. SetInput may replace the
			// queried state and invalidate the pointers above; nothing below may read them after a mutation.
			const bool warsBotHasOutput = warsBot && agOutputOnLadder;
			const char warsBotOutput = warsBotHasOutput ? agOutputOnLadder[0] : '\0';
			char warsBotState[256] = {};
			if (warsBot && pAGState)
			{
				if (const char* currentState = pAGState->GetCurrentStateName())
					std::strncpy(warsBotState, currentState, sizeof(warsBotState)-1);
			}
			if (warsBot)
			{
				const bool exitHeld = std::strstr(warsBotState, "ladderOffTop") != NULL;
				char currentSignal[256] = {};
				if (pAGState)
				{
					const IAnimationGraphState::InputID signalId = pAGState->GetInputId("Signal");
					const char* signalName = pAGState->GetInputName(signalId);
					if (signalName && std::strcmp(signalName, "Signal")==0) pAGState->GetInput(signalId, currentSignal);
				}
				float graphPseudo=-1.0f;
				if (pAGState)
				{
					const auto id=pAGState->GetInputId("PseudoSpeed");
					const char* name=pAGState->GetInputName(id);
					if (name && std::strcmp(name,"PseudoSpeed")==0) graphPseudo=pAGState->GetInputAsFloat(id);
				}
				ICharacterInstance* character = player.GetEntity()->GetCharacter(0);

				// C154: an accepted Signal is not proof that the installed actor can select/play its clip.
				// Read the live inputs and FIFO without forcing a state, loading an animation or changing motion.
				char action[256]={}, stance[256]={}, item[256]={};
				if (pAGState)
				{
					const auto readInput=[&](const char* key,char* value) {
						const auto id=pAGState->GetInputId(key);
						const char* name=pAGState->GetInputName(id);
						if (name && std::strcmp(name,key)==0) pAGState->GetInput(id,value);
					};
					readInput("Action",action); readInput("Stance",stance); readInput("Item",item);
				}
				IAnimationSet* animationSet=character?character->GetIAnimationSet():nullptr;
				ISkeletonAnim* skeleton=character?character->GetISkeletonAnim():nullptr;
				const int exitClip=animationSet?animationSet->GetAnimIDByName("relaxed_ladderOffTop_nw_01"):-1;
				const int fifo=skeleton?skeleton->GetNumAnimsInFIFO(0):0;
				int fifoClip=-1; float fifoTime=-1.f;
				if (fifo>0)
				{
					const CAnimation& animation=skeleton->GetAnimFromFIFO(0,fifo-1);
					fifoClip=animation.m_LMG0.m_nAnimID[0]; fifoTime=animation.m_fAnimTime;
				}
				const char* fifoName=animationSet&&fifoClip>=0?animationSet->GetNameByAnimID(fifoClip):nullptr;
				WB_LOG(2,"[WarsBots] LADDER EXIT INPUTS actor=%u action=%s stance=%s item=%s animationSet=%d exitClip=%d fifo=%d fifoClip=%d fifoName=%s fifoTime=%.3f model=%s.",
					player.GetEntityId(),action,stance,item,animationSet?1:0,exitClip,fifo,fifoClip,fifoName?fifoName:"<none>",fifoTime,
					character?character->GetFilePath():"<none>");

				WB_LOG(2,"[WarsBots] LADDER EXIT actor=%u graph=%d output=%d active=%d state=%s held=%d signal=%s pseudo=%.2f upd=%d motionFlags=%u pos=(%.3f,%.3f,%.3f).",
					player.GetEntityId(),pAGState?1:0,warsBotHasOutput?int(static_cast<unsigned char>(warsBotOutput)):-1,
					pAGState&&pAGState->GetAnimationActivation()?1:0,
					warsBotState[0]?warsBotState:"<none>",exitHeld?1:0,
					currentSignal[0]?currentSignal:"<default>",
					graphPseudo,
					character&&(character->GetFlags()&CS_FLAG_UPDATE_ALWAYS)?1:0,
					pAC?unsigned(pAC->GetParams().flags):0u,player.GetEntity()->GetWorldPos().x,
					player.GetEntity()->GetWorldPos().y,player.GetEntity()->GetWorldPos().z);
			}
#endif
			// DS-107: an owned bot must not turn a missing graph/output into a premature ReachedEnd. Keep the native
			// exiting state and let the policy fail closed (BlockedTop) unless the graph gives a real completion.
			if (warsBot && (!pAC || !pAGState || !agOutputOnLadder)) return;
#if defined(WARSBOTS_EXPERIMENTAL_SPAWNER)
			if (warsBot)
			{
				// The stock graph has OnLadder=1 only in ladder states; completion may return its empty default.
				// Empty is never completion by itself: require the same real physical egress as literal 0.
				if (warsBotOutput && warsBotOutput!='0' && warsBotOutput!='1') return;
				if (player.NeedsWarsBotLadderPhysicsExit())
				{
					bool landed=false;
					ProcessWarsBotLadderPhysicsExit(player,landed);
					if (landed) player.RequestLeaveLadder(CPlayer::eLAT_ReachedEnd);
					return;
				}
				if (warsBotOutput!='1')
				{
					const Vec3 feet=player.GetEntity()->GetWorldPos();
					const bool physicalReady=WarsBots::NativeLadderExitPhysicalReady(
						WarsBots::LVec{feet.x,feet.y,feet.z},
						WarsBots::LVec{m_stats.ladderBottom.x,m_stats.ladderBottom.y,m_stats.ladderBottom.z},
						WarsBots::LVec{m_stats.ladderTop.x,m_stats.ladderTop.y,m_stats.ladderTop.z});
					// Completion is checked first and returns immediately: a real exit must never be re-signalled.
					if (physicalReady)
					{
						player.RequestLeaveLadder(CPlayer::eLAT_ReachedEnd);
						return;
					}
					// Not complete: at most one bounded recovery signal for this owned native exit. It does not
					// move, teleport or complete the actor; the policy still fails closed after the exit budget.
					player.TryWarsBotLadderExitRetry(pAGState);
					return;
				}
			}
#endif
			bool onLadder = agOutputOnLadder && agOutputOnLadder[0] == '1';
			if (!onLadder)
				player.RequestLeaveLadder(CPlayer::eLAT_ReachedEnd);
		}
		return;
	}
}
 };
struct MockArgs : IConsoleCmdArgs { std::vector<std::string> a; int GetArgCount()const{return (int)a.size();} const char* GetArg(int i)const{return a[i].c_str();} };
struct MockManager;

void MockManager::NavAuditCommand(IConsoleCmdArgs* args) {

 BotManager* self=Self();if(!self || !self->CanRunLocalIA() || !args)return;
 CGameRules::TPlayers players;g_pGame->GetGameRules()->GetPlayers(players);
 if(self->Count() || !players.empty()){WB_LOG(0,"[WarsBots] NAV AUDIT refused: no bots/players allowed");return;}
 if(args->GetArgCount()==2 && !std::strcmp(args->GetArg(1),"stop")){self->m_nav.Reset();return;}
 // C154 net-zero flags: retire the latched wb_nav_knee_fix switch; preserve its diagnostic modes per command.
 int variant=0;
 if(args->GetArgCount()==2) {
  if(!ParseInt(args->GetArg(1),variant) || variant<0 || variant>2) {WB_LOG(0,"[WarsBots] Usage: wb_nav_audit [0|1|2|stop]");return;}
 } else if(args->GetArgCount()!=1) {WB_LOG(0,"[WarsBots] Usage: wb_nav_audit [0|1|2|stop]");return;}
 self->m_nav.StartAudit(variant);

}


static int fails=0;
static void check(bool ok,const char* what){ if(!ok){std::printf("FAIL %s\n",what);++fails;} }
int main(){
 MockManager mgr; MockManager::self=&mgr; g_game.mgr=&mgr;
 CPlayer actor; actor.st.ladderBottom=Vec3(0,0,10); actor.st.ladderTop=Vec3(0,0,20); actor.st.ladderOrientation=Vec3(0,1,0);
 actor.e.p=Vec3(0,0,10);

 // 1. Grab-lock: probe at 1.7 m then walk to 1.1 m; new grabs once, old never grabs.
 {
  MockBrain b; b.cornerLink.a[0]=0;b.cornerLink.a[1]=0;b.cornerLink.a[2]=10;
  b.cornerLink.b[0]=0;b.cornerLink.b[1]=0;b.cornerLink.b[2]=20;b.cornerLink.kind=NavLinkLadder;
  b.m_ladder.Start(0,LVec{0,0,10},LVec{0,0,20});
  actor.ladderUsable=true; actor.grabs=0; actor.isLadderCalls=0;
  Vec3 center(0,0,11.2f);
  for(int tick=0;tick<3;++tick){
   LadderInput in; in.now=0; in.grounded=true; // actual UpdateLadder constructs fresh input every tick
   actor.e.p=Vec3(0,0,10); in.feet=LVec{0,0,10};
   if(tick==0) in.feet=LVec{0,1.7f,10}; if(tick==1||tick==2) in.feet=LVec{0,1.1f,10};
   in.onLadder=actor.st.isOnLadder.Value(); in.exiting=actor.st.isExitingLadder;
   b.ProbeBlock(actor,in,center);
   LadderOutput o=b.m_ladder.Update(in);
   b.GrabBlock(actor,o,in);
  }
  check(actor.grabs==1,"grab-lock: new must grab exactly once");
  check(b.m_ladderNativeLocked,"grab-lock: frame locked after native attach");
  const int calls=actor.isLadderCalls;
  actor.ladderUsable=false;
  LadderInput climb; climb.now=1; climb.grounded=true; climb.onLadder=true; climb.feet=LVec{0,1.1f,10};
  b.ProbeBlock(actor,climb,center);
  check(actor.isLadderCalls==calls,"grab-lock: no ray mutation while climbing");
 }

 // 2. Cancel pending is consumed once; old early return leaves it set.
 {
  MockBrain b; b.m_ladder.Reset(); b.m_ladderNativeLocked=false; b.m_ladderCancelPending=true;
  actor.st.isOnLadder=false; actor.st.isExitingLadder=false;
  b.CancelLadder(actor,"reset",false);
  check(!b.m_ladderCancelPending,"cancel: pending must be cleared once");
 }

 // 3. Fail bans once; stop-style cancel does not ban a valid link.
 {
  MockBrain b; b.m_ladder.Start(0,LVec{0,0,10},LVec{0,0,20}); b.m_ladderNow.ref=42;
  b.CancelLadder(actor,"fail",true);
  check(b.mesh.bans==1,"cancel: real failure bans the link once");
  MockBrain c; c.m_ladder.Start(0,LVec{0,0,10},LVec{0,0,20}); c.m_ladderNow.ref=42;
  c.CancelLadder(actor,"stop",false);
  check(c.mesh.bans==0,"cancel: stop must not ban a valid link");
  MockBrain d; d.m_ladder.Reset(); d.m_ladderNow.ref=42;
  d.CancelLadder(actor,"no_grab",true);
  check(d.mesh.bans==1,"cancel: failed inactive policy still bans its actual route");
  d.CancelLadder(actor,"no_grab",true);
  check(d.mesh.bans==1,"cancel: duplicate cleanup must not ban twice");
 }

 // 4. Native exit graph: new rejects empty/0-before-egress; human legacy still leaves.
 {
  CPlayer p; p.st.isExitingLadder=true; p.st.ladderBottom=Vec3(0,0,10); p.st.ladderTop=Vec3(0,0,20);
  p.ac.ag=&p.ag; p.e.p=Vec3(0,0,10);
  MockMovement m(p);
  p.ag.out=nullptr; m.ExitBlock(p); check(p.leaves==0,"exit: null output must not leave");
  p.ac.ag=nullptr; m.ExitBlock(p); check(p.leaves==0,"exit: missing graph must not leave");p.ac.ag=&p.ag;
  p.ag.out="invalid"; m.ExitBlock(p); check(p.leaves==0,"exit: invalid output must not leave");
  p.ag.out=""; m.ExitBlock(p); check(p.leaves==0,"exit: empty graph output must not leave");
  p.ag.out="0"; p.e.p=Vec3(0,0,10); m.ExitBlock(p); check(p.leaves==0,"exit: graph0 before physical egress must not leave");
  p.e.p=Vec3(0.6f,0.6f,19.5f); m.ExitBlock(p); check(p.leaves==1,"exit: graph0 after physical egress leaves");
  p.leaves=0;p.st.isExitingLadder=true;p.st.isOnLadder=true;p.ag.out="";m.ExitBlock(p);check(p.leaves==1,"exit: empty native default completes only after physical egress");
  p.leaves=0; p.warsBot=false; gEnv->bClient=true; p.st.isExitingLadder=true; p.ag.out="0"; p.e.p=Vec3(0,0,10); m.ExitBlock(p);
  check(p.leaves==1,"exit: human/client legacy behavior preserved"); gEnv->bClient=false;
 }

 // 5. ExecuteLadder: new requires native attach and sends stance speed; old bypasses without attach and uses 1.0.
 {
  MockLoco l; CPlayer p; p.st.isOnLadder=true; p.st.isExitingLadder=false;
  const bool ok=l.ExecuteLadder(p,Vec3(0,0,10),Vec3(0,0,11),0.1f,true);
  check(ok && l.m_movement.moves==1,"execute: attached new runs");
  check(l.m_movement.lastSpeed>1.0f,"execute: new uses stance/full forward speed");
  check(l.m_movement.lastJump,"execute: attached uses the SafeStep exception");
  p.st.isOnLadder=false; l.m_movement.moves=0;
  const bool ok2=l.ExecuteLadder(p,Vec3(0,0,10),Vec3(0,0,11),0.1f,true);
  check(!ok2 && l.m_movement.moves==0,"execute: new refuses non-attached bypass");
 }

 // 6. Policy settle: 1.5 m is not Done on new (0.8 m), old accepts it; reattach is Lost.
 {
  BotLadder L; L.Start(0,LVec{0,0,10},LVec{0,0,20}); L.SetNative(LVec{0,0,10},LVec{0,0,20},LVec{0,1,0});
  LadderInput in; in.now=1; in.grounded=true; in.feet=LVec{0,0,10}; L.Update(in);
  in.onLadder=true; in.feet=LVec{0,0,19.5f}; L.Update(in); in.exiting=true; L.Update(in);
  in.onLadder=false; in.exiting=false; in.grounded=true; in.feet=LVec{0,1.5f,19.5f};
  LadderOutput o=L.Update(in);
  check(o.phase!=LadderPhase::Done,"settle: 1.5 m is not a real arrival");
  BotLadder R; R.Start(0,LVec{0,0,10},LVec{0,0,20}); R.SetNative(LVec{0,0,10},LVec{0,0,20},LVec{0,1,0});
  LadderInput ri; ri.now=0; ri.grounded=true; ri.feet=LVec{0,0,10};
  R.Update(ri); ri.onLadder=true; ri.feet=LVec{0,0,19.5f}; ri.exiting=true; R.Update(ri);
  ri.onLadder=false; ri.exiting=false; ri.grounded=true; ri.feet=LVec{0,1.5f,19.5f}; R.Update(ri);
  ri.onLadder=true; o=R.Update(ri); check(o.phase==LadderPhase::Failed && o.fail==LadderFail::Lost,"settle: reattach is not Done");
 }

 // 7. Generic jump start rejects Ladder/Lift; Drop/Jump/Strength still start.
 {
  MockBrain b; b.m_intent.move=true; b.m_blackboard.position=Vec3(0,0,10);
  const int kinds[]={NavLinkDrop,NavLinkJump,NavLinkStrength,NavLinkLadder,NavLinkLift};
  b.cornerLink.a[0]=0;b.cornerLink.a[1]=0;b.cornerLink.a[2]=10;
  b.cornerLink.b[0]=0;b.cornerLink.b[1]=0;b.cornerLink.b[2]=20;
  for(int k:kinds){ b.cornerLink.kind=k; b.m_link.starts=0; b.m_link.active=false; b.ApplyLinkStart(actor);
   if(k==NavLinkLadder||k==NavLinkLift) check(b.m_link.starts==0,"link: unsupported kind must not start as Drop");
   else check(b.m_link.starts==1,"link: supported kind still starts"); }
 }

 // 8. Stop prefix sets m_running=false before native cleanup (new) and keeps valid ladder un-banned.
 {
  MockBrain b; b.m_running=true; b.m_ladder.Start(0,LVec{0,0,10},LVec{0,0,20}); b.m_ladderNow.ref=42;
  BotInstance bi; bi.a.p=&actor; b.StopPrefix(bi);
  check(b.cancelRunningSeen==0,"stop: new must clear running before native cleanup");
  check(b.lastCancelBan==false,"stop: must not globally ban a valid ladder");
 }

 // 9. Net-zero nav audit variants: default/0/1/2/stop and invalid/no-actor guards.
 {
  MockManager m; MockManager::self=&m; g_game.mgr=&m; m.count=0;
  MockArgs a0; a0.a={"wb_nav_audit"}; m.NavAuditCommand(&a0); check(m.m_nav.audit==0,"nav: default variant 0");
  MockArgs a1; a1.a={"wb_nav_audit","1"}; m.NavAuditCommand(&a1); check(m.m_nav.audit==1,"nav: variant 1");
  MockArgs a2; a2.a={"wb_nav_audit","2"}; m.NavAuditCommand(&a2); check(m.m_nav.audit==2,"nav: variant 2");
  MockArgs as; as.a={"wb_nav_audit","stop"}; m.NavAuditCommand(&as); check(m.m_nav.resets==1,"nav: stop resets");
  MockArgs bad; bad.a={"wb_nav_audit","3"}; m.m_nav.audit=-1; m.NavAuditCommand(&bad); check(m.m_nav.audit==-1,"nav: invalid ignored");
  MockArgs a1b; a1b.a={"wb_nav_audit","1"}; m.count=1; m.m_nav.audit=-1; m.NavAuditCommand(&a1b); check(m.m_nav.audit==-1,"nav: actors guard");
 }
 if(fails){std::printf("DS108 caller controls FAILED (%d)\n",fails);return 1;}
 std::printf("DS108 real caller controls PASS\n"); return 0;
}
