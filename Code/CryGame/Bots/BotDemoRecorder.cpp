#include "PortIncludes.h"
#include "BotDemoRecorder.h"
#include "BotDemoCodec.h"
#include "BotDataPath.h"
#include "BotManager.h"
#include "BotConfig.h"
#include "BotLog.h"
#include "CryGame/Game.h"
#include "CryGame/GameRules.h"
#include "CryGame/Actors/Player/Player.h"
#include "CryGame/Actors/Player/NanoSuit.h"
#include "CryCommon/CryAction/IGameplayRecorder.h"
#include "CryCommon/CryAction/IItemSystem.h"
#include "CryCommon/CrySystem/ILog.h"
#include "nlohmann/json.hpp"
#include "version.h"
#include <map>
#include <array>
#include <unordered_map>
#include <algorithm>
#include <filesystem>
#include <ctime>
#include <cstdio>
namespace WarsBots {
namespace {
using DemoCodec::Bytes;
void Vector(Bytes& b,const Vec3& v){b.F32(v.x);b.F32(v.y);b.F32(v.z);}
uint32_t Now(){return uint32_t(std::max(0.0,double(gEnv->pTimer->GetCurrTime())*1000.0));}
float Seconds(){return gEnv->pTimer->GetCurrTime();}
uint16_t Count(int value){return uint16_t(std::clamp(value,0,65535));}
CPlayer* Player(IGameFramework* f,EntityId id){return CPlayer::FromIActor(f->GetIActorSystem()->GetActor(id));}
}
struct BotDemoRecorder::Impl final:IGameplayListener {
 struct Seen {
  bool bot=false,viewValid=false,movementValid=false,grounded=false,jumped=false,alive=false;
  int16_t yaw=0;uint16_t peakYaw=0;float peakZ=0,lastLand=-100;
  uint64_t visited=0;unsigned decision=~0u;std::string state;std::vector<uint8_t> inventory;
 };
 IGameFramework* framework;BotManager* manager;FILE* file=nullptr;
 std::filesystem::path path;Bytes buffer;std::map<std::string,uint16_t> strings;
 std::unordered_map<EntityId,Seen> seen;uint64_t serial=0,written=0,frames=0,fileSerial=0;
 std::array<uint32_t,18001> aimHistogram{};uint32_t aimSamples=0;
 bool failed=false;int hz=20;float nextFrame=0,lastFlush=0,nextProfile=0;
 double totalMs=0,worstMs=0,eventMs=0;
 Impl(IGameFramework* f,BotManager* m):framework(f),manager(m){}
 ~Impl(){Close();}
 template<class F>void Safe(F action){
  if(!file)return;const auto start=gEnv->pTimer->GetAsyncTime();
  try{action();}catch(const std::exception& e){failed=true;WB_LOG(0,"[WarsBots] DEMO FAILED %s",e.what());Close();}
  eventMs+=(gEnv->pTimer->GetAsyncTime()-start).GetMilliSeconds();
 }
 void Flush(){
  if(buffer.data.empty())return;
  const size_t n=buffer.data.size();const bool ok=fwrite(buffer.data.data(),1,n,file)==n;
  written+=n;buffer.data.clear();
  if(!ok || fflush(file)!=0)throw std::runtime_error("demo flush failed");
 }
 void Close(){
  if(file){
   bool ok=true;try{Flush();}catch(...){ok=false;}
   ok=fclose(file)==0 && ok;file=nullptr;
   WB_LOG(0,"[WarsBots] DEMO STOP path=%s bytes=%llu complete=%d",path.u8string().c_str(),written,ok?1:0);
  }
  buffer.data.clear();strings.clear();seen.clear();
 }
 void Record(uint8_t type,const Bytes& payload){
  if(written+buffer.data.size()+payload.data.size()+3>512u*1024u*1024u)throw std::runtime_error("512MB demo safety cap");
  buffer.Record(type,payload);
 }
 uint16_t String(std::string text){
  if(text.empty())return 0;
  if(text.size()>255){size_t cut=255;while(cut>0 && (uint8_t(text[cut])&0xc0)==0x80)--cut;text.resize(cut);}
  if(auto it=strings.find(text);it!=strings.end())return it->second;
  if(strings.size()>=65534)throw std::runtime_error("demo string table full");
  const uint16_t id=uint16_t(strings.size()+1);strings.emplace(text,id);
  Bytes b;b.U16(id);b.U8(uint8_t(text.size()));b.Raw(text.data(),text.size());Record(1,b);return id;
 }
 Seen* Ensure(EntityId id){
  if(auto it=seen.find(id);it!=seen.end())return &it->second;
  CPlayer* actor=Player(framework,id);if(!actor)return nullptr;
  BotInstance* bot=manager->Find(id);if(!bot && !actor->GetChannelId())return nullptr;
  if(seen.size()>=64)throw std::runtime_error("demo actor limit64");
  const uint16_t name=String(actor->GetEntity()->GetName());
  std::string profile=bot?std::string(BotProfile::SkillName(bot->Profile().skill))+"/"+BotProfile::StyleName(bot->Profile().style):"human";
  const uint16_t prof=String(profile);
  Bytes b;b.U32(Now());b.U32(id);b.U8(bot?1:0);b.U16(name);b.U16(prof);b.U8(1);Record(2,b);
  Seen& s=seen[id];s.bot=bot!=nullptr;return &s;
 }
 void Event(uint8_t kind,EntityId actor,EntityId other,float value,const Vec3& pos){
  if(actor)Ensure(actor);if(other && framework->GetIActorSystem()->GetActor(other))Ensure(other);
  Bytes b;b.U32(Now());b.U8(kind);b.U32(actor);b.U32(other);b.F32(value);Vector(b,pos);Record(5,b);
 }
 Vec3 Position(EntityId id){auto* e=gEnv->pEntitySystem->GetEntity(id);return e?e->GetWorldPos():Vec3(0,0,0);}
 void Land(EntityId id,float height){
  Seen* s=Ensure(id);if(!s || Seconds()-s->lastLand<.25f)return;
  s->lastLand=Seconds();Event(9,id,0,std::max(0.f,height),Position(id));
 }
 bool Open(const char* label,int rate){
  if(!manager->CanRunLocalIA() || !gEnv->bServer)return false;
  std::string tag=label?label:"";if(tag.size()>32 || tag.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=std::string::npos)throw std::runtime_error("demo label must be<=32 letters/digits/_/-");
  auto* level=framework->GetILevelSystem()->GetCurrentLevel();if(!level)return false;
  Close();failed=false;hz=std::clamp(rate,10,60);
  const std::string map=level->GetLevelInfo()->GetName();std::string key=map;
  for(char& c:key)if(c=='/' || c=='\\')c='_';
  std::time_t time=std::time(nullptr);std::tm local{};localtime_s(&local,&time);char stamp[32],iso[32];
  std::strftime(stamp,sizeof(stamp),"%Y%m%d-%H%M%S",&local);std::strftime(iso,sizeof(iso),"%Y-%m-%dT%H:%M:%S",&local);
  do{path=DataDirectory(L"Demos")/(key+"-"+stamp+(tag.empty()?"":"-"+tag)+"-"+std::to_string(++fileSerial)+".wbdemo");}while(std::filesystem::exists(path));
  file=_wfopen(path.c_str(),L"wb");if(!file)throw std::runtime_error("cannot create demo");setvbuf(file,nullptr,_IOFBF,65536);
  nlohmann::json flags=nlohmann::json::object();
  const BotConfig& config=BotConfig::Get();
  flags["wb_bot_pace"]=config.botPace;
  flags["wb_bot_look"]=config.botLook;
  flags["wb_bot_turn_cap"]=config.botTurnCap;
  flags["wb_bot_weapon_rank"]=config.weaponRank;
  flags["wb_bot_arbiter"]=config.goalArbiter;
  flags["wb_bot_navmesh"]=config.botNavmesh;
  flags["wb_bot_perception"]=config.botPerception;
  flags["wb_bot_aim_model"]=config.aimModel;
  flags["wb_spawn_exclude195"]=config.spawnExclude195;
  flags["wb_revive_guard"]=config.reviveGuard;
  flags["wb_trap_recovery"]=config.trapRecovery;
  flags["wb_reachable_goals"]=config.reachableGoals;
  flags["wb_move_trace"]=config.moveTrace;
  flags["wb_qa_socom_only"]=config.qaSocomOnly;
  flags["wb_demo_record"]=1;flags["wb_demo_hz"]=hz;
  std::string server=gEnv->pLog?std::filesystem::path(gEnv->pLog->GetFileName()).stem().string():"local";
  const std::string header=nlohmann::json({{"map",map},{"build",CRYMP_GIT_SHA},{"started",iso},{"hz",hz},{"server",server},{"navmesh","none"},{"flags",flags}}).dump();
  buffer.Raw("WBDM",4);buffer.U16(2);buffer.U32(uint32_t(header.size()));buffer.Raw(header.data(),header.size());
  aimHistogram.fill(0);aimSamples=0;
  written=frames=serial=0;totalMs=worstMs=eventMs=0;nextFrame=lastFlush=Seconds();nextProfile=Seconds()+10;
  WB_LOG(0,"[WarsBots] DEMO START path=%s version=2 hz=%d",path.u8string().c_str(),hz);return true;
 }
 void Inventory(CPlayer& actor,Seen& s){
  Bytes items;uint8_t n=0;std::vector<EntityId> ids;
  if(IItem* held=actor.GetCurrentItem())ids.push_back(held->GetEntityId());
  if(IInventory* inv=actor.GetInventory())for(int i=0;i<inv->GetCount();++i){EntityId id=inv->GetItem(i);if(std::find(ids.begin(),ids.end(),id)==ids.end())ids.push_back(id);}
  for(EntityId id:ids){
   IItem* item=framework->GetIItemSystem()->GetItem(id);IWeapon* weapon=item?item->GetIWeapon():nullptr;if(!weapon)continue;
   const char* cls=item->GetEntity()->GetClass()->GetName();if(!std::strcmp(cls,"OffHand") || !std::strcmp(cls,"Fists"))continue;
   IFireMode* mode=weapon->GetFireMode(weapon->GetCurrentFireMode());IEntityClass* ammo=mode?mode->GetAmmoType():nullptr;
   items.U16(String(cls));items.U16(Count(ammo?weapon->GetAmmoCount(ammo):0));items.U16(Count(ammo && actor.GetInventory()?actor.GetInventory()->GetAmmoCount(ammo):0));
   if(++n==4)break;
  }
  std::vector<uint8_t> fingerprint{n};fingerprint.insert(fingerprint.end(),items.data.begin(),items.data.end());
  if(fingerprint==s.inventory)return;s.inventory=std::move(fingerprint);
  Bytes b;b.U32(Now());b.U32(actor.GetEntityId());b.U8(n);if(!items.data.empty())b.Raw(items.data.data(),items.data.size());Record(7,b);
 }
 void Brain(CPlayer& actor,Seen& s){
  BotInstance* bot=manager->Find(actor.GetEntityId());if(!bot)return;
  const auto& brain=bot->Brain();const std::string state=brain.StateName();
  if(s.decision==brain.DecisionSerial() && s.state==state)return;
  s.decision=brain.DecisionSerial();s.state=state;
  const auto& intent=brain.Intent();const auto& combat=brain.Combat();
  uint8_t bits=0;
  if(combat.Visible()){
   const Vec3 delta=(combat.LastPosition()-actor.GetEntity()->GetWorldPos()).GetNormalizedSafe(FORWARD_DIRECTION);
   if(actor.GetViewRotation().GetColumn1().Dot(delta)>=.573576f)bits|=1;
   if(auto* target=Player(framework,combat.Target()))if(target->GetNanoSuit() && target->GetNanoSuit()->GetMode()==NANOMODE_CLOAK)bits|=2;
  }
  if(combat.DamageMemory(Seconds()))bits|=4;
  const uint16_t reason=String(brain.DecisionReason()),stateId=String(state);
  Bytes b;b.U32(Now());b.U32(actor.GetEntityId());b.U16(stateId);b.U32(combat.Target());b.U8(combat.Visible()?1:0);
  // Raw observed target point, before BotAimControl filtering/sine command noise.
  const Vec3 rawAim=combat.Target()?combat.LastAim():intent.aim;
  Vector(b,rawAim);Vector(b,intent.destination);Vector(b,brain.NextCorner());b.U16(reason);b.U8(uint8_t(std::clamp(combat.VisibleEnemies(),0,255)));b.U8(bits);b.U32(combat.LastAttacker());Record(4,b);
  if(combat.Target() && actor.GetHealth()>0){
   SMovementState movement;actor.GetMovementController()->GetMovementState(movement);
   const Vec3 delta=rawAim-movement.eyePosition;
   if(delta.IsValid() && delta.len2()>1e-6f){
    const Vec3 view=actor.GetViewRotation().GetColumn1().GetNormalizedSafe(FORWARD_DIRECTION);
    const float angle=RAD2DEG(std::acos(std::clamp(view.Dot(delta.GetNormalized()),-1.f,1.f)));
    ++aimHistogram[size_t(std::clamp(int(std::lround(angle*100)),0,18000))];++aimSamples;
   }
  }
  Vec3 direction=intent.move?(intent.destination-actor.GetEntity()->GetWorldPos()):Vec3(0,0,0);direction.z=0;direction.NormalizeSafe(Vec3(0,0,0));
  Bytes in;in.U32(Now());in.U32(actor.GetEntityId());in.F32(direction.x);in.F32(direction.y);in.U8(uint8_t(intent.gait));in.U8(uint8_t(intent.jump));in.U8(uint8_t(intent.suitMode));in.U8(uint8_t(intent.combat));in.U32(combat.Target());in.U32(0);in.U16(reason);Record(6,in);
 }
 void Sample(CPlayer& actor,Seen& s,bool due,Bytes& states,uint8_t& count){
  s.visited=serial;const Vec3 pos=actor.GetEntity()->GetWorldPos();const auto& stats=*static_cast<const SPlayerStats*>(actor.GetActorStats());
  const bool alive=actor.GetHealth()>0,ground=stats.onGround>0 && stats.inAir<=.01f;
  const Vec3 view=actor.GetViewRotation().GetColumn1();const int16_t yaw=DemoCodec::Yaw(RAD2DEG(std::atan2(view.x,view.y)));
  if(s.viewValid)s.peakYaw=std::max(s.peakYaw,DemoCodec::YawDelta(yaw,s.yaw));s.yaw=yaw;s.viewValid=true;
  if(!alive || !s.movementValid || !s.alive){s.peakZ=pos.z;s.grounded=ground;s.jumped=stats.jumped;s.movementValid=alive;}
  else{
   s.peakZ=std::max(s.peakZ,pos.z);
   if(!s.bot && stats.jumped && !s.jumped)Event(8,actor.GetEntityId(),0,actor.GetNanoSuit() && actor.GetNanoSuit()->GetMode()==NANOMODE_STRENGTH?1.f:0.f,pos);
   if(!s.grounded && ground){Land(actor.GetEntityId(),s.peakZ-pos.z);s.peakZ=pos.z;}
   if(s.grounded && !ground)s.peakZ=pos.z;
   s.grounded=ground;s.jumped=stats.jumped;
  }
  s.alive=alive;
  if(s.bot)Brain(actor,s);
  if(!due)return;
  Inventory(actor,s);
  DemoCodec::ActorState row;row.id=actor.GetEntityId();row.x=pos.x;row.y=pos.y;row.z=pos.z;row.yaw=yaw;
  row.pitch=int16_t(std::clamp(int(std::lround(RAD2DEG(std::atan2(view.z,std::sqrt(view.x*view.x+view.y*view.y)))*100)), -9000,9000));
  row.stance=!alive || actor.GetSpectatorMode()!=CActor::eASM_None?4:actor.GetStance()==STANCE_CROUCH?1:actor.GetStance()==STANCE_PRONE?2:actor.GetStance()==STANCE_SWIM?3:0;
  CNanoSuit* suit=actor.GetNanoSuit();row.mode=suit?uint8_t(suit->GetMode()):3;row.energy=uint8_t(std::clamp(suit?int(suit->GetSuitEnergy()):0,0,200));row.health=uint8_t(std::clamp(actor.GetHealth(),0,255));
  IItem* item=actor.GetCurrentItem();IWeapon* weapon=item?item->GetIWeapon():nullptr;IFireMode* mode=weapon?weapon->GetFireMode(weapon->GetCurrentFireMode()):nullptr;
  row.flags=(alive?1:0)|(s.bot && actor.IsSprinting()?2:0)|(ground?4:0)|(mode && mode->IsFiring()?8:0)|(weapon && weapon->IsZoomed()?16:0)|(weapon && weapon->IsReloading()?32:0);
  row.weapon=item?String(item->GetEntity()->GetClass()->GetName()):0;row.weaponId=item?item->GetEntityId():0;row.peakYaw=s.peakYaw;s.peakYaw=0;row.Write(states);++count;
 }
 void Update(int rate){
  const auto started=gEnv->pTimer->GetAsyncTime();const float now=Seconds();hz=std::clamp(rate,10,60);
  bool due=now>=nextFrame;if(due)nextFrame+=(std::floor((now-nextFrame)*hz)+1.f)/hz;
  ++serial;Bytes states;uint8_t count=0;
  IActorIteratorPtr actors=framework->GetIActorSystem()->CreateActorIterator();
  while(IActor* a=actors->Next())if(CPlayer* actor=CPlayer::FromIActor(a))if(Seen* s=Ensure(actor->GetEntityId()))Sample(*actor,*s,due,states,count);
  for(auto it=seen.begin();it!=seen.end();)if(it->second.visited!=serial){Bytes b;b.U32(Now());b.U32(it->first);b.U8(it->second.bot?1:0);b.U16(0);b.U16(0);b.U8(0);Record(2,b);it=seen.erase(it);}else ++it;
  if(due){Bytes b;b.U32(Now());b.U8(count);if(!states.data.empty())b.Raw(states.data.data(),states.data.size());Record(3,b);}
  if(now-lastFlush>=1.f){Flush();lastFlush=now;}
  const double cost=(gEnv->pTimer->GetAsyncTime()-started).GetMilliSeconds()+eventMs;eventMs=0;totalMs+=cost;worstMs=std::max(worstMs,cost);++frames;
  if(now>=nextProfile){WB_LOG(1,"[WarsBots] DEMO PROFILE avg=%.4fms/frame worst=%.4fms frames=%llu hz=%d bytes=%llu",totalMs/frames,worstMs,frames,hz,written+buffer.data.size());if(aimSamples){
    const auto percentile=[&](double q){uint32_t sum=0,target=uint32_t(std::ceil(aimSamples*q));for(size_t i=0;i<aimHistogram.size();++i){sum+=aimHistogram[i];if(sum>=target)return float(i)*.01f;}return 0.f;};
    WB_LOG(1,"[WarsBots] DEMO AIM ERROR samples=%u median=%.3fdeg p95=%.3fdeg reference=native_eye_to_cached_target_3d",aimSamples,percentile(.5),percentile(.95));
   }nextProfile=now+10;}
 }
 void OnGameplayEvent(IEntity* entity,const GameplayEvent& event)override{
  Safe([&]{if(!entity)return;EntityId id=entity->GetId();const EntityId extra=EntityId(reinterpret_cast<uintptr_t>(event.extra));
   if(event.event==eGE_WeaponShot)Event(1,id,0,0,entity->GetWorldPos());
   else if(event.event==eGE_ItemPickedUp || event.event==eGE_ItemDropped){
    IEntity* item=gEnv->pEntitySystem->GetEntity(extra);const float cls=item?float(String(item->GetClass()->GetName())):0;
    Event(event.event==eGE_ItemPickedUp?5:6,id,extra,cls,entity->GetWorldPos());
   }else if(event.event==eGE_SuitModeChanged)Event(7,id,0,event.value,entity->GetWorldPos());
   else if(event.event==eGE_Revive && !manager->Find(id)){
    EntityId spawn=0;float best=4.f;
    if(auto* rules=g_pGame->GetGameRules())for(int i=0;i<rules->GetSpawnLocationCount();++i)if(auto* p=gEnv->pEntitySystem->GetEntity(rules->GetSpawnLocation(i))){float d=(p->GetWorldPos()-entity->GetWorldPos()).len2();if(d<best){best=d;spawn=p->GetId();}}
    if(Seen* s=Ensure(id))s->movementValid=false;Event(4,id,0,float(spawn),entity->GetWorldPos());
   }
  });
 }
};
BotDemoRecorder::BotDemoRecorder()=default;
BotDemoRecorder::~BotDemoRecorder(){Shutdown();}
void BotDemoRecorder::Initialize(IGameFramework* f,BotManager* m){m_impl=std::make_unique<Impl>(f,m);if(auto* recorder=f->GetIGameplayRecorder())recorder->RegisterListener(m_impl.get());}
void BotDemoRecorder::Shutdown(){if(m_impl){if(auto* r=m_impl->framework->GetIGameplayRecorder())r->UnregisterListener(m_impl.get());m_impl.reset();}}
void BotDemoRecorder::EndLevel(){if(m_impl){m_impl->Close();m_impl->failed=false;}}
void BotDemoRecorder::Stop(){if(m_impl)m_impl->Close();}
bool BotDemoRecorder::Active()const{return m_impl && m_impl->file;}
bool BotDemoRecorder::Start(const char* label,int hz){if(!m_impl)return false;try{return m_impl->Open(label,hz);}catch(const std::exception& e){m_impl->failed=true;WB_LOG(0,"[WarsBots] DEMO FAILED %s",e.what());m_impl->Close();return false;}}
void BotDemoRecorder::Update(bool enabled,int hz){if(!m_impl)return;if(!enabled){m_impl->Close();m_impl->failed=false;return;}if(!m_impl->manager->CanRunLocalIA() || !gEnv->bServer){m_impl->Close();return;}if(Active() && m_impl->hz!=std::clamp(hz,10,60))m_impl->Close();if(!Active() && !m_impl->failed)Start(nullptr,hz);if(Active())try{m_impl->Update(hz);}catch(const std::exception& e){m_impl->failed=true;WB_LOG(0,"[WarsBots] DEMO FAILED %s",e.what());m_impl->Close();}}
void BotDemoRecorder::Mark(const char* text){if(m_impl)m_impl->Safe([&]{m_impl->Event(10,0,0,float(m_impl->String(text?text:"")),Vec3(0,0,0));});}
void BotDemoRecorder::Hit(const HitInfo& hit){if(m_impl)m_impl->Safe([&]{m_impl->Event(2,hit.targetId,hit.shooterId,hit.damage,hit.pos);});}
void BotDemoRecorder::Kill(uint32_t victim,uint32_t killer){if(m_impl)m_impl->Safe([&]{m_impl->Event(3,victim,killer,0,m_impl->Position(victim));});}
void BotDemoRecorder::Revive(uint32_t id,uint32_t spawn){if(m_impl)m_impl->Safe([&]{if(auto* s=m_impl->Ensure(id))s->movementValid=false;m_impl->Event(4,id,0,float(spawn),m_impl->Position(id));});}
void BotDemoRecorder::Pickup(uint32_t id,uint32_t item){if(m_impl)m_impl->Safe([&]{auto* entity=gEnv->pEntitySystem->GetEntity(item);if(entity)m_impl->Event(5,id,item,float(m_impl->String(entity->GetClass()->GetName())),m_impl->Position(id));});}
void BotDemoRecorder::Jump(uint32_t id,bool strength){if(m_impl)m_impl->Safe([&]{m_impl->Event(8,id,0,strength?1.f:0.f,m_impl->Position(id));});}
void BotDemoRecorder::Land(uint32_t id,float height){if(m_impl)m_impl->Safe([&]{m_impl->Land(id,height);});}
float BotDemoRecorder::AverageMs()const{return m_impl && m_impl->frames?float(m_impl->totalMs/m_impl->frames):0;}
float BotDemoRecorder::WorstMs()const{return m_impl?float(m_impl->worstMs):0;}
}
