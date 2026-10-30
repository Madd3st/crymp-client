"""R4: execute actual UpdateWeaponSeek and actual weapon-related ResetLife assignments.

Mocks provide engine boundaries only; no copied search/deadline decision logic. This does
not prove engine pickup or firing. --expect old requires known late-exhaustion failures,
while compiler errors always fail. Intended controls: episode timing, bounded pursuit,
one fallback attempt per life, throttle, rearm, QA exclusion, and actor isolation.
"""
from pathlib import Path
import argparse
import re
import subprocess


def method(text, signature):
    start = text.index(signature)
    left = text.index('{', start)
    depth = 0
    for i in range(left, len(text)):
        depth += (text[i] == '{') - (text[i] == '}')
        if not depth:
            return text[start:i + 1]
    raise ValueError('Unclosed method: ' + signature)


CPP = r"""
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
 void ResetLife(){ @RESET@ }
 bool UpdateWeaponSeek(CPlayer&,float,bool);
};
@ACTUAL@
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
"""


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--repo',type=Path,default=Path(__file__).resolve().parents[1])
    ap.add_argument('--revision')
    ap.add_argument('--out',type=Path)
    ap.add_argument('--expect',choices=['new','old'],default='new')
    a=ap.parse_args();root=a.repo.resolve();out=(a.out or root/'units/weapon-search-caller').resolve();out.mkdir(parents=True,exist_ok=True)
    if a.revision:
        source=subprocess.check_output(['git','-C',str(root),'show',a.revision+':Code/CryGame/Bots/BotBrain.cpp']).decode('utf-8-sig')
    else:source=(root/'Code/CryGame/Bots/BotBrain.cpp').read_text(encoding='utf-8-sig')
    resets=re.findall(r'\bm_(?:weaponTarget|fallbackGiven|weaponSearch\w*|nextSearch)\s*=\s*[^;]+;',method(source,'void BotBrain::ResetLife()'))
    assert len(resets)>=4,'Missing actual reset statements'
    cpp=CPP.replace('@ACTUAL@',method(source,'bool BotBrain::UpdateWeaponSeek(')).replace('@RESET@','\n'.join(resets))
    src=out/'caller.cpp';exe=out/'caller.exe';src.write_text(cpp,encoding='utf-8')
    comp=subprocess.run(['cl','/nologo','/std:c++17','/EHsc',str(src),'/Fo:'+str(out)+'/', '/Fe:'+str(exe)],capture_output=True)
    (out/'compile.log').write_bytes(comp.stdout+comp.stderr)
    if comp.returncode:raise RuntimeError('Compiler failure is not semantic RED: '+(comp.stdout+comp.stderr).decode(errors='replace'))
    run=subprocess.run([str(exe),a.expect],capture_output=True);(out/'run.log').write_bytes(run.stdout+run.stderr)
    print(run.stdout.decode(errors='replace'));return run.returncode


if __name__=='__main__':raise SystemExit(main())
