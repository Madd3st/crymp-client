#include "PortIncludes.h"
#include "BotMoveTrace.h"
#include "BotManager.h"
#include "BotLog.h"
#include "CryGame/Game.h"
#include "CryGame/GameRules.h"
#include "CryGame/Actors/Player/Player.h"
#include "CryGame/Actors/Player/NanoSuit.h"
#include <cstring>
namespace WarsBots {
void BotMoveTrace::Update(IGameFramework* framework,const BotManager& manager)
{
 const ICVar* flag=gEnv->pConsole->GetCVar("wb_move_trace");
 CGameRules* rules=g_pGame?g_pGame->GetGameRules():nullptr;
 const bool enabled=flag && flag->GetIVal()==1 && framework && rules &&
  !std::strcmp(rules->GetEntity()->GetClass()->GetName(),"InstantAction") &&
  (gEnv->bServer?manager.CanRunLocalIA():gEnv->bMultiplayer);
 if(!enabled){if(m_clock.active)Reset();return;}
 const float now=gEnv->pTimer->GetCurrTime();const bool due=m_clock.Poll(now,true);++m_frame;
 if(gEnv->bServer){
  IActorIteratorPtr actors=framework->GetIActorSystem()->CreateActorIterator();
  while(IActor* actor=actors->Next()){
   CPlayer* player=CPlayer::FromIActor(actor);if(!player)continue;
   const bool bot=manager.Find(player->GetEntityId())!=nullptr;
   if(bot || player->GetChannelId()>0)Visit(*player,bot,due,now);
  }
 }else if(CPlayer* player=CPlayer::FromIActor(framework->GetClientActor()))Visit(*player,false,due,now);
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
 WB_LOG(2,"[WarsBots] MOVE TRACE actor=%u kind=%s side=%s local=%d t=%.3f sampleDt=%.3f pos=(%.3f,%.3f,%.3f) vel=(%.3f,%.3f,%.3f) speed=%.3f vz=%.3f mode=%d energy=%.1f sprint=%d grounded=%d flying=%d jump=%d land=%d takeoffs=%u jumpEdges=%u landEdges=%u jumpTime=%.3f landTime=%.3f peakZ=%.3f health=%d",
  id,bot?"bot":"human",gEnv->bServer?"server":"client",actor.IsClient()?1:0,now,record.logged<0?0:now-record.logged,
  pos.x,pos.y,pos.z,velocity.x,velocity.y,velocity.z,velocity.GetLength2D(),velocity.z,
  suit?suit->GetMode():-1,suit?suit->GetSuitEnergy():-1.f,actor.IsSprinting()?1:0,ground?1:0,flying,stats.jumped?1:0,stats.landed?1:0,
  state.takeoffs,state.jumpEdges,state.landEdges,state.jumpTime,state.landTime,state.peakZ,actor.GetHealth());
 record.logged=now;record.state.Consumed(pos.z);++m_lines;
}
}
