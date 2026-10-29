// Read-only offline query of existing meshes; no invented geometry or runtime actor.
#include "CryGame/Bots/BotNavMeshCore.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cmath>
int main(int argc,char**argv) {
 if(argc<3 || argc>6)return 2;WarsBots::NavMeshCore mesh;std::string err;
 if(!mesh.LoadFile(argv[1],&err)){std::printf("load_error=%s\n",err.c_str());return 3;}
 const unsigned short mask=argc>=5?static_cast<unsigned short>(std::stoul(argv[4])):14;
 std::ifstream f(argv[2]);std::stringstream ss;ss<<f.rdbuf();mesh.LoadLinkKinds(ss.str().c_str());mesh.SetLinkMask(mask);
 struct P{const char*name;float p[3];};
 const P pts[]={{"lower545",{2114.500f,1887.875f,90.375f}}, {"lower530",{2113.625f,1880.375f,90.375f}},
  {"ladder_bottom_observed",{2116.125f,1880.625f,90.586f}}, {"ladder_top_observed",{2116.245f,1879.727f,99.721f}},
  {"upper532",{2110.500f,1882.500f,103.450f}}, {"pool_rim_observed",{2123.632f,1883.959f,89.575f}},
  {"dive_low_observed",{2112.903f,1876.227f,83.656f}}, {"sea_surface",{2083.f,1863.f,99.5f}},
  {"dry_shore654",{1794.54f,1690.64f,101.53f}}};
 std::printf("polys=%d components=%d links=%d mask=%u\n",mesh.Polys(),mesh.Components(),mesh.Links(),mask);
 for(const auto&p:pts){float s[3]={};bool ok=mesh.Project(p.p,s,1.5f,1.f,3.f);std::printf("project %s ok=%d xyz=%.3f,%.3f,%.3f dz=%.3f floor_match=%d\n",p.name,ok,s[0],s[1],s[2],s[2]-p.p[2],ok&&std::fabs(s[2]-p.p[2])<1.f);}
 for(const auto&p:pts){std::vector<float>path;std::vector<WarsBots::NavPathLink>links;const char*why=nullptr;
  bool ok=mesh.FindPathLinks(pts[0].p,p.p,path,links,&why);
  std::printf("lower545_to %s path=%d reason=%s corners=%zu links=%zu\n",p.name,ok,why?why:"",path.size()/3,links.size());}
 for(int j : {7,8,0}){std::vector<float>path;std::vector<WarsBots::NavPathLink>links;const char*why=nullptr;
  bool ok=mesh.FindPathLinks(pts[4].p,pts[j].p,path,links,&why);
  std::printf("upper532_to %s path=%d reason=%s corners=%zu links=%zu\n",pts[j].name,ok,why?why:"",path.size()/3,links.size());}
 if(argc>=4){
  FILE*f=std::fopen(argv[3],"r");if(!f)return 4;float time,p[3],previous[3];bool havePrevious=false;
  while(std::fscanf(f,"%f %f %f %f",&time,p,p+1,p+2)==4){
   float s[3]={};bool projected=mesh.Project(p,s,1.5f,1.f,3.f);std::vector<float>path;std::vector<WarsBots::NavPathLink>links;const char*why=nullptr;
   bool ok=havePrevious && mesh.FindPathLinks(previous,p,path,links,&why);
   std::printf("native_trace t=%.3f xyz=%.3f,%.3f,%.3f project=%d dz=%.3f floor_match=%d from_previous=%d reason=%s\n",time,p[0],p[1],p[2],projected,s[2]-p[2],projected&&std::fabs(s[2]-p[2])<1.f,ok,why?why:"first");
   for(int k=0;k<3;++k)previous[k]=p[k];havePrevious=true;
  }
  std::fclose(f);
 }
 if(argc==6){
  FILE*f=std::fopen(argv[5],"r");if(!f)return 5;int id;float p[3];
  while(std::fscanf(f,"%d %f %f %f",&id,p,p+1,p+2)==4){
   if(p[2]>=99.5f)continue;float s[3]={};bool projected=mesh.Project(p,s,1.5f,1.f,3.f);
   std::vector<float>path;std::vector<WarsBots::NavPathLink>links;const char*why=nullptr;
   bool ok=mesh.FindPathLinks(p,pts[8].p,path,links,&why);
   std::printf("lower_spawn id=%d project=%d dz=%.3f floor_match=%d to_dryshore=%d reason=%s links=%zu\n",id,projected,s[2]-p[2],projected&&std::fabs(s[2]-p[2])<1.f,ok,why?why:"",links.size());
  }
  std::fclose(f);
 }
 return 0;
}
