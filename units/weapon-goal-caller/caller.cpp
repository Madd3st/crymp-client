
    #include "CryGame/Bots/BotWeaponGoal.h"
    #include "CryGame/Bots/BotGoalArbiter.h"
    #include <cstdio>
    #include <cstdint>
    #define WB_LOG(...) ((void)0)
    using EntityId=uint32_t;
    using WarsBots::ArbiterGoal;
    struct CPlayer {};
    struct BotConfig {int sniperReach=0,firstPrimary=1;static BotConfig& Get(){static BotConfig c;return c;}};
    struct BotWeapons {static bool HoldsSniper(CPlayer&){return false;}};
    class BotBrain {
    public:
     EntityId m_upgradeTarget=0,m_upgradeTracked=0,candidate=77;
     float m_upgradeBest=1e9f,m_upgradeBestAt=0,m_upgradePathSig=0,m_upgradeSince=0,m_now=1,m_nextUpgradeSearch=0;
     int m_number=1,scans=0;
     float PickupRange(){return 20;}
     EntityId UpgradeCandidate(CPlayer&,bool,bool,bool){++scans;return candidate;}
     void SetUpgradeTarget(EntityId);
     void ReleaseClaims(){}
     bool TryAcquireUpgradeGoal(CPlayer&);
    };
    void BotBrain::SetUpgradeTarget(EntityId id)
{
	if (m_upgradeTarget!=id)
		WB_LOG(2,"[WarsBots] #%d WEAPON GOAL previous=%u target=%u event=%s.",m_number,m_upgradeTarget,id,id?"acquire":"clear");
	if (m_upgradeTarget && !id) ReleaseClaims(); // given up, picked up or worthless: free it for the others now
	m_upgradeTarget = id;
	m_upgradeTracked = 0; m_upgradeBest = 1e9f; m_upgradeBestAt = 0; m_upgradePathSig = 0;
	m_upgradeSince = 0; // re-anchored on the first tick that actually pursues the goal
}
bool BotBrain::TryAcquireUpgradeGoal(CPlayer& actor)
{
	if (!WarsBots::MayAcquireWeaponGoal(m_upgradeTarget, m_now, m_nextUpgradeSearch,
		WarsBots::WeaponGoalPursues(m_upgradeSince))) return false;
	const bool sniperOnly = BotConfig::Get().sniperReach==1 && PickupRange()>30.0f && !BotWeapons::HoldsSniper(actor);
	const EntityId id = UpgradeCandidate(actor, false, BotConfig::Get().firstPrimary==1 || sniperOnly, sniperOnly);
	m_nextUpgradeSearch = m_now + 3.0f;
	if (!id) { SetUpgradeTarget(0); return false; }
	SetUpgradeTarget(id);
	return true;
}
bool CancellationFromBrain(EntityId m_upgradeTarget, const WarsBots::GoalArbiter& m_arbiter, const WarsBots::ArbiterInput& in) { return m_upgradeTarget && m_arbiter.Current()==ArbiterGoal::Weapon && !m_arbiter.WeaponGoalWorthwhile(in); }

    int main(){
     CPlayer actor;BotBrain b;
     if(!b.TryAcquireUpgradeGoal(actor)||b.scans!=1||b.m_upgradeTarget!=77)return 1;
     b.m_now=2;b.candidate=88;
     if(b.TryAcquireUpgradeGoal(actor)||b.scans!=1)return 2;
     b.m_now=4;
     if(!b.TryAcquireUpgradeGoal(actor)||b.scans!=2||b.m_upgradeTarget!=88){
      std::puts("FAIL: unchosen candidate did not refresh after the search throttle");return 7;
     }
     b.m_upgradeSince=4;b.m_now=8;b.candidate=99;
     if(b.TryAcquireUpgradeGoal(actor)||b.scans!=2||b.m_upgradeTarget!=88)return 3;
     b.SetUpgradeTarget(0);b.m_now=9;b.candidate=0;
     if(b.TryAcquireUpgradeGoal(actor)||b.scans!=3||b.m_upgradeTarget!=0)return 4;
     b.m_now=10;b.candidate=100;
     if(b.TryAcquireUpgradeGoal(actor)||b.scans!=3)return 5;
     b.m_now=12;
     if(!b.TryAcquireUpgradeGoal(actor)||b.scans!=4||b.m_upgradeTarget!=100)return 6;
     WarsBots::GoalArbiter loot,upgrade;WarsBots::ArbiterInput in;
     in.weaponGoalAvailable=true;in.weaponGoalDistance=20;in.weaponGoalValue=1;in.heldWeaponValue=1;in.hasPrimary=true;
     if(CancellationFromBrain(77,loot,in)){std::puts("FAIL: same-class tactics loot cancelled in Roam");return 8;}
     in.hasPrimary=false;upgrade.Update(in);in.hasPrimary=true;
     if(!CancellationFromBrain(77,upgrade,in)){std::puts("FAIL: obsolete arbiter upgrade not cancelled");return 9;}
     std::puts("PASS real Brain callers: candidate refresh, pursuit hold, empty-search throttle");return 0;
    }
    