#include "CryGame/Bots/BotTrapWatchdog.h"
#include <cassert>
using namespace WarsBots;
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
}
