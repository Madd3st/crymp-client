// Diagnostic tessellation only; no collision/navigation changes.
#pragma once
#include <array>
#include <vector>
#include <cmath>
namespace WarsBots::GeometryMesh {
struct Point {float x,y,z;};
using Triangle=std::array<Point,3>;
// Z-axis surfaces of revolution. Profile runs bottom to top; outward winding.
template<class Sink> void Lathe(const std::vector<std::array<float,2>>& profile,Sink sink){
 constexpr int segments=16;constexpr float pi=3.14159265358979323846f;
 for(size_t j=1;j<profile.size();++j)for(int i=0;i<segments;++i){
  float a=2*pi*i/segments,b=2*pi*(i+1)/segments;
  const auto point=[](const auto& p,float angle){return Point{p[1]*std::cos(angle),p[1]*std::sin(angle),p[0]};};
  Point p=point(profile[j-1],a),q=point(profile[j-1],b),r=point(profile[j],b),s=point(profile[j],a);
  if(profile[j-1][1]>1e-6f)sink(Triangle{p,q,r});
  if(profile[j][1]>1e-6f)sink(Triangle{p,r,s});
 }
}
template<class Sink> void Cylinder(float radius,float halfHeight,Sink sink){
 Lathe({{{-halfHeight,0}},{{-halfHeight,radius}},{{halfHeight,radius}},{{halfHeight,0}}},sink);
}
template<class Sink> void Capsule(float radius,float halfHeight,Sink sink){
 constexpr float pi=3.14159265358979323846f;std::vector<std::array<float,2>> profile;
 for(int i=0;i<=8;++i){float a=-pi/2+pi*i/16;profile.push_back({-halfHeight+radius*std::sin(a),i==0?0.f:radius*std::cos(a)});}
 if(halfHeight>0)profile.push_back({halfHeight,radius});
 for(int i=1;i<=8;++i){float a=pi*i/16;profile.push_back({halfHeight+radius*std::sin(a),i==8?0.f:radius*std::cos(a)});}
 Lathe(profile,sink);
}
template<class Sink> void Box(Sink sink){
 const Point v[8]={{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
 const int f[12][3]={{0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,1,5},{0,5,4},{3,7,6},{3,6,2},{0,4,7},{0,7,3},{1,2,6},{1,6,5}};
 for(auto& t:f)sink(Triangle{v[t[0]],v[t[1]],v[t[2]]});
}
}
