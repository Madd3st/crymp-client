// Escalate only a continuously requested move without actual displacement.
#pragma once
#include "BotProgress.h"
#include <cmath>
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

// D-064 (owner 15.11, package adb16bb8: three TRAP4 self-kills, two at the Shore submarine ladder foot right after
// LINK keep only_exit): the native self-kill stays the last resort, but level 4 first runs a series of jumps and short
// navmesh steps aside. The series has its own anchor: a jump in place lifts the feet and would reset the watchdog, so
// only a grounded bot at least `freedDist` away (2D) from where the series began counts as freed. Not freed after the
// last attempt -> Kill, once. Movement requests only (the caller): no SetPos, no impulse.
enum class TrapResortAction { None=0, Jump, StepLeft, StepRight, StepBack, Kill };
inline const char* TrapResortName(TrapResortAction a)
{
 switch (a)
 {
 case TrapResortAction::Jump: return "jump";
 case TrapResortAction::StepLeft: return "step_left";
 case TrapResortAction::StepRight: return "step_right";
 case TrapResortAction::StepBack: return "step_back";
 case TrapResortAction::Kill: return "kill";
 default: return "none";
 }
}
class TrapLastResort {
public:
 float attemptTime=2.f;   // seconds per attempt
 int attempts=6;          // jump, step left, jump, step right, jump, step back -> 12 s before the self-kill
 float freedDist=2.f;     // grounded and this far (2D) from the anchor = freed
 void Reset() { m_active=false; m_freed=false; m_killed=false; m_next=0; }
 bool Active() const { return m_active; }
 bool Freed() const { return m_freed; }
 void Begin(float x,float y,float now) { m_active=true; m_freed=false; m_killed=false; m_next=0; m_x=x; m_y=y; m_since=now; }
 int Attempt() const { return m_next; }
 // The action to start at this tick (each once), or None. Freed ends the series (Freed() true, Active() false).
 TrapResortAction Update(float x,float y,bool grounded,float now)
 {
  if (!m_active) return TrapResortAction::None;
  if (now<m_since) { m_since=now; m_next=0; }   // clock rollback (same-map reset): start the series again
  const float dx=x-m_x,dy=y-m_y;
  if (grounded && dx*dx+dy*dy>=freedDist*freedDist) { m_active=false; m_freed=true; return TrapResortAction::None; }
  if (m_next<attempts && now>=m_since+attemptTime*float(m_next))
  {
   static const TrapResortAction order[3]={TrapResortAction::StepLeft,TrapResortAction::StepRight,TrapResortAction::StepBack};
   const int i=m_next++;
   return i%2==0?TrapResortAction::Jump:order[(i/2)%3];
  }
  if (m_next>=attempts && !m_killed && now>=m_since+attemptTime*float(attempts))
  { m_killed=true; m_active=false; return TrapResortAction::Kill; }
  return TrapResortAction::None;
 }
 float Moved(float x,float y) const { const float dx=x-m_x,dy=y-m_y; return std::sqrt(dx*dx+dy*dy); }
private:
 bool m_active=false,m_freed=false,m_killed=false;
 int m_next=0;
 float m_x=0,m_y=0,m_since=0;
};
}
