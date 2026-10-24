// C014 bounded post-revive check; owner-approved native retries, never a kill.
#pragma once
namespace WarsBots {
class BotReviveGuard {
 bool m_pending=false;float m_since=0;unsigned m_retries=0;
public:
 enum class Result { Waiting, Clear, Retry, Exhausted };
 void Begin(float now,bool newLife){m_since=now;m_pending=true;if(newLife)m_retries=0;}
 void Cancel(){m_pending=false;m_retries=0;}
 unsigned Retries()const{return m_retries;}
 bool Pending()const{return m_pending;}
 Result Check(float now,float moved,bool requesting,bool badBody){
  if(!m_pending)return Result::Waiting;
  if(now<m_since){Cancel();return Result::Clear;}
  if(now-m_since<2.f)return Result::Waiting;
  m_pending=false;
  // Actual displacement prevents re-reviving a healthy jump/airborne actor.
  if(moved>=.3f || (!requesting && !badBody))return Result::Clear;
  if(m_retries>=2)return Result::Exhausted;
  ++m_retries;return Result::Retry;
 }
};
}
