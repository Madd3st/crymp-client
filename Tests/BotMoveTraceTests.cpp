#include "CryGame/Bots/BotMoveTraceState.h"
#include <cassert>
using namespace WarsBots;
int main(){
 BotMoveTraceClock c;assert(!c.Poll(0,false));assert(c.Poll(0,true));assert(!c.Poll(.249f,true));assert(c.Poll(.25f,true));assert(c.Poll(10,true));assert(!c.Poll(10.01f,true));assert(c.Poll(1,true));assert(!c.Poll(1,false));assert(c.Poll(1,true));
 {BotMoveTraceClock f;int n=0;for(int i=0;i<=1000;++i)if(f.Poll(i*.001f,true,20.f))++n;assert(n>=20 && n<=21);} BotMoveTraceClock phase;assert(phase.Poll(0,true));assert(phase.Poll(.2667f,true));assert(phase.Poll(.5f,true));assert(!phase.Poll(.51f,true));
 BotMoveTraceState s;s.Observe(0,true,true,false,10);assert(s.valid && !s.landEdges);
 s.Observe(.05f,true,true,true,10);s.Observe(.08f,true,false,true,11);s.Observe(.12f,true,false,true,14);s.Observe(.20f,true,true,false,10);
 assert(s.jumpEdges==1 && s.takeoffs==1 && s.landEdges==1 && s.peakZ==14); // Short hop entirely inside one4Hz window.
 s.Consumed(10);assert(!s.jumpEdges && !s.landEdges && s.peakZ==10);
 s.Observe(.22f,false,false,false,10);assert(!s.valid);s.Observe(.24f,true,true,false,20);assert(!s.landEdges && s.peakZ==20);
 s.Observe(.1f,true,false,true,30);assert(!s.jumpEdges && !s.takeoffs && s.peakZ==30); // Clock rollback resets, no fabricated jump.
}
