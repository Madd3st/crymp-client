#include "CryGame/Bots/BotGeometryMesh.h"
#include <cassert>
#include <algorithm>
using namespace WarsBots::GeometryMesh;
Point sub(Point a,Point b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Point cross(Point a,Point b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float dot(Point a,Point b){return a.x*b.x+a.y*b.y+a.z*b.z;}
int main(){
 int count=0;auto box=[&](Triangle t){++count;auto n=cross(sub(t[1],t[0]),sub(t[2],t[0]));assert(dot(n,t[0])>0);};Box(box);assert(count==12);
 const auto surface=[&](float hh,int expected,bool cylinder){
  count=0;auto check=[&](Triangle t){++count;for(auto p:t){assert(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z));assert(p.x*p.x+p.y*p.y<=1.0001f);assert(std::fabs(p.z)<=hh+(cylinder?0:1)+.0001f);}
   Point center{(t[0].x+t[1].x+t[2].x)/3,(t[0].y+t[1].y+t[2].y)/3,(t[0].z+t[1].z+t[2].z)/3};
   Point normal=cross(sub(t[1],t[0]),sub(t[2],t[0]));assert(dot(normal,normal)>1e-10f);
   Point axis{0,0,std::clamp(center.z,-hh,hh)};
   if(cylinder && std::fabs(center.z)>=hh-.0001f)assert(normal.z*center.z>0);
   else assert(dot(normal,sub(center,axis))>0);
  };if(cylinder)Cylinder(1,hh,check);else Capsule(1,hh,check);assert(count==expected);
 };
 surface(1,64,true);surface(0,480,false);surface(1,512,false);
}
