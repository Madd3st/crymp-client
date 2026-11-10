
#include <cmath>
#include <cstdio>
#include <algorithm>
#include "E:/WarsBots/Development/warsbots-psdest/Code/CryGame/Bots/BotLadder.h"
#define WB_LOG(...) ((void)0)
using std::min; using std::max;
float fabs_tpl(float v){return std::fabs(v);}
struct Vec3 {
 float x=0,y=0,z=0; Vec3(){} Vec3(float a,float b,float c):x(a),y(b),z(c){}
 Vec3 operator+(Vec3 b)const{return {x+b.x,y+b.y,z+b.z};}
 Vec3 operator-(Vec3 b)const{return {x-b.x,y-b.y,z-b.z};}
 Vec3 operator-()const{return {-x,-y,-z};}
 Vec3 operator*(float f)const{return {x*f,y*f,z*f};}
 Vec3 operator/(float f)const{return *this*(1.f/f);}
 float Dot(Vec3 b)const{return x*b.x+y*b.y+z*b.z;}
 float len()const{return std::sqrt(Dot(*this));}
 bool IsValid()const{return std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z);}
 void zero(){x=y=z=0;} Vec3 GetNormalized()const{return *this/len();} void Normalize(){*this=GetNormalized();}
};
struct pe_player_dimensions {Vec3 sizeCollider={.3f,.3f,.6f};float heightCollider=1.f,heightPivot=0;int bUseCapsule=1;};
struct IPhysicalEntity {bool dimensionsOK=true;pe_player_dimensions dims;int GetParams(pe_player_dimensions* p){*p=dims;return dimensionsOK;}};
namespace primitives {struct primitive{};struct cylinder:primitive {enum{type=5};Vec3 center,axis;float r,hh;};struct capsule:cylinder {enum{type=6};};}
struct geom_contact{};struct ray_hit{Vec3 pt,n;};
enum {ent_static=1,ent_terrain=2,ent_rigid=4,ent_sleeping_rigid=8,ent_living=16,rwi_stop_at_pierceable=32,geom_colltype0=1,geom_colltype_player=64,rwi_colltype_bit=8};
struct World {
 bool floor=true,edge=false,staticWall=false;float floorZ=9.6f,normal=1,overlap=0,sweep=0;int rays=0,overlaps=0,sweeps=0;bool skipOK=true,flagsOK=true;
 int RayWorldIntersection(Vec3 origin,Vec3,int mask,int flags,ray_hit* hit,int,IPhysicalEntity** skip,int count){
  ++rays;flagsOK&=((flags>>rwi_colltype_bit)&(geom_colltype0|geom_colltype_player))==(geom_colltype0|geom_colltype_player);skipOK&=count==1&&skip&&skip[0]&&!(mask&ent_living);if(!floor||(edge&&origin.x>1.3f))return 0;
  hit->pt={origin.x,origin.y,floorZ};hit->n={0,0,normal};return 1;
 }
 float PrimitiveWorldIntersection(int,primitives::primitive*,Vec3 delta,int mask,geom_contact**,int,int flags,void*,void*,int,IPhysicalEntity** skip,int count){
  flagsOK&=flags==(geom_colltype0|geom_colltype_player);skipOK&=count==1&&skip&&skip[0]&&(mask&ent_living);if(staticWall&&(flags&geom_colltype0))return delta.len()<.0001f?1.f:.02f;if(delta.len()<.0001f){++overlaps;return overlap;}++sweeps;return sweep;
 }
} world;
struct Env {bool bServer=true;World* pPhysicalWorld=&world;} env,*gEnv=&env;
struct Manager {bool owns=true;bool Owns(unsigned){return owns;}};
struct Game {Manager mgr;Manager& GetBotManager(){return mgr;}} game,*g_pGame=&game;
namespace WarsBots {struct BotConfig {int ladder=1;static BotConfig& Get(){static BotConfig c;return c;}};}
struct Animations {int clip=-1;int GetAnimIDByName(const char*){return clip;}};
struct Character {Animations a;bool registry=true;Animations* GetIAnimationSet(){return registry?&a:nullptr;}};
using ICharacterInstance=Character;using IAnimationSet=Animations;
struct Entity {Vec3 pos={0,0,8};IPhysicalEntity physics;Character ch;Character* GetCharacter(int){return &ch;}IPhysicalEntity* GetPhysics(){return &physics;}Vec3 GetWorldPos(){return pos;}};
enum {eColliderMode_Undefined=0,eColliderMode_Pushable=3,eColliderModeLayer_Game=1,eCMT_Fly=1,eCMT_Normal=2};
struct AC {int mode=-1,calls=0;int GetPhysicalColliderMode(){return mode;}void RequestPhysicalColliderMode(int value,int,const char*){mode=value;++calls;}};
struct Coherent{bool value=true;bool Value()const{return value;}};
struct Stats {Coherent isOnLadder;bool isExitingLadder=true,isRagDoll=false;float onGround=0;Vec3 ladderTop={0,0,10},ladderBottom={0,0,0},ladderOrientation={-1,0,0};};
struct CPlayer {
 bool bot=true,frozen=false,m_warsBotLadderExitPhysics=false,m_warsBotLadderExitSettling=false;int health=100;mutable Entity entity;Stats m_stats;Vec3 m_warsBotLadderExitTarget;bool m_warsBotLadderExitTargetValid=false;AC ac;AC* m_pAnimatedCharacter=&ac;
 bool IsWarsBotActor()const{return bot;}unsigned GetEntityId()const{return 7;}int GetHealth()const{return health;}bool IsFrozen(){return frozen;}
 Entity* GetEntity()const{return &entity;} AC* GetAnimatedCharacter(){return m_pAnimatedCharacter;} bool WarsBotLadderExitSettling()const{return m_warsBotLadderExitSettling;}
 CPlayer(){SetWarsBotLadderExitTarget(Vec3(1.1f,0,9.6f));}
 void CleanupPhysicsPrefix(){
	m_warsBotLadderExitTargetValid=false;
	SetWarsBotLadderExitPhysics(false,false); // release our collider layer even if the graph was replaced
	}
 void SetWarsBotLadderExitTarget(const Vec3& target)
{
	if (!gEnv || !gEnv->bServer || !g_pGame || !IsWarsBotActor() ||
		!g_pGame->GetBotManager().Owns(GetEntityId()) || WarsBots::BotConfig::Get().ladder!=1 ||
		!m_stats.isOnLadder.Value()) return;
	m_warsBotLadderExitTargetValid=false;
	const Vec3& top=m_stats.ladderTop;
	const float side=Vec3(target.x-top.x,target.y-top.y,0).len();
	if (!target.IsValid() || !top.IsValid() || side<0.6f || side>2.f || fabs_tpl(target.z-top.z)>1.2f) return;
	m_warsBotLadderExitTarget=target;
	m_warsBotLadderExitTargetValid=true;
}
 bool GetWarsBotLadderExitTarget(Vec3& target) const
{
	if (!m_warsBotLadderExitTargetValid) return false;
	target=m_warsBotLadderExitTarget;
	return true;
}
 bool NeedsWarsBotLadderPhysicsExit()
{
	if (!gEnv || !gEnv->bServer || !g_pGame || !IsWarsBotActor() ||
		!g_pGame->GetBotManager().Owns(GetEntityId()) || WarsBots::BotConfig::Get().ladder!=1 ||
		!m_stats.isOnLadder.Value() || !m_stats.isExitingLadder || GetHealth()<=0 || IsFrozen() || m_stats.isRagDoll)
		return false;
	ICharacterInstance* character=GetEntity()->GetCharacter(0);
	IAnimationSet* animations=character?character->GetIAnimationSet():nullptr;
	return character && (!animations || animations->GetAnimIDByName("relaxed_ladderOffTop_nw_01")<0);
}
 void SetWarsBotLadderExitPhysics(bool enable, bool settling)
{
	if (!enable && !m_warsBotLadderExitPhysics) { m_warsBotLadderExitSettling=false; return; }
	if (enable && !NeedsWarsBotLadderPhysicsExit()) return;
	const bool owned=gEnv && gEnv->bServer && g_pGame && IsWarsBotActor() &&
		g_pGame->GetBotManager().Owns(GetEntityId());
	m_warsBotLadderExitPhysics=enable && owned;
	m_warsBotLadderExitSettling=m_warsBotLadderExitPhysics && (m_warsBotLadderExitSettling || settling);
	if (owned && m_pAnimatedCharacter)
		m_pAnimatedCharacter->RequestPhysicalColliderMode(enable?eColliderMode_Pushable:eColliderMode_Undefined,
			eColliderModeLayer_Game,"WarsBot native-frame exit");
}
};
struct CPlayerMovement {
 Stats& m_stats;float m_frameTime=.03f;struct{int type=0;Vec3 velocity={100,100,100};}m_request;
 CPlayerMovement(CPlayer& p):m_stats(p.m_stats){}
 void ProcessWarsBotLadderPhysicsExit(CPlayer&,bool&);
};
void CPlayerMovement::ProcessWarsBotLadderPhysicsExit(CPlayer& player, bool& landed)
{
	landed=false;
	if (!player.NeedsWarsBotLadderPhysicsExit()) return;
	m_request.type=eCMT_Fly; m_request.velocity.zero(); // a refused step holds; never reuse old climb velocity
	IPhysicalEntity* physics=player.GetEntity()->GetPhysics();
	if (!physics || !gEnv->pPhysicalWorld || !std::isfinite(m_frameTime) || m_frameTime<=0.f) return;
	const Vec3 feet=player.GetEntity()->GetWorldPos();
	const Vec3 top=m_stats.ladderTop, bottom=m_stats.ladderBottom;
	Vec3 axis=top-bottom;
	if (!axis.IsValid() || axis.len()<4.f) return;
	axis.Normalize();
	if (!feet.IsValid() || !top.IsValid() || !bottom.IsValid() || axis.z<0.95f) return;
	if ((feet-bottom-axis*((feet-bottom).Dot(axis))).len()>2.5f || feet.z<top.z-3.f || feet.z>top.z+0.75f) return;
	pe_player_dimensions dimensions;
	if (!physics->GetParams(&dimensions) || !dimensions.sizeCollider.IsValid() ||
		!std::isfinite(dimensions.heightCollider) || !std::isfinite(dimensions.heightPivot)) return;
	if (dimensions.bUseCapsule!=0 && dimensions.bUseCapsule!=1) return;
	const float radius=max(dimensions.sizeCollider.x,dimensions.sizeCollider.y);
	if (radius<0.1f || radius>0.75f || dimensions.sizeCollider.z<0.f || dimensions.sizeCollider.z>1.5f ||
		dimensions.heightCollider-dimensions.heightPivot<0.1f || dimensions.heightCollider-dimensions.heightPivot>2.f) return;
	IPhysicalEntity* skip[]={physics};
	const int collisionFlags=geom_colltype0|geom_colltype_player; // documented any-of part flags: static geometry AND player blockers
	const int floorMask=ent_static|ent_terrain|ent_rigid|ent_sleeping_rigid;
	const int mask=floorMask|ent_living;
	Vec3 landing;
	if (!player.GetWarsBotLadderExitTarget(landing))
	{ WB_LOG(2,"[WarsBots] LADDER PHYSICAL blocked actor=%u reason=no_link_landing.",player.GetEntityId()); return; }
	const float landingSide=Vec3(landing.x-top.x,landing.y-top.y,0).len();
	if (!landing.IsValid() || landingSide<0.6f || landingSide>2.f || fabs_tpl(landing.z-top.z)>1.2f) return;
	float floorZ=0.f;
	// Require support across the landing footprint, never a single ray over a ledge.
	const Vec3 offsets[]={Vec3(0,0,0),Vec3(radius,0,0),Vec3(-radius,0,0),Vec3(0,radius,0),Vec3(0,-radius,0)};
	for (int i=0;i<5;++i)
	{
		ray_hit floor;
		const int hits=gEnv->pPhysicalWorld->RayWorldIntersection(landing+offsets[i]+Vec3(0,0,0.5f),Vec3(0,0,-2.f),floorMask,
			rwi_stop_at_pierceable|(collisionFlags<<rwi_colltype_bit),&floor,1,skip,1);
		if (!hits || !floor.n.IsValid() || floor.n.z<0.75f || !floor.pt.IsValid() || floor.pt.z<top.z-1.2f || floor.pt.z>top.z+0.25f || fabs_tpl(floor.pt.z-landing.z)>0.25f)
		{
			WB_LOG(2,"[WarsBots] LADDER PHYSICAL blocked actor=%u reason=floor sample=%d hits=%d floor=%.3f normal=%.3f top=%.3f target=(%.3f,%.3f).",
				player.GetEntityId(),i,hits,hits?floor.pt.z:-10000.f,hits?floor.n.z:0.f,top.z,landing.x,landing.y);
			return;
		}
		if (!i) floorZ=floor.pt.z;
		else if (fabs_tpl(floor.pt.z-floorZ)>0.12f)
		{ WB_LOG(2,"[WarsBots] LADDER PHYSICAL blocked actor=%u reason=uneven_floor sample=%d center=%.3f edge=%.3f.",player.GetEntityId(),i,floorZ,floor.pt.z); return; }
	}
	const bool egress=WarsBots::NativeLadderExitPhysicalReady(
		WarsBots::LVec{feet.x,feet.y,feet.z},WarsBots::LVec{bottom.x,bottom.y,bottom.z},WarsBots::LVec{top.x,top.y,top.z});
	if (egress && player.WarsBotLadderExitSettling() && player.GetAnimatedCharacter()
		&& player.GetAnimatedCharacter()->GetPhysicalColliderMode()==eColliderMode_Pushable && m_stats.onGround>0.01f && fabs_tpl(feet.z-floorZ)<0.12f
		&& Vec3(feet.x-landing.x,feet.y-landing.y,0).len()<0.15f)
	{
		ray_hit support;
		landed=gEnv->pPhysicalWorld->RayWorldIntersection(feet+Vec3(0,0,0.15f),Vec3(0,0,-0.3f),floorMask,
			rwi_stop_at_pierceable|(collisionFlags<<rwi_colltype_bit),&support,1,skip,1)>0 && support.n.z>=0.75f &&
			fabs_tpl(support.pt.z-floorZ)<0.08f;
		if (landed) return;
	}
	Vec3 delta;
	const char* phase;
	if (feet.z<floorZ+0.04f && (feet-bottom-axis*((feet-bottom).Dot(axis))).len()<0.35f)
	{
		phase="rise"; delta=axis*((floorZ+0.06f-feet.z)/axis.z);
	}
	else
	{
		phase="egress"; delta=Vec3(landing.x-feet.x,landing.y-feet.y,0.f);
	}
	const float dt=min(m_frameTime,0.05f), speed=1.7f;
	if (delta.len()>speed*dt) delta=delta.GetNormalized()*speed*dt;
	primitives::capsule body;
	body.axis=Vec3(0,0,1); body.r=radius; body.hh=dimensions.sizeCollider.z;
	body.center=feet+Vec3(0,0,dimensions.heightCollider-dimensions.heightPivot);
	geom_contact* contact=nullptr;
	const int type=dimensions.bUseCapsule?primitives::capsule::type:primitives::cylinder::type;
	const float overlap=gEnv->pPhysicalWorld->PrimitiveWorldIntersection(type,&body,Vec3(0,0,0),mask,&contact,
		0,collisionFlags,nullptr,nullptr,0,skip,1);
	if (!std::isfinite(overlap) || overlap>0.f)
	{ WB_LOG(2,"[WarsBots] LADDER PHYSICAL blocked actor=%u reason=overlap hits=%.3f radius=%.3f half=%.3f center=(%.3f,%.3f,%.3f) floor=%.3f phase=%s.",player.GetEntityId(),overlap,radius,body.hh,body.center.x,body.center.y,body.center.z,floorZ,phase); return; }
	if (delta.len()>0.001f)
	{
		const float hit=gEnv->pPhysicalWorld->PrimitiveWorldIntersection(type,&body,delta,mask,&contact,
			0,collisionFlags,nullptr,nullptr,0,skip,1);
		if (!std::isfinite(hit) || hit>0.f)
		{ WB_LOG(2,"[WarsBots] LADDER PHYSICAL blocked actor=%u reason=sweep hit=%.3f radius=%.3f half=%.3f center=(%.3f,%.3f,%.3f) delta=(%.3f,%.3f,%.3f) phase=%s.",player.GetEntityId(),hit,radius,body.hh,body.center.x,body.center.y,body.center.z,delta.x,delta.y,delta.z,phase); return; }
	}
	player.SetWarsBotLadderExitPhysics(true,false); // ordinary world-colliding mode; stock leave/cancel restores it
	if (!player.GetAnimatedCharacter() || player.GetAnimatedCharacter()->GetPhysicalColliderMode()!=eColliderMode_Pushable) return;
	m_request.velocity=delta/m_frameTime; // swept displacement matches the full real frame, including a slow frame
	if (phase[0]=='e' && delta.len()<0.04f)
	{ m_request.type=eCMT_Normal; player.SetWarsBotLadderExitPhysics(true,true); } // let gravity settle onto verified floor
	WB_LOG(2,"[WarsBots] LADDER PHYSICAL actor=%u phase=%s floor=%.3f target=(%.3f,%.3f) radius=%.3f half=%.3f velocity=(%.3f,%.3f,%.3f).",
		player.GetEntityId(),phase,floorZ,landing.x,landing.y,radius,body.hh,m_request.velocity.x,m_request.velocity.y,m_request.velocity.z);
}
void BindRouteLanding(CPlayer& actor,bool locked,bool attached,Vec3 goal){
 bool m_ladderNativeLocked=locked;struct{bool onLadder;}in{attached};struct{float b[3];}m_ladderNow{{goal.x,goal.y,goal.z}};
 if (m_ladderNativeLocked && in.onLadder)
	{
		actor.SetWarsBotLadderExitTarget(Vec3(m_ladderNow.b[0],m_ladderNow.b[1],m_ladderNow.b[2]));
	}
}
 int failures=0;
