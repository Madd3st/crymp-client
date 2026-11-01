#include "CryGame/Bots/BotNavMeshCore.h"
#include <fstream>
#include <sstream>
#include <cstdio>
#include <map>
int main(int argc,char**argv){
 if(argc!=4)return 2; WarsBots::NavMeshCore m;std::string e;
 if(!m.LoadFile(argv[1],&e))return 3;
 std::ifstream lf(argv[2]);std::stringstream ls;ls<<lf.rdbuf();m.LoadLinkKinds(ls.str().c_str());m.SetLinkMask(14);
 std::ifstream f(argv[3]);float t,x,y,z,w;int n=0,p=0,r=0,paths=0,raw=0,ray=0,waterCorners=0;
 std::map<std::string,int> failures;const float goal[3]={1794.54f,1690.64f,101.53f};auto box=WarsBots::WaterQueryBounds();
 while(f>>t>>x>>y>>z>>w){float feet[3]={x,y,z},snap[3];std::vector<float> xyz;std::vector<WarsBots::NavPathLink> links;const char*why=nullptr;
 ++n;raw+=m.Project(feet,snap);p+=m.Project(feet,snap,box.side,box.up,box.down,true);r+=m.Reachable(feet,goal,2.6f,&box);
 bool ok=m.FindPathLinks(feet,goal,xyz,links,&why,&box);paths+=ok;if(!ok)++failures[why?why:"null"];
 if(ok&&xyz.size()>=3){ray+=m.RayClear(feet,xyz.data());for(size_t i=0;i<xyz.size();i+=3){float a[3]={xyz[i],xyz[i+1],xyz[i+2]-1.5f};waterCorners+=m.Project(a,snap,box.side,box.up,box.down,true);}}
 if(n==1&&ok){for(size_t i=0;i<xyz.size();i+=3)printf("entry corner %zu %.3f %.3f %.3f\n",i/3,xyz[i],xyz[i+1],xyz[i+2]);}
 }
 printf("rows=%d raw=%d waterProject=%d waterReach=%d paths=%d rayFromFeet=%d waterCorners=%d\n",n,raw,p,r,paths,ray,waterCorners);for(auto&v:failures)printf("failure %s=%d\n",v.first.c_str(),v.second);
 return n==1136?0:4;
}
