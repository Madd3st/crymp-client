// DS-046 sniper reach / impossible Strength-link tests (engine-free).
#include "CryGame/Bots/BotSniperReach.h"
#include "CryGame/Bots/BotJumpLink.h"
#include <cassert>
#include <cstdio>
using namespace WarsBots;
static int failures=0;
#define CHECK(c) do{if(!(c)){std::printf("FAIL %s:%d %s\n",__FILE__,__LINE__,#c);++failures;}}while(0)
int main(){
    CHECK(IsSniperClass("DSG1") && IsSniperClass("GaussRifle"));
    CHECK(!IsSniperClass("SCAR") && !IsSniperClass("SOCOM") && !IsSniperClass(nullptr));
    CHECK(SniperSearchHalfHeight(true,true)==32.f);
    CHECK(SniperSearchHalfHeight(true,false)==12.f);
    CHECK(SniperSearchHalfHeight(false,true)==12.f);
    CHECK(MaxStrengthLinkRise()>=3.6f && MaxStrengthLinkRise()<3.8f);
    {
        BotJumpLink link; LinkInput in; in.now=0.1f; in.grounded=true;
        link.Start(0.f,LinkMove::Strength,JVec{0,0,0},JVec{0,0,4.3f});
        const LinkOutput o=link.Update(in);
        CHECK(o.phase==LinkPhase::Failed && o.fail==LinkFail::TooHigh);
    }
    {
        BotJumpLink link; LinkInput in; in.now=0.1f; in.grounded=true;
        link.Start(0.f,LinkMove::Strength,JVec{0,0,0},JVec{0,0,3.0f});
        const LinkOutput o=link.Update(in);
        CHECK(!(o.phase==LinkPhase::Failed && o.fail==LinkFail::TooHigh));
    }
    if(failures){std::printf("sniper_reach: %d FAILURES\n",failures);return 1;}
    std::printf("sniper_reach: all tests passed\n"); return 0;
}
