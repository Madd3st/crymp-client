// A synthetic connected mesh with a cheap land detour around a water shortcut. No engine or baked assets needed.
#include "CryGame/Bots/BotNavMeshCore.h"
#include "CryGame/Bots/BotRoamFallback.h"
#include "DetourNavMeshBuilder.h"
#include "DetourNavMesh.h"
#include <algorithm>
#include "DetourAlloc.h"
#include <vector>
#include <string>
#include <cstdio>
#include <cmath>
#include <cstring>

static void Append(std::vector<unsigned char>& out,const void* p,size_t n)
{const auto* b=static_cast<const unsigned char*>(p);out.insert(out.end(),b,b+n);}
int main()
{
 const int nx=4,nz=3,nvp=4;
 std::vector<unsigned short> verts;
 for(int z=0;z<=nz;++z)for(int x=0;x<=nx;++x){verts.push_back(x*100);verts.push_back(100);verts.push_back(z*100);}
 std::vector<unsigned short> polys(nx*nz*nvp*2,0xffff),flags(nx*nz,1);
 std::vector<unsigned char> areas(nx*nz,63);
 for(int z=0;z<nz;++z)for(int x=0;x<nx;++x){
  int i=z*nx+x;auto* p=&polys[i*nvp*2];
  p[0]=z*(nx+1)+x;p[1]=(z+1)*(nx+1)+x;p[2]=(z+1)*(nx+1)+x+1;p[3]=z*(nx+1)+x+1;
  if(x>0)p[nvp]=i-1;if(z+1<nz)p[nvp+1]=i+nx;
  if(x+1<nx)p[nvp+2]=i+1;if(z>0)p[nvp+3]=i-nx;
  if(z==1&&(x==1||x==2))areas[i]=4;
 }
 dtNavMeshCreateParams p={};p.verts=verts.data();p.vertCount=(int)verts.size()/3;p.polys=polys.data();
 p.polyFlags=flags.data();p.polyAreas=areas.data();p.polyCount=nx*nz;p.nvp=nvp;
 p.bmin[0]=0;p.bmin[1]=90;p.bmin[2]=0;p.bmax[0]=40;p.bmax[1]=110;p.bmax[2]=30;
 p.walkableHeight=1.8f;p.walkableRadius=.3f;p.walkableClimb=.5f;p.cs=.1f;p.ch=.1f;p.buildBvTree=true;
 unsigned char* data=nullptr;int size=0;
 if(!dtCreateNavMeshData(&p,&data,&size)){std::puts("fixture build failed");return 2;}
 struct Header{int magic,version,numTiles;dtNavMeshParams params;};
 struct Tile{dtTileRef ref;int bytes;};
 Header h={};h.magic='M'<<24|'S'<<16|'E'<<8|'T';h.version=1;h.numTiles=1;
 h.params.orig[0]=0;h.params.orig[1]=90;h.params.orig[2]=0;h.params.tileWidth=40;h.params.tileHeight=30;h.params.maxTiles=1;h.params.maxPolys=16;
 Tile t={0,size};std::vector<unsigned char> bytes;Append(bytes,&h,sizeof h);Append(bytes,&t,sizeof t);Append(bytes,data,size);dtFree(data);
 WarsBots::NavMeshCore nav;std::string err;if(!nav.LoadMemory(bytes.data(),bytes.size(),&err)){std::puts(err.c_str());return 3;}
 const float a[3]={5,-15,100},b[3]={35,-15,100};std::vector<float> path;const char*why=nullptr;
 if(!nav.Reachable(a,b)||!nav.FindPath(a,b,path,&why)){std::puts(why?why:"unreachable");return 4;}
 float deviation=0;for(size_t i=0;i<path.size();i+=3)deviation=std::max(deviation,std::fabs(path[i+1]+15));
 std::printf("water detour: corners=%zu deviation=%.2f\n",path.size()/3,deviation);
 if(deviation<4.5f){std::puts("water shortcut selected despite expensive area");return 1;}
 // Use production cs=0.1 and non-degenerate vertical tile bounds. Detour clamps BV-tree query bounds to
 // tile bounds; bminY==bmaxY at the plane falsely admitted a query wholly below that plane.
 // Vertices keep the physical 40x30 m tile and z=100 plane with the padded 90..110 vertical bounds.
 // Real-core water-start controls: measure the plane, and retain strict raw dry RED / bounded water GREEN checks.
 {
  const float onMesh[3]={5,-15,100};           // land tile x0, known on-mesh before the plane is known
  float plane[3];
  if(!nav.Project(onMesh,plane)){std::puts("DS089 fixture plane not found");return 5;}
  const float planeZ=plane[2]; std::printf("fixture planeZ=%.3f\n",planeZ);
  if(planeZ<95.0f||planeZ>105.0f){std::puts("DS089 fixture plane out of range");return 5;}
  const float land[3]={5,-15,planeZ};          // dry land at the measured plane
  const float swim[3]={15,-15,planeZ-1.5f};    // water row x1, native float 1.5 m below the plane
  const float deep[3]={15,-15,planeZ-2.3f};    // C147 measured floats up to 2.31 m below the plane
  float snap[3];
  // (a) raw native swimmer start: the dry box must not project (RED, b06681ea behaviour).
  if(nav.Project(swim,snap)){std::puts("DS089 raw dry projection unexpectedly found");return 6;}
  if(nav.Reachable(swim,land)){std::puts("DS089 raw dry reach unexpectedly found");return 7;}
  // (b) plane-sized water box: projects and reaches; a fixed 2.0 m box loses the 2.3 m float (the C147 extra ten).
  const WarsBots::NavQueryBounds water=WarsBots::WaterQueryBounds(1.5f);
  if(!nav.Project(swim,snap,water.side,water.up,water.down,true)){std::puts("DS089 water projection failed");return 8;}
  if(std::fabs(snap[2]-planeZ)>0.6f){std::puts("DS089 water projection wrong height");return 9;}
  if(!nav.Reachable(swim,land,2.6f,&water)){std::puts("DS089 water reach failed");return 10;}
  const WarsBots::NavQueryBounds fixed2=WarsBots::WaterQueryBounds();
  if(nav.Reachable(deep,land,2.6f,&fixed2)){std::puts("DS089 fixed 2.0 box reached a 2.3 m float");return 11;}
  const WarsBots::NavQueryBounds deepBox=WarsBots::WaterQueryBounds(2.3f);
  if(!nav.Reachable(deep,land,2.6f,&deepBox)){std::puts("DS089 plane-sized box missed the deep float");return 12;}
  // (c) water query is water-only: a land start fails, the dry caller still works.
  if(nav.Reachable(land,land,2.6f,&water)){std::puts("DS089 water query accepted land");return 13;}
  if(!nav.Reachable(land,land)){std::puts("DS089 dry land failed");return 14;}
  // (d) no water ref (outside the mesh): fail closed, never substitute the land/dry.
  const float hole[3]={-5,-15,planeZ-1.5f};
  if(nav.Reachable(hole,land,2.6f,&water)){std::puts("DS089 water query accepted hole");return 15;}
  // (e) corner ray: raw feet fail (C147 0/1085), the plane-sized box makes the ray usable.
  if(nav.RayClear(swim,land)){std::puts("DS089 raw-feet ray unexpectedly clear");return 16;}
  if(!nav.RayClear(swim,land,&water)){std::puts("DS089 water ray not clear");return 17;}
  // (f) DS-092: the local water leg's GOAL is projected with the dry box, so a goal at the swimmer's feet z is not
  // pathable while the same XY at the native plane is - this is why the committed patrol goal must be lifted too,
  // not only the four samples.
  const float goalFeetZ[3]={18,-15,planeZ-1.5f};   // 3 m along the water row, feet height
  const float goalWaterZ[3]={18,-15,WarsBots::RoamGoalZ(true,swim[2],planeZ)};       // the same leg at the native water plane
  std::vector<float> lp; std::vector<WarsBots::NavPathLink> ll; const char* lw=nullptr;
  if(nav.FindPathLinks(swim,goalFeetZ,lp,ll,&lw,&water)){std::puts("DS092 feet-z water goal unexpectedly pathable");return 18;}
  if(nav.Reachable(swim,goalFeetZ,2.6f,&water)){std::puts("DS092 feet-z water goal unexpectedly reachable");return 19;}
  if(!nav.FindPathLinks(swim,goalWaterZ,lp,ll,&lw,&water)){std::puts(lw?lw:"DS092 water-plane goal path failed");return 20;}
  if(!nav.Reachable(swim,goalWaterZ,2.6f,&water)){std::puts("DS092 water-plane goal not reachable");return 21;}
  const float offGoal[3]={60,-15,planeZ};          // fail-closed: off-mesh local goal stays rejected
  if(nav.Reachable(swim,offGoal,2.6f,&water)){std::puts("DS092 off-mesh water goal accepted");return 22;}
 }
 std::puts("BotNavMeshWaterTests OK");return 0;
}
