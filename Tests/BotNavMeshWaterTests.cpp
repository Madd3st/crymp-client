// A synthetic connected mesh with a cheap land detour around a water shortcut. No engine or baked assets needed.
#include "CryGame/Bots/BotNavMeshCore.h"
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
 for(int z=0;z<=nz;++z)for(int x=0;x<=nx;++x){verts.push_back(x*10);verts.push_back(0);verts.push_back(z*10);}
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
 p.bmin[0]=0;p.bmin[1]=100;p.bmin[2]=0;p.bmax[0]=40;p.bmax[1]=100;p.bmax[2]=30;
 p.walkableHeight=1.8f;p.walkableRadius=.3f;p.walkableClimb=.5f;p.cs=1;p.ch=.1f;p.buildBvTree=true;
 unsigned char* data=nullptr;int size=0;
 if(!dtCreateNavMeshData(&p,&data,&size)){std::puts("fixture build failed");return 2;}
 struct Header{int magic,version,numTiles;dtNavMeshParams params;};
 struct Tile{dtTileRef ref;int bytes;};
 Header h={};h.magic='M'<<24|'S'<<16|'E'<<8|'T';h.version=1;h.numTiles=1;
 h.params.orig[0]=0;h.params.orig[1]=100;h.params.orig[2]=0;h.params.tileWidth=40;h.params.tileHeight=30;h.params.maxTiles=1;h.params.maxPolys=16;
 Tile t={0,size};std::vector<unsigned char> bytes;Append(bytes,&h,sizeof h);Append(bytes,&t,sizeof t);Append(bytes,data,size);dtFree(data);
 WarsBots::NavMeshCore nav;std::string err;if(!nav.LoadMemory(bytes.data(),bytes.size(),&err)){std::puts(err.c_str());return 3;}
 const float a[3]={5,-15,100},b[3]={35,-15,100};std::vector<float> path;const char*why=nullptr;
 if(!nav.Reachable(a,b)||!nav.FindPath(a,b,path,&why)){std::puts(why?why:"unreachable");return 4;}
 float deviation=0;for(size_t i=0;i<path.size();i+=3)deviation=std::max(deviation,std::fabs(path[i+1]+15));
 std::printf("water detour: corners=%zu deviation=%.2f\n",path.size()/3,deviation);
 if(deviation<4.5f){std::puts("water shortcut selected despite expensive area");return 1;}
 std::puts("BotNavMeshWaterTests OK");return 0;
}
