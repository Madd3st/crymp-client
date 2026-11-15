#include "CryGame/Bots/BotTrapWatchdog.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
using namespace WarsBots;

// The RecoverTrap caller in miniature (BotBrain.cpp): the watchdog runs until level 4, then only the series runs.
// `world` moves the bot in reply to the action it was given.
struct Sim {
 BotTrapWatchdog w; TrapLastResort r;
 float x=0,y=0,z=0; bool grounded=true;
 std::vector<TrapResortAction> actions;
 int kills=0, freed=0;
 template<class World> void Run(float until,World world)
 {
  for (int i=0;i*0.1f<=until;++i)
  {
   const float t=float(i)*0.1f;
   if (r.Active())
   {
    const TrapResortAction a=r.Update(x,y,grounded,t);
    if (r.Freed()) { ++freed; w.Reset(); r.Reset(); continue; }
    if (a==TrapResortAction::Kill) { ++kills; r.Reset(); x=y=z=0; w.Reset(); continue; }   // respawn
    if (a!=TrapResortAction::None) actions.push_back(a);
    world(*this,a,t);
    continue;
   }
   if (w.Update(x,y,z,t,true,true)==4) { r.Begin(x,y,t); continue; }
   world(*this,TrapResortAction::None,t);
  }
 }
};

int main() {
 BotTrapWatchdog w;
 assert(w.Update(0,0,0,0,true,true)==0);
 assert(w.Update(0,0,0,7.99f,true,true)==0);
 assert(w.Update(0,0,0,8,true,true)==1);
 assert(w.Update(0,0,0,9,true,true)==0);
 assert(w.Update(.1f,0,0,16,true,true)==2);
 assert(w.Update(0,0,0,24,true,true)==3);
 assert(w.Update(0,0,0,44.99f,true,true)==0);
 assert(w.Update(0,0,0,45,true,true)==4);
 assert(w.Update(0,0,0,60,true,true)==0); // terminal action once, no suicide loop
 assert(w.Update(.5f,0,0,61,true,true)==0 && w.Level()==0);
 assert(w.Update(.5f,0,0,69,true,true)==1);
 w.Update(.5f,0,0,70,false,true);assert(w.Level()==0);
 w.Update(0,0,0,71,true,true);assert(w.Update(0,0,0,79,true,true)==1);
 w.Update(0,0,0,80,true,false);assert(w.Level()==0);
 w.Update(0,0,0,81,true,true);assert(w.Update(0,0,.6f,89,true,true)==0); // climb counts as progress
 assert(w.Update(0,0,.6f,2,true,true)==0 && w.Level()==0); // clock rollback
 w.Reset();w.Update(0,0,0,0,true,true,true);
 assert(w.Update(0,0,0,8,true,true,true)==2); // Closed SCC bypasses repath at8s.
 assert(w.Update(0,0,0,16,true,true,true)==0);
 assert(w.Update(0,0,0,24,true,true,true)==3);
 assert(w.Update(0,0,0,45,true,true,true)==4);

 // D-064 (package adb16bb8: TRAP4 self-kills t990.03, t1097.40 right after LINK keep only_exit, t2969.20 - the last two
 // at the Shore submarine ladder foot (2116,1880,90)): jumps and navmesh steps first, the self-kill only if none helped.
 {
  // the order of the series and the time of the kill: jump, left, jump, right, jump, back, then kill at +12 s, once
  TrapLastResort r; r.Begin(0,0,45.f);
  std::vector<TrapResortAction> got;
  int kills=0;
  for (int i=0;i<250;++i)
  {
   const float t=45.f+float(i)*0.1f;
   const TrapResortAction a=r.Update(0,0,true,t);
   if (a==TrapResortAction::Kill) { ++kills; assert(t>=57.f-1e-3f); }
   else if (a!=TrapResortAction::None) got.push_back(a);
  }
  const TrapResortAction want[6]={TrapResortAction::Jump,TrapResortAction::StepLeft,TrapResortAction::Jump,
   TrapResortAction::StepRight,TrapResortAction::Jump,TrapResortAction::StepBack};
  assert(got.size()==6 && kills==1 && !r.Active() && !r.Freed());
  for (int i=0;i<6;++i) assert(got[size_t(i)]==want[i]);
 }
 {
  // the bot the first jump frees does not die: stuck 45 s, the jump lifts it over the lip and it lands 2.5 m on
  Sim s;
  bool jumped=false; float landAt=0;
  s.Run(120.f,[&](Sim& m,TrapResortAction a,float t)
  {
   if (a==TrapResortAction::Jump && !jumped) { jumped=true; m.grounded=false; m.z=1.f; landAt=t+0.8f; }
   if (jumped && !m.grounded && t>=landAt) { m.grounded=true; m.z=0; m.x=2.5f; }
   if (m.freed) m.x+=0.5f;   // free: it walks on
  });
  assert(s.kills==0 && s.freed==1);
  assert(s.actions.size()==1 && s.actions[0]==TrapResortAction::Jump);
 }
 {
  // a jump in place (the feet go up and come down at the same spot) is no exit: the series goes on to the kill
  Sim s;
  s.Run(58.5f,[&](Sim& m,TrapResortAction a,float)
  {
   if (a==TrapResortAction::Jump) { m.z=1.f; m.grounded=false; }
   else { m.z=0; m.grounded=true; }
  });
  assert(s.kills==1 && s.freed==0 && s.actions.size()==6);
 }
 {
  // a step aside frees it (second attempt): no kill, the watchdog starts over
  Sim s;
  s.Run(120.f,[&](Sim& m,TrapResortAction a,float)
  {
   if (a==TrapResortAction::StepLeft) m.y+=2.2f;
   if (m.freed) m.x+=0.5f;
  });
  assert(s.kills==0 && s.freed==1 && s.actions.size()==2 && s.actions[1]==TrapResortAction::StepLeft);
 }
 {
  // in the air far from the anchor is not freed yet; grounded there is
  TrapLastResort r; r.Begin(0,0,0);
  assert(r.Update(3,0,false,0.5f)==TrapResortAction::Jump && r.Active());
  assert(r.Update(3,0,true,0.6f)==TrapResortAction::None && r.Freed() && !r.Active());
  // clock rollback inside the series starts it again, never an instant kill
  TrapLastResort b; b.Begin(0,0,100.f);
  for (int i=0;i<20;++i) b.Update(0,0,true,100.f+float(i)*0.5f);
  assert(b.Update(0,0,true,3.f)==TrapResortAction::Jump && b.Active());
  for (int i=1;i<24;++i) assert(b.Update(0,0,true,3.f+float(i)*0.5f)!=TrapResortAction::Kill);
  assert(b.Update(0,0,true,15.f)==TrapResortAction::Kill);
  assert(std::string(TrapResortName(TrapResortAction::StepBack))=="step_back");
 }
 std::puts("BotTrapWatchdogTests OK");
 return 0;
}
