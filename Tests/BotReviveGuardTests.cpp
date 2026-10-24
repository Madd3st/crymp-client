#include "CryGame/Bots/BotReviveGuard.h"
#include <cassert>
using namespace WarsBots;
int main(){
 using R=BotReviveGuard::Result;BotReviveGuard g;
 g.Begin(0,true);assert(g.Check(1.99f,0,true,true)==R::Waiting);assert(g.Check(2,0,true,false)==R::Retry && g.Retries()==1);
 g.Begin(2,false);assert(g.Check(4,.1f,false,true)==R::Retry && g.Retries()==2);g.Begin(4,false);assert(g.Check(6,0,true,true)==R::Exhausted);assert(!g.Pending());assert(g.Check(8,0,true,true)==R::Waiting);
 g.Begin(10,true);assert(g.Retries()==0);assert(g.Check(12,.3f,true,true)==R::Clear); // Moving/airborne actors kept.
 g.Begin(20,true);assert(g.Check(22,0,false,false)==R::Clear); // Intentional idle is not a freeze.
 g.Begin(30,true);g.Cancel();assert(g.Check(32,0,true,true)==R::Waiting);g.Begin(40,true);assert(g.Check(39,0,true,true)==R::Clear);
}
