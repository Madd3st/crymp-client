#include <cstring>
#include <cassert>
#include <cmath>
#include "CryGame/Bots/BotProfile.h"
#include "CryGame/Bots/BotProgress.h"
using namespace WarsBots;
int main()
{
    for (int style=0;style<int(BotStyle::Count);++style)
    {
        float lastReaction=10;
        for (int tier=0;tier<int(BotSkill::Count);++tier)
        {
            auto p=BotProfile::Make(BotStyle(style),BotSkill(tier));
            assert(p.NextTargetBurst(10.0f,1.0f)==10.0f);
            assert(p.NextTargetBurst(0.0f,1.0f)>=1.0f+p.reaction);
            assert(p.reaction>=0.2f && p.reaction<=lastReaction); lastReaction=p.reaction;
            assert(p.aimErrorMax>0 && p.aimErrorPerMetre>0);
            assert(!p.CanSwitchTarget(p.targetHold-0.01f,1.0f,10.0f));
            assert(!p.CanSwitchTarget(p.targetHold+1.0f,1.0f,1.01f));
            assert(p.CanSwitchTarget(p.targetHold+1.0f,1.0f,1.0f+p.targetSwitchMargin+0.01f));
            assert(std::isfinite(p.TargetUtility(20,12,true,true,true,20,true)));
            assert(p.WeaponRange("Shotgun",1)>0 && p.WeaponRange("DSG1",1)>0);
        }
    }
    BotProgress progress;
    assert(!progress.Stalled(0,0,0,0,4));
    assert(!progress.Stalled(0,0,0,3.9f,4));
    assert(progress.Stalled(0,0,0,4.0f,4));
    progress.Reset();
    assert(!progress.Stalled(0,0,0,10,4));
    assert(!progress.Stalled(-2,0,0,13,4)); // Moving away on a valid detour is still progress.
    assert(!progress.Stalled(-2,0,0,16,4));
    assert(!progress.Stalled(-2,0,2,17,4)); // Vertical movement counts too.
    assert(!progress.Stalled(-2,0,2,1,4)); // A level clock reset starts a new window.
    auto b=BotProfile::Make(BotStyle::Universal,BotSkill::Beginner);
    auto e=BotProfile::Make(BotStyle::Universal,BotSkill::Elite);
    assert(!b.useSpeed && e.useSpeed && b.retreatHealth<e.retreatHealth);
    assert(b.TargetUtility(10,12,false,false,false,100,true)>b.TargetUtility(40,12,true,true,true,100,true));
    assert(e.TargetUtility(20,20,true,false,true,100,true)>e.TargetUtility(20,20,true,false,false,100,true));
    assert(e.TargetUtility(20,20,true,false,false,100,true)>e.TargetUtility(20,20,false,false,false,100,true));
    assert(e.TargetUtility(20,20,true,true,false,20,true)<e.TargetUtility(20,20,true,false,false,20,true));
    assert(e.TargetUtility(20,20,true,false,false,100,true)>e.TargetUtility(2,20,true,false,false,100,true));
    assert(e.WeaponRange("Shotgun",1)<e.WeaponRange("DSG1",1));
    auto assault=BotProfile::Make(BotStyle::Assault,BotSkill::Elite);
    auto marksman=BotProfile::Make(BotStyle::Marksman,BotSkill::Elite);
    assert(assault.WeaponRange("SCAR",1)<marksman.WeaponRange("SCAR",1));
    assert(assault.reaction==marksman.reaction && assault.targetJudgement==marksman.targetJudgement);
    return 0;
}
