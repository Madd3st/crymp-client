#include "CryGame/Bots/BotHotspot.h"
#include <cassert>
#include <vector>
#include <chrono>
#include <cstdio>
using namespace WarsBots;
int main() {
 auto oddOnly=[](std::size_t i){return i==3?50.f:0.f;};
 for(unsigned seed=0;seed<100;seed++) {
  assert(BotHotspot::Pick(seed,8,2,oddOnly)==-1);
  assert(BotHotspot::Pick(seed,8,1,oddOnly)==3);
 }
 assert(BotHotspot::Pick(1,0,1,oddOnly)==-1);
 assert(BotHotspot::Pick(1,8,0,oddOnly)==-1);
 std::vector<float> weights(7633);
 for(std::size_t i=0;i<weights.size();++i)weights[i]=i%13==0?float(i%23):0;
 auto weight=[&](std::size_t i){return weights[i];};
 const int fixed=BotHotspot::Pick(42,weights.size(),1,weight);
 assert(fixed>=0 && weights[fixed]>.5f);
 assert(fixed==BotHotspot::Pick(42,weights.size(),1,weight));
 volatile unsigned checksum=0;
 for(std::size_t stride:{2u,1u}) {
  const auto start=std::chrono::steady_clock::now();
  for(unsigned seed=0;seed<5000;seed++)checksum+=BotHotspot::Pick(seed,weights.size(),stride,weight);
  const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/5000;
  std::printf("hotspot nodes=%zu stride=%zu mean_us=%.3f queries=5000 checksum=%u\n",weights.size(),stride,us,unsigned(checksum));
 }
}
