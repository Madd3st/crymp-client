// Executes the real loader; no engine/game binary. Link with the same Recast/Detour/zlib objects as the baker.
#define main RecastBakeMain
#include "recast_bake.cpp"
#undef main
#define CHECK(c) do { if(!(c)) { std::fprintf(stderr,"retain-box check failed: %s\n",#c); return 1; } } while(0)
int main(int argc,char**argv) {
 if(argc!=2)return 2;
 std::vector<float>v0,v1;std::vector<int>t0,t1,a0,a1;int d0=0,d1=0,kept=0;
 CHECK(LoadObj(argv[1],99.5f,105.f,v0,t0,d0,&a0));
 CHECK(t0.empty() && d0==4 && a0.size()==12);
 const std::vector<RetainBox> boxes={{{-1,-1,89,3,3,111}}};
 CHECK(LoadObj(argv[1],99.5f,105.f,v1,t1,d1,&a1,&boxes,&kept));
 CHECK((t1==std::vector<int>{0,1,2})); // outside room and giant crossing triangles are never admitted
 CHECK(d1==3 && kept==1);             // upper zmax still wins over the box
 CHECK(v0==v1 && a0==a1);             // water-mask source geometry is exactly unchanged
 std::puts("retain-box PASS: real inside floor kept; outside/crossing/above-max rejected; water input unchanged");
 return 0;
}
