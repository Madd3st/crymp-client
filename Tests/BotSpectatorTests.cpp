#include "CryGame/Bots/BotSpectator.h"
#include <cassert>
using namespace WarsBots;
int main() {
 std::vector<unsigned> ids{10,20,30},empty,one{10};
 assert(BotSpectator::Cycle(ids,10u,1)==20);
 assert(BotSpectator::Cycle(ids,30u,1)==10);
 assert(BotSpectator::Cycle(ids,10u,-1)==30);
 assert(BotSpectator::Cycle(ids,99u,1)==10);
 assert(BotSpectator::Cycle(ids,99u,-1)==30);
 assert(BotSpectator::Cycle(one,10u,-1)==10);
 assert(BotSpectator::Cycle(empty,0u,1)==0);
 assert(BotSpectator::CanInspect(true,true,true));
 assert(!BotSpectator::CanInspect(true,true,false));
 assert(!BotSpectator::CanInspect(false,true,true));
 assert(!BotSpectator::CanInspect(true,false,true));
 assert(BotSpectator::Eligible(true,true,false,false));
 assert(!BotSpectator::Eligible(false,true,false,false));
 assert(!BotSpectator::Eligible(true,false,false,false));
 assert(!BotSpectator::Eligible(true,true,true,false));
 assert(!BotSpectator::Eligible(true,true,false,true));
}
