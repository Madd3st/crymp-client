#include "CryGame/Bots/BotNavMeshCore.h"
#include <fstream>
#include <sstream>
#include <cstdio>
#include <string>
int main(int argc,char**argv){if(argc!=4)return 2;WarsBots::NavMeshCore m;std::string e;if(!m.LoadFile(argv[1],&e))return 3;std::ifstream lf(argv[2]);std::stringstream ls;ls<<lf.rdbuf();m.LoadLinkKinds(ls.str().c_str());m.SetLinkMask(14);std::ifstream f(argv[3]);float t,x,y,z,w;int n=0,oldP=0,newP=0,oldR=0,newR=0;const float goal[3]={1794.54f,1690.64f,101.53f};
while(f>>t>>x>>y>>z>>w){float feet[3]={x,y,z},surface[3]={x,y,w},a[3],b[3];bool p=m.Project(feet,a),q=m.Project(surface,b),r=m.Reachable(feet,goal),s=m.Reachable(surface,goal);if(!q)printf("surfaceMissing t=%.3f pos=(%.3f,%.3f,%.3f) water=%.2f\n",t,x,y,z,w);++n;oldP+=p;newP+=q;oldR+=r;newR+=s;if(n==1||n%200==0)printf("t=%.3f feetZ=%.3f nativeWater=%.2f rawProject=%d rawReach654=%d surfaceProject=%d surfaceReach654=%d snappedZ=%.3f\n",t,z,w,p,r,q,s,q?b[2]:-999.f);}
printf("samples=%d rawProjected=%d surfaceProjected=%d rawReach654=%d surfaceReach654=%d\n",n,oldP,newP,oldR,newR);return 0;}
