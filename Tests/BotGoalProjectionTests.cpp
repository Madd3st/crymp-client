#include "CryGame/Bots/BotGoalProjection.h"
#include "CryGame/Bots/BotReachability.h"
#include <cassert>
using namespace WarsBots;
int main(){
 BotGoalProjection p;assert(!p.Unreachable());
 p.Consider(1,10,1,false);assert(p.Unreachable());
 p.Consider(2,10.2f,2,true);assert(!p.Unreachable());assert(p.to==1 && p.candidates==2 && p.reachableCandidates==1);
 p=BotGoalProjection();p.Consider(3,30,3,false);p.Consider(4,31,1,false);assert(p.Unreachable() && p.to==4 && p.toZ==31);
 BotReachability graph;assert(graph.Build({{1},{0,2},{},{}}));
 p=BotGoalProjection();for(int id:{3,1})p.Consider(id,0,float(id),graph.Reaches(0,id));assert(!p.Unreachable());
 assert(graph.Closed(2) && graph.Closed(3)); // Source guard must fail open for sinks/isolated nodes.
}
