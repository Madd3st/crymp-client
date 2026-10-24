#include "CryGame/Bots/BotAimControl.h"
#include <cassert>
using namespace WarsBots;
int main(){
 for(int s=0;s<4;s++) {
  auto p=BotProfile::Make(BotStyle::Universal,BotSkill(s));
  assert(BotAimControl::ErrorRadius(p,.1f)>=p.aimErrorFloor);
  assert(BotAimControl::ErrorRadius(p,100.f)<=p.aimErrorMax);
  const float alpha=BotAimControl::Alpha(p,.1f);assert(alpha>0 && alpha<1);
  float tracked=0;tracked+=(10-tracked)*alpha;assert(tracked>0 && tracked<10);
  for(int i=0;i<20;i++)tracked+=(10-tracked)*alpha;
  assert(tracked>9.9f && tracked<=10);
  assert(BotAimControl::Alpha(p,0)==0);
  unsigned heads=0;for(unsigned i=0;i<1000;i++)heads+=BotAimControl::PreferHead(p,"SOCOM",15,i);
  assert(heads>0 && heads<200);
  assert(!BotAimControl::PreferHead(p,"SOCOM",2,0));
  assert(!BotAimControl::PreferHead(p,"Shotgun",15,0));
 }
 auto assault=BotProfile::Make(BotStyle::Assault,BotSkill::Elite);
 auto marksman=BotProfile::Make(BotStyle::Marksman,BotSkill::Elite);
 assert(assault.trackingLag==marksman.trackingLag && assault.headPreference==marksman.headPreference);
}
