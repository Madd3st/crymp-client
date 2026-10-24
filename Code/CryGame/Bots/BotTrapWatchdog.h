// Escalate only a continuously requested move without actual displacement.
#pragma once
#include "BotProgress.h"
namespace WarsBots {
class BotTrapWatchdog {
public:
 void Reset() { m_progress.Reset(); m_level=0; }
 int Update(float x,float y,float z,float now,bool requested,bool canMove,bool closedComponent=false) {
  if (!requested || !canMove) { Reset(); return 0; }
  m_progress.Stalled(x,y,z,now,8.f,.5f);
  const float age=m_progress.Age(now);
  const int next=age>=45.f?4:age>=24.f?3:(age>=16.f || (closedComponent && age>=8.f))?2:age>=8.f?1:0;
  if (next==m_level) return 0;
  m_level=next;return next;
 }
 int Level() const { return m_level; }
 float Age(float now) const { return m_progress.Age(now); }
private:
 BotProgress m_progress;
 int m_level=0;
};
}
