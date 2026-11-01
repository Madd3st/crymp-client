#include "CryGame/Bots/BotNavMeshCore.h"
#include <fstream>
#include <cstdio>
#include <vector>
#include <string>
int main(int argc,char**argv){if(argc!=3)return 2;WarsBots::NavMeshCore m;std::string err;if(!m.LoadFile(argv[1],&err))return 3;
 std::ifstream in(argv[2]);int id,target;float p[3];const float nk[3]={1842.8513f,1251.f,111.27f},us[3]={2111.88f,1882.5f,103.25f};
 while(in>>id>>target>>p[0]>>p[1]>>p[2]){float o[3]={};bool on=m.Project(p,o,.2f,2.f,3.f);std::vector<float>path;const char*why=nullptr;
 bool fromNK=on&&m.FindPath(nk,o,path,&why);bool fromUS=on&&m.FindPath(us,o,path,&why);
 printf("%d\t%d\t%d\t%.6f\t%.6f\t%.6f\t%d\t%d\n",id,target,on?1:0,o[0],o[1],o[2],fromNK?1:0,fromUS?1:0);}
}
