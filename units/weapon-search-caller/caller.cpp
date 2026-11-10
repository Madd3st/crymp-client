
#include <cmath>
#include <cstdio>
#include <cstring>
using EntityId = unsigned;
struct Vec3 {
 float x=0,y=0,z=0;
 Vec3()=default; Vec3(float a,float b,float c):x(a),y(b),z(c){}
 Vec3 operator+(const Vec3& b)const{return Vec3(x+b.x,y+b.y,z+b.z);}
 Vec3 operator-(const Vec3& b)const{return Vec3(x-b.x,y-b.y,z-b.z);}
 float GetLengthSquared2D()const{return x*x+y*y;}
 float GetLength()const{return std::sqrt(x*x+y*y+z*z);}
};
float fabs_tpl(float x){return std::fabs(x);}
struct EntityClass { const char* name="SCAR"; const char* GetName(){return name;} };
struct IEntity { Vec3 pos; EntityClass cls; Vec3 GetWorldPos(){return pos;} EntityClass* GetClass(){return &cls;} };
struct Item { IEntity entity; IEntity* GetEntity(){return &entity;} };
struct Weapon {};
struct CPlayer {
 IEntity entity; Item item; bool usable=false; Weapon weapon;
 IEntity* GetEntity(){return &entity;} Item* GetCurrentItem(){return &item;}
};
struct World {
 IEntity item; EntityId candidate=77; bool exists=true,available=true,pickupSuccess=true,grantSuccess=true;
 int scans=0,pickups=0,grants=0; float pickupRange=0;
 void Reset(){*this=World();item.pos=Vec3(1,0,0);}
} world;
struct EntitySystem { IEntity* GetEntity(EntityId id){return id&&world.exists?&world.item:nullptr;} } entities;
struct Env {EntitySystem* pEntitySystem=&entities;} env; Env* gEnv=&env;
struct Config {int firstPrimary=0;};
struct BotConfig {static Config& Get(){static Config c;return c;}};
namespace BotWeapons {
 Weapon* Current(CPlayer& a){return &a.weapon;}
 bool HasAmmunition(CPlayer& a,Weapon&){return a.usable;}
 bool SelectAvailable(CPlayer& a){return a.usable;}
 EntityId FindNearby(CPlayer&,float,EntityClass*,unsigned,bool,void*,int,float,bool){++world.scans;return world.candidate;}
 bool AvailableForPickup(CPlayer&,EntityId){return world.available;}
 bool PickUp(CPlayer& a,EntityId,float range){++world.pickups;world.pickupRange=range;if(world.pickupSuccess)a.usable=true;return world.pickupSuccess;}
 bool GiveTestSCAR(CPlayer& a){++world.grants;if(world.grantSuccess)a.usable=true;return world.grantSuccess;}
}
#define WB_LOG(...) ((void)0)
#define WBI(field,value) (m_intent.field=(value))
#define WBI_MOVETO(goal,look) (m_intent.move=true)
struct Combat {void StopFire(){}};
struct BotBrain {
 enum class State{SPAWNING,SEEKING_WEAPON,MOVING_TO_WEAPON,SEARCHING_ENEMY};
 State m_state=State::SPAWNING;
 float m_now=0,m_weaponSearchStarted=0,m_nextSearch=0;
 bool m_fallbackGiven=false;
 EntityId m_weaponTarget=0; EntityClass* m_previousWeaponClass=nullptr; unsigned m_life=1;int m_number=1,executes=0;
 Combat m_combat; struct Intent{bool move=false;}m_intent;
 void Change(State s){m_state=s;} void SetUpgradeTarget(EntityId){}
 float PickupRange()const{return 20.f;} void ExecuteIntent(CPlayer&,float){++executes;}
 Vec3 Steer(const Vec3&,const Vec3& target){return target;} void TravelGait(float){}
 void ResetLife(){ m_weaponTarget = 0;
m_fallbackGiven = false;
m_weaponSearchStarted = m_now;
m_nextSearch = m_now; }
 bool UpdateWeaponSeek(CPlayer&,float,bool);
};
bool BotBrain::UpdateWeaponSeek(CPlayer& actor,float decisionTime,bool socomQA)
{
    if (socomQA && (!actor.GetCurrentItem() ||
        strcmp(actor.GetCurrentItem()->GetEntity()->GetClass()->GetName(),"SOCOM") ||
        !BotWeapons::Current(actor) || !BotWeapons::HasAmmunition(actor,*BotWeapons::Current(actor)))) {
        m_combat.StopFire();WBI(move,false);ExecuteIntent(actor,decisionTime);return true;
    }
	if (!BotWeapons::SelectAvailable(actor))
	{
		m_combat.StopFire();
		if (m_now >= m_nextSearch && !m_weaponTarget)
		{
			Change(State::SEEKING_WEAPON);
			// R4: each new bounded map search owns its budget, independent of life age.
			m_weaponSearchStarted = m_now;
			m_weaponTarget = BotWeapons::FindNearby(actor, 30.0f, m_previousWeaponClass, m_life, false, nullptr, 0, 20.f, BotConfig::Get().firstPrimary==1);
			m_nextSearch = m_now + 2.0f;
		}
		if (m_weaponTarget && !BotWeapons::AvailableForPickup(actor, m_weaponTarget))
		{
			WB_LOG(2, "[WarsBots] #%d weapon goal cancelled item=%u reason=unavailable.", m_number, m_weaponTarget);
			m_weaponTarget = 0;
		}
		IEntity* item = gEnv->pEntitySystem->GetEntity(m_weaponTarget);
		if (item && m_now - m_weaponSearchStarted < 6.0f)
		{
			Change(State::MOVING_TO_WEAPON);
			if ((item->GetWorldPos() - actor.GetEntity()->GetWorldPos()).GetLengthSquared2D() <= 4.0f && fabs_tpl(item->GetWorldPos().z - actor.GetEntity()->GetWorldPos().z) <= 2.2f)
			{
				WBI(move,false);
				if (BotWeapons::PickUp(actor, m_weaponTarget, PickupRange())) { m_previousWeaponClass = actor.GetCurrentItem()->GetEntity()->GetClass(); m_weaponTarget = 0; SetUpgradeTarget(0); Change(State::SEARCHING_ENEMY); }
				else m_weaponTarget = 0;
			}
			else { const Vec3 step=Steer(actor.GetEntity()->GetWorldPos(),item->GetWorldPos()); WBI_MOVETO(step,step+Vec3(0,0,1.2f)); TravelGait((item->GetWorldPos()-actor.GetEntity()->GetWorldPos()).GetLength()); }
		}
		else if (!m_fallbackGiven && (!item || m_now - m_weaponSearchStarted >= 6.0f))
		{
			WBI(move,false);
			m_fallbackGiven = true; // At most one test supply per life, never per frame.
			if (BotWeapons::GiveTestSCAR(actor)) { SetUpgradeTarget(0); Change(State::SEARCHING_ENEMY); }
		}
		else { m_weaponTarget = 0; WBI(move,false); Change(State::SEEKING_WEAPON); }
		ExecuteIntent(actor,decisionTime);
		return true;
	}


	// A usable weapon (including an external rearm) ends the previous pickup search.
	m_weaponTarget = 0;
    return false;
}
int invariantFails=0,episodeFails=0,lateFails=0;
void invariant(bool ok,const char* s){if(!ok){++invariantFails;std::printf("INVARIANT FAIL %s\n",s);}}
void episode(bool ok,const char* s,bool late=false){if(!ok){++episodeFails;if(late)++lateFails;std::printf("EPISODE FAIL %s\n",s);}}
void Tick(BotBrain& b,CPlayer& a,float t,bool qa=false){b.m_now=t;b.m_intent.move=false;b.UpdateWeaponSeek(a,.1f,qa);}
int main(int argc,char** argv){
 const bool old=argc>1&&!std::strcmp(argv[1],"old");
 // Long life with real map item must not invoke a test supply, even if it is still available.
 {world.Reset();CPlayer a;BotBrain b;b.ResetLife();a.usable=true;Tick(b,a,20);a.usable=false;Tick(b,a,30);
  episode(world.pickups==1&&world.grants==0&&a.usable,"late loss uses map pickup before fallback",true);
 }
 // The reported R4 case: fallback already used this life, later ammunition exhausted.
 {world.Reset();CPlayer a;BotBrain b;b.ResetLife();b.m_fallbackGiven=true;a.usable=true;Tick(b,a,20);a.usable=false;Tick(b,a,40);
  episode(world.pickups==1&&a.usable,"late loss after spent fallback still picks up",true);
  invariant(world.grants==0&&b.m_fallbackGiven,"search cannot reset lifetime fallback");
 }
 // Six-second budget belongs to an episode and cannot slide forward every tick or target refresh.
 {world.Reset();world.item.pos=Vec3(8,0,0);CPlayer a;BotBrain b;b.ResetLife();b.m_fallbackGiven=true;
  Tick(b,a,30);episode(b.m_intent.move,"new late episode pursues a distant map item",true);
  Tick(b,a,35.9f);episode(b.m_intent.move,"episode retains budget before six seconds");
  Tick(b,a,36.1f);invariant(!b.m_intent.move,"deadline bounds continuous pursuit");invariant(world.grants==0,"timeout cannot repeat fallback");
 }
 // After a spent-fallback timeout, the existing search cadence can acquire a new bounded attempt.
 {world.Reset();world.item.pos=Vec3(8,0,0);CPlayer a;BotBrain b;b.ResetLife();b.m_fallbackGiven=true;
  Tick(b,a,30);Tick(b,a,36.1f);world.item.pos=Vec3(1,0,0);Tick(b,a,36.2f);
  episode(world.pickups==1&&a.usable&&world.grants==0,"timeout after spent fallback does not permanently disable pickup",true);
 }
 // Existing six-second fallback decision and native pickup height/range guards stay intact.
 {world.Reset();world.item.pos=Vec3(8,0,0);CPlayer a;BotBrain b;b.ResetLife();Tick(b,a,.1f);Tick(b,a,6.2f);
  invariant(world.grants==1&&b.m_fallbackGiven,"existing fallback timeout remains six seconds");
 }
 {world.Reset();world.item.pos=Vec3(1,0,3);CPlayer a;BotBrain b;b.ResetLife();Tick(b,a,.1f);
  invariant(world.pickups==0&&b.m_intent.move,"native pickup height guard retained");
 }
 // External rearm closes stale target ownership. Later loss gets its own budget.
 {world.Reset();world.item.pos=Vec3(8,0,0);CPlayer a;BotBrain b;b.ResetLife();b.m_fallbackGiven=true;
  Tick(b,a,1);a.usable=true;Tick(b,a,10);episode(b.m_weaponTarget==0,"external rearm clears obsolete search target");
  a.usable=false;world.item.pos=Vec3(1,0,0);Tick(b,a,12);episode(world.pickups==1&&a.usable,"second episode starts after external rearm",true);
 }
 // Failed native pickup must not manufacture a usable weapon or grant again; next search stays throttled.
 {world.Reset();world.pickupSuccess=false;CPlayer a;BotBrain b;b.ResetLife();b.m_fallbackGiven=true;
  Tick(b,a,0.1f);invariant(world.pickups==1&&!a.usable&&b.m_weaponTarget==0,"failed pickup clears target without inventing ammo");
  const int scans=world.scans;world.candidate=0;Tick(b,a,.5f);invariant(world.scans==scans,"failed pickup retains scan throttle");
  invariant(world.grants==0,"failed pickup never replenishes lifetime grant");
 }
 // No-candidate grant attempt remains one per life, including a rejected supply; respawn permits one again.
 {world.Reset();world.candidate=0;world.grantSuccess=false;CPlayer a;BotBrain b;b.ResetLife();
  Tick(b,a,.1f);Tick(b,a,.5f);Tick(b,a,2.2f);invariant(world.grants==1&&b.m_fallbackGiven,"one supply attempt per life even on failure");
  b.m_now=100;b.ResetLife();Tick(b,a,100.1f);invariant(world.grants==2,"real ResetLife renews supply limit");
 }
 // SOCOM QA's existing hold must not run scans, map pickups or fallback.
 {world.Reset();CPlayer a;BotBrain b;b.ResetLife();a.item.entity.cls.name="SOCOM";a.usable=false;
  Tick(b,a,100,true);invariant(world.scans==0&&world.pickups==0&&world.grants==0&&!b.m_intent.move,"SOCOM QA exclusion");
 }
 // Delayed first decision after a reset still starts a fresh search, rather than a life-age timeout.
 {world.Reset();CPlayer a;BotBrain b;b.m_fallbackGiven=true;b.m_now=100;b.ResetLife();Tick(b,a,110);
  episode(world.pickups==1&&world.grants==0,"delayed first decision after reset can pick up",true);
 }
 // Per-actor episode clocks and fallback limits are independent.
 {world.Reset();world.item.pos=Vec3(8,0,0);CPlayer a,c;BotBrain b,d;b.ResetLife();d.ResetLife();b.m_fallbackGiven=d.m_fallbackGiven=true;
  Tick(b,a,10);Tick(d,c,15);Tick(b,a,16.1f);invariant(!b.m_intent.move,"first actor remains bounded");
  Tick(d,c,16.1f);episode(d.m_intent.move,"second actor has independent episode budget");
 }
 std::printf("invariant_failures=%d episode_failures=%d late_failures=%d\n",invariantFails,episodeFails,lateFails);
 if(invariantFails)return 2;
 if(old){if(lateFails<2)return 3;std::puts("R4 actual caller semantic RED reproduced");return 0;}
 if(episodeFails)return 1;
 std::puts("R4 actual caller GREEN PASS");return 0;
}
