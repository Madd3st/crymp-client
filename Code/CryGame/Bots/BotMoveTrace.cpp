#include "PortIncludes.h"
#include "BotConfig.h"
#include "BotMoveTrace.h"
#include "BotMoveTraceEligibility.h"
#include "BotManager.h"
#include "BotMatch.h"
#include "BotLog.h"
#include "CryGame/Game.h"
#include "CryGame/GameRules.h"
#include "CryGame/Actors/Player/Player.h"
#include "CryGame/Actors/Player/NanoSuit.h"
#include <cstring>
namespace WarsBots {
void BotMoveTrace::Update(IGameFramework* framework,const BotManager& manager)
{
 const int& flag=BotConfig::Get().moveTrace;
 CGameRules* rules=g_pGame?g_pGame->GetGameRules():nullptr;
 // DS-076/C145 X-218: use the existing BotMatch policy instead of a hardcoded "InstantAction" class name, so the
 // planned PowerStruggle native-water observation can reach Visit. Opt-in flag and local/multiplayer guards unchanged.
 MoveTraceMode mode=MoveTraceMode::Unsupported;
 if(rules){
  if(BotMatch::IsPowerStruggle(*rules))mode=MoveTraceMode::PowerStruggle;
  else if(BotMatch::IsBotMode(*rules))mode=MoveTraceMode::InstantAction;
 }
 const bool enabled=framework && rules && MoveTraceEligible(flag,gEnv->bServer,manager.CanRunLocalIA(),gEnv->bMultiplayer,mode);
 if(!enabled){if(m_clock.active)Reset();return;}
 const float now=gEnv->pTimer->GetCurrTime();// C-084: the owner's client trace at wb_move_trace_hz (circle/wall jumps need >= 20 Hz); server and bots stay 4 Hz
 const bool due=m_clock.Poll(now,true,gEnv->bServer?4.f:float(BotConfig::Get().moveTraceHz));++m_frame;
 if(gEnv->bServer){
  IActorIteratorPtr actors=framework->GetIActorSystem()->CreateActorIterator();
  while(IActor* actor=actors->Next()){
   CPlayer* player=CPlayer::FromIActor(actor);if(!player)continue;
   const bool bot=manager.Find(player->GetEntityId())!=nullptr;
   if(bot || player->GetChannelId()>0)Visit(*player,bot,due,now);
  }
 }else {
  if(CPlayer* player=CPlayer::FromIActor(framework->GetClientActor()))Visit(*player,false,due,now);
  if(flag==2){
   // Spawn metadata identifies remote WarsBots; never infer ownership from an EntityId/name.
   IActorIteratorPtr actors=framework->GetIActorSystem()->CreateActorIterator();
   while(IActor* actor=actors->Next()){
    CPlayer* player=CPlayer::FromIActor(actor);
    if(player && !player->IsClient() && player->IsWarsBotActor())Visit(*player,true,true,now);
   }
  }
 }
 for(auto it=m_records.begin();it!=m_records.end();)if(it->second.seen!=m_frame)it=m_records.erase(it);else ++it;
}
void BotMoveTrace::Visit(CPlayer& actor,bool bot,bool due,float now)
{
 if(actor.GetSpectatorMode()!=CActor::eASM_None)return;
 const unsigned id=actor.GetEntityId();auto found=m_records.find(id);
 if(found==m_records.end()){
  if(m_records.size()>=64)return;
  found=m_records.emplace(id,Record()).first;
 }
 Record& record=found->second;record.seen=m_frame;
 const SPlayerStats& stats=*static_cast<const SPlayerStats*>(actor.GetActorStats());
 const Vec3 pos=actor.GetEntity()->GetWorldPos();const bool alive=actor.GetHealth()>0;
 const bool ground=stats.onGround>0.f && stats.inAir<=.01f;
 record.state.Observe(now,alive,ground,stats.jumped,pos.z);
 if(!due || !alive || m_capped)return;
 if(m_lines>=200000){WB_LOG(0,"[WarsBots] MOVE TRACE capped lines=200000; toggle wb_move_trace0/1 for a new capture");m_capped=true;return;}
 Vec3 velocity=stats.velocity;int flying=-1;
 if(IPhysicalEntity* physics=actor.GetEntity()->GetPhysics()){
  pe_status_dynamics dynamics;if(physics->GetStatus(&dynamics))velocity=dynamics.v;
  pe_status_living living;if(physics->GetStatus(&living))flying=living.bFlying?1:0;
 }
 CNanoSuit* suit=actor.GetNanoSuit();const BotMoveTraceState& state=record.state;
 const Vec3 view=actor.GetViewRotation().GetColumn1();
 const float yaw=RAD2DEG(atan2f(view.x,view.y));
 const float pitch=RAD2DEG(atan2f(view.z,sqrtf(view.x*view.x+view.y*view.y)));
 WB_LOG(2,"[WarsBots] MOVE TRACE actor=%u kind=%s side=%s local=%d t=%.3f sampleDt=%.3f pos=(%.3f,%.3f,%.3f) vel=(%.3f,%.3f,%.3f) speed=%.3f vz=%.3f mode=%d energy=%.1f sprint=%d grounded=%d flying=%d jump=%d land=%d takeoffs=%u jumpEdges=%u landEdges=%u jumpTime=%.3f landTime=%.3f peakZ=%.3f health=%d frame=%u yaw=%.3f pitch=%.3f remote=%d stance=%d source=PostUpdate_entity_view swim=%d waterZ=%.2f relativeWater=%.2f bottomDepth=%.2f underwater=%.2f",
  id,bot?"bot":"human",gEnv->bServer?"server":"client",actor.IsClient()?1:0,now,record.logged<0?0:now-record.logged,
  pos.x,pos.y,pos.z,velocity.x,velocity.y,velocity.z,velocity.GetLength2D(),velocity.z,
  suit?suit->GetMode():-1,suit?suit->GetSuitEnergy():-1.f,actor.IsSprinting()?1:0,ground?1:0,flying,stats.jumped?1:0,stats.landed?1:0,
  state.takeoffs,state.jumpEdges,state.landEdges,state.jumpTime,state.landTime,state.peakZ,actor.GetHealth(),m_frame,yaw,pitch,(!gEnv->bServer && bot && !actor.IsClient())?1:0,int(actor.GetStance()),actor.ShouldSwim()?1:0,stats.worldWaterLevel,stats.relativeWaterLevel,stats.relativeBottomDepth,stats.headUnderWaterTimer);
 record.logged=now;record.state.Consumed(pos.z);++m_lines;
}
}
