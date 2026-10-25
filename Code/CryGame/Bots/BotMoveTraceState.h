// Read-only sampling state, independent of the engine; no catch-up bursts after a stall.
#pragma once
#include <cmath>
namespace WarsBots {
struct BotMoveTraceClock {
 float next=0,last=-1;bool active=false;
 bool Poll(float now,bool enabled,float hz=4.f) {
  if(!(hz>=1.f))hz=1.f; if(hz>30.f)hz=30.f;
  if(!enabled){active=false;return false;}
  if(!active || now<last){next=now;active=true;}
  last=now;if(now<next)return false;next+=(std::floor((now-next)*hz)+1.f)/hz;return true;
 }
};
struct BotMoveTraceState {
 bool valid=false,grounded=false,jumped=false;
 unsigned takeoffs=0,jumpEdges=0,landEdges=0;
 float last=-1,jumpTime=-1,landTime=-1,peakZ=0;
 void Observe(float now,bool alive,bool ground,bool jump,float z) {
  if(!alive){*this=BotMoveTraceState();return;}
  if(!valid || now<last){*this=BotMoveTraceState();valid=true;grounded=ground;jumped=jump;last=now;peakZ=z;return;}
  if(z>peakZ)peakZ=z;
  if(jump && !jumped){++jumpEdges;jumpTime=now;}
  if(grounded && !ground)++takeoffs;
  if(!grounded && ground){++landEdges;landTime=now;}
  grounded=ground;jumped=jump;last=now;
 }
 void Consumed(float z){takeoffs=jumpEdges=landEdges=0;peakZ=z;}
};
}
