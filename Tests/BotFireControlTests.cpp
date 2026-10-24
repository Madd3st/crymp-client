#include "CryGame/Bots/BotFireControl.h"
#include <cassert>
#include <limits>
using WarsBots::BotFireControl;
int main() {
    auto pistol=BotFireControl::Plan("Single",450,.22f,.3f,.4675f);
    assert(pistol.pulse && pistol.holdSeconds==0 && pistol.startInterval>=.24f && pistol.startInterval<.3f);
    auto beginner=BotFireControl::Plan("Single",450,.75f,.3f,.7f);
    assert(beginner.startInterval>pistol.startInterval);
    auto shotgun=BotFireControl::Plan("Shotgun",80,.22f,.2f,.5f);
    assert(shotgun.pulse && shotgun.startInterval>=.75f);
    auto sniper=BotFireControl::Plan("Single",38,.22f,.12f,.425f);
    auto gauss=BotFireControl::Plan("Single",30,.22f,.12f,.425f);
    assert(sniper.startInterval>=60.f/38 && gauss.startInterval>=2.f);
    for (auto type:{"Rapid","Automatic","Burst","unknown"}) {
        auto automatic=BotFireControl::Plan(type,700,.22f,.3f,.4675f);
        assert(!automatic.pulse && automatic.holdSeconds==.3f && automatic.startInterval==.4675f);
    }
    for (float rpm:{0.f,-1.f,std::numeric_limits<float>::quiet_NaN()})
        assert(BotFireControl::Plan("Single",rpm,.22f,.2f,.5f).startInterval>=.24f);
    auto spin=BotFireControl::Plan("Single",30,.22f,.12f,.425f,1.f);
    assert(!spin.pulse && spin.holdSeconds>=1.1f && spin.startInterval>spin.holdSeconds);
    assert(!BotFireControl::Plan(nullptr,0,.22f,.2f,.5f).pulse);
}
