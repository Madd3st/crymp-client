#include "CryGame/Bots/BotPhysicsProbe.h"
#include <cassert>
using namespace WarsBots;
int main(){
 BotPhysicsProbe p;assert(p.Take(20)==0);
 p.Start(10);assert(p.Take(10.9f)==0);assert(p.Take(11)==1);assert(p.Take(12)==0);
 assert(p.Take(15)==2);assert(p.Take(16)==0);
 p.Start(20);assert(p.Take(26)==3);assert(p.Elapsed(26)==6);assert(p.Take(27)==0);
 p.Start(30);p.Cancel();assert(p.Take(40)==0);
 p.Start(50);p.Start(51);assert(p.Take(51)==0);assert(p.Take(52)==1);
 p.Start(60);assert(p.Take(59)==0);assert(p.Take(70)==0);
}