void check(bool v,const char* s){if(!v){std::printf("FAIL %s\n",s);++failures;}}
void reset(){world=World();env=Env();game=Game();g_pGame=&game;gEnv=&env;WarsBots::BotConfig::Get().ladder=1;}
int main(){
 {reset();CPlayer p;Vec3 goal;check(p.GetWarsBotLadderExitTarget(goal),"current bounded link landing stored");p.SetWarsBotLadderExitTarget(Vec3(NAN,0,9));check(!p.GetWarsBotLadderExitTarget(goal),"invalid goal invalidates old target");p.SetWarsBotLadderExitTarget(Vec3(0,0,9.6f));check(!p.GetWarsBotLadderExitTarget(goal),"shaft center is not an exit target");}

 {reset();CPlayer p;check(p.NeedsWarsBotLadderPhysicsExit(),"owned missing asset eligible");
  p.entity.ch.registry=false;check(p.NeedsWarsBotLadderPhysicsExit(),"missing registry eligible without dereference");p.entity.ch.registry=true;
  p.entity.ch.a.clip=12;check(!p.NeedsWarsBotLadderPhysicsExit(),"registered asset preserves native animation");p.entity.ch.a.clip=-1;
  p.bot=false;check(!p.NeedsWarsBotLadderPhysicsExit(),"human");p.bot=true;game.mgr.owns=false;check(!p.NeedsWarsBotLadderPhysicsExit(),"foreign");game.mgr.owns=true;
  env.bServer=false;check(!p.NeedsWarsBotLadderPhysicsExit(),"replica");env.bServer=true;WarsBots::BotConfig::Get().ladder=0;check(!p.NeedsWarsBotLadderPhysicsExit(),"OFF");WarsBots::BotConfig::Get().ladder=1;
  p.health=0;check(!p.NeedsWarsBotLadderPhysicsExit(),"dead");p.health=100;p.frozen=true;check(!p.NeedsWarsBotLadderPhysicsExit(),"frozen");p.frozen=false;
  p.m_stats.isRagDoll=true;check(!p.NeedsWarsBotLadderPhysicsExit(),"ragdoll");p.m_stats.isRagDoll=false;p.m_stats.isExitingLadder=false;check(!p.NeedsWarsBotLadderPhysicsExit(),"not exit");}
 {reset();CPlayer p;CPlayerMovement m(p);bool landed=true;m.ProcessWarsBotLadderPhysicsExit(p,landed);
  check(!landed&&m.m_request.type==eCMT_Fly&&m.m_request.velocity.z>0,"request real native-axis rise");check(world.rays==5&&world.overlaps==1&&world.sweeps==1&&world.skipOK&&world.flagsOK,"support footprint/fullbody checks and own body skipped");check(p.ac.mode==eColliderMode_Pushable&&p.m_warsBotLadderExitPhysics,"world collider acquired");
  p.CleanupPhysicsPrefix();check(!p.m_warsBotLadderExitTargetValid,"route cleanup invalidates landing before graph early-return");check(p.ac.mode==eColliderMode_Undefined&&!p.m_warsBotLadderExitPhysics,"cleanup releases owned collider layer");}
 {reset();CPlayer p;p.entity.ch.a.clip=1;CPlayerMovement m(p);bool landed=true;m.ProcessWarsBotLadderPhysicsExit(p,landed);check(!landed&&world.rays==0&&world.overlaps==0&&p.ac.calls==0&&m.m_request.velocity.x==100,"registered clip path untouched by fallback");}
 {reset();CPlayer p;p.m_warsBotLadderExitTargetValid=false;Vec3 goal;
  BindRouteLanding(p,false,true,Vec3(0,1.5f,9.6f));check(!p.GetWarsBotLadderExitTarget(goal),"unlocked route cannot provide landing");
  BindRouteLanding(p,true,false,Vec3(0,1.5f,9.6f));check(!p.GetWarsBotLadderExitTarget(goal),"detached actor cannot provide landing");
  BindRouteLanding(p,true,true,Vec3(0,1.5f,9.6f));check(p.GetWarsBotLadderExitTarget(goal)&&goal.x==0&&goal.y==1.5f,"actual brain binder forwards b, not invented native-forward offset");
  p.entity.pos={0,0,9.67f};CPlayerMovement m(p);bool landed=false;m.ProcessWarsBotLadderPhysicsExit(p,landed);check(m.m_request.velocity.y>0&&m.m_request.velocity.x==0,"executor follows real side landing even perpendicular to native facing");}
 for(int fault=0;fault<10;++fault){reset();CPlayer p;CPlayerMovement m(p);bool landed=true;
  if(fault==0)world.floor=false;if(fault==1)world.edge=true;if(fault==2)world.normal=.4f;if(fault==3)world.overlap=1;if(fault==4)world.sweep=.02f;if(fault==5)p.entity.physics.dimensionsOK=false;if(fault==6)m.m_frameTime=NAN;if(fault==7)world.staticWall=true;if(fault==8)p.m_warsBotLadderExitTargetValid=false;if(fault==9)p.SetWarsBotLadderExitTarget(Vec3(20,0,9.6f));
  m.ProcessWarsBotLadderPhysicsExit(p,landed);check(!landed&&m.m_request.velocity.len()==0&&!p.m_warsBotLadderExitPhysics,"blocked/missing floor/ledge/slope/overlap/wall/dimensions/dt refuse without success or motion");}
 {reset();CPlayer p;CPlayerMovement m(p);m.m_frameTime=.12f;bool landed=false;m.ProcessWarsBotLadderPhysicsExit(p,landed);check(m.m_request.velocity.len()*.12f<=1.7f*.05f+.0001f,"slow frame displacement stays inside swept interval");}
 {reset();CPlayer p;p.entity.pos={0,0,9.67f};CPlayerMovement m(p);bool landed=true;m.ProcessWarsBotLadderPhysicsExit(p,landed);check(!landed&&m.m_request.velocity.x>0&&m.m_request.velocity.z==0,"rise then native forward egress");}
 {reset();CPlayer p;p.entity.pos={1.1f,0,9.6f};CPlayerMovement m(p);bool landed=true;m.ProcessWarsBotLadderPhysicsExit(p,landed);check(!landed,"position alone never completes");p.m_stats.onGround=.2f;m.ProcessWarsBotLadderPhysicsExit(p,landed);check(landed,"real upper floor plus unchanged physical egress plus grounded");}
 {reset();CPlayer p;p.entity.pos={1.1f,0,9.6f};p.m_stats.onGround=.2f;world.floor=false;CPlayerMovement m(p);bool landed=true;m.ProcessWarsBotLadderPhysicsExit(p,landed);check(!landed,"stale grounded at missing floor never completes");}
 {reset();CPlayer p;p.entity.pos={1.1f,0,9.6f};p.m_stats.onGround=.2f;p.m_warsBotLadderExitSettling=true;p.ac.mode=0;CPlayerMovement m(p);bool landed=true;m.ProcessWarsBotLadderPhysicsExit(p,landed);check(!landed,"foreign collider override never completes from stale grounded");}
 std::printf("Actual physics exit caller: failures=%d\n",failures);return failures?1:0;
}
