#include "CryGame/Bots/BotReachability.h"
#include <cassert>
using namespace WarsBots;
int main(){
 BotReachability r;std::vector<std::vector<int>> g{{1},{0,2},{3},{4},{3},{}};
 assert(r.Build(g));assert(r.Count()==4);assert(r.Component(0)==r.Component(1));
 assert(r.Reaches(0,4));assert(!r.Reaches(4,0));assert(r.Reaches(3,4));assert(r.Reaches(5,5));
 assert(!r.Reaches(5,0));assert(!r.Reaches(-1,0));assert(r.Closed(3));assert(r.Closed(5));assert(!r.Closed(0));
 assert(r.Build({{1},{0}}));assert(!r.Closed(0));assert(r.Size(0)==2);
 assert(r.Build({{1,2},{3},{3},{}}));assert(r.Reaches(0,3));assert(!r.Reaches(1,2));
 std::vector<std::vector<int>> chain(8000);for(int i=0;i<7999;++i)chain[i].push_back(i+1);
 assert(r.Build(chain));assert(r.Reaches(0,7999));assert(!r.Reaches(7999,0));
 assert(!r.Build(std::vector<std::vector<int>>(15000)));assert(!r.Ready());
 assert(!r.Build({{1}}));r.Reset();assert(!r.Ready());assert(!r.Build({}));
}
