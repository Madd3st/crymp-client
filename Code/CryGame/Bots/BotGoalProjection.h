// Conservative goal projection: one reachable nearby floor keeps the goal.
#pragma once
namespace WarsBots {
struct BotGoalProjection {
 int to=-1,candidates=0,reachableCandidates=0;
 float toZ=0,bestScore=1e9f;
 void Consider(int node,float z,float score,bool reachable) {
  ++candidates;if(reachable)++reachableCandidates;
  if(score<bestScore){bestScore=score;to=node;toZ=z;}
 }
 bool Unreachable()const{return candidates>0 && reachableCandidates==0;}
};
}
