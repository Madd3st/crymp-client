#include "CryGame/Bots/BotGoalArbiter.h"
#include <cassert>
#include <cstdio>
using namespace WarsBots;

static ArbiterInput Base(float t)
{
	ArbiterInput in; in.now = t; in.weaponGoalAvailable = true; in.weaponGoalDistance = 20.f;
	in.weaponGoalValue = 1.f; in.heldWeaponValue = 0.1f; in.engageRange = 25.f;
	return in;
}

int main()
{
	// C153: a primary gained through another gameplay path makes the old pickup worthless.
	// Commitment must not keep pursuing an inferior item after its utility disappears.
	{
		GoalArbiter a; ArbiterInput in=Base(0.f);
		assert(a.Update(in).goal==ArbiterGoal::Weapon);
		in.now=0.1f;in.hasPrimary=true;in.heldWeaponValue=1.f;
		assert(a.Update(in).goal!=ArbiterGoal::Weapon);
	}
	// C153: the first arbiter tick can follow a long first-primary shortcut. A newly lost, known threat that
	// just damaged us must interrupt even before m_started; otherwise the high weapon score consumes the tick.
	{
		GoalArbiter a; ArbiterInput in=Base(1.f);
		in.damagedRecently=true; in.enemyVisible=false; in.lastSeenAge=0.3f; in.enemyDistance=20.f;
		const ArbiterDecision d=a.Update(in);
		assert(d.goal==ArbiterGoal::Fight && d.reason==ArbiterReason::Damaged);
		GoalArbiter b; in.lastSeenAge=2.f;
		assert(b.Update(in).goal==ArbiterGoal::Weapon);
		GoalArbiter c; in.lastSeenAge=1e9f;
		assert(c.Update(in).goal==ArbiterGoal::Weapon);
	}
	// 1. The flip scenario from DECISION_FLIPS.md: pistol only, enemy 40 m away, LOS toggling every 0.1 s tick.
	//    Old logic flips CHASING <-> MOVING_TO_WEAPON every tick; the arbiter must stay on Weapon.
	{
		GoalArbiter a; int switches = 0;
		for (int i = 0; i < 300; ++i)                               // 30 s at 10 Hz
		{
			ArbiterInput in = Base(i * 0.1f);
			in.enemyVisible = (i % 2) == 0; in.enemyDistance = 40.f; in.lastSeenAge = (i % 2) ? 0.1f : 0.f;
			ArbiterDecision d = a.Update(in);
			if (d.switched && i > 0) ++switches;
		}
		assert(a.Current() == ArbiterGoal::Weapon);
		assert(switches == 0);
	}
	// 2. Pistol only, enemy steps inside 15 m -> immediate Fight (interrupt), even during the weapon commitment.
	{
		GoalArbiter a; a.Update(Base(0.f));
		assert(a.Current() == ArbiterGoal::Weapon);
		ArbiterInput in = Base(0.5f); in.enemyVisible = true; in.enemyDistance = 12.f; in.lastSeenAge = 0.f;
		ArbiterDecision d = a.Update(in);
		assert(d.goal == ArbiterGoal::Fight && d.reason == ArbiterReason::EnemyClose);
	}
	// 3. Damage interrupts toward Fight; with a primary a visible enemy in range is Fight from the start.
	{
		GoalArbiter a; a.Update(Base(0.f));
		ArbiterInput in = Base(1.f); in.damagedRecently = true; in.enemyVisible = true; in.enemyDistance = 60.f;
		assert(a.Update(in).reason == ArbiterReason::Damaged);
		GoalArbiter b; ArbiterInput p = Base(0.f); p.hasPrimary = true; p.heldWeaponValue = 1.f;
		p.enemyVisible = true; p.enemyDistance = 20.f; p.lastSeenAge = 0.f;
		assert(b.Update(p).goal == ArbiterGoal::Fight);
	}
	// 4. Fight -> enemy lost: stays Fight for the debounce, then Hunt; Hunt decays to Roam after memory.
	{
		GoalArbiter a; ArbiterInput in = Base(0.f); in.hasPrimary = true; in.heldWeaponValue = 1.f;
		in.weaponGoalAvailable = false; in.enemyVisible = true; in.enemyDistance = 20.f; in.lastSeenAge = 0.f;
		a.Update(in);
		for (int i = 1; i <= 8; ++i) { in.now = i * 0.1f; in.enemyVisible = false; in.lastSeenAge = in.now; a.Update(in); }
		assert(a.Current() == ArbiterGoal::Fight);                  // 0.8 s < 1.0 s debounce
		for (int i = 9; i <= 30; ++i) { in.now = i * 0.1f; in.lastSeenAge = in.now; a.Update(in); }
		assert(a.Current() == ArbiterGoal::Hunt);
		for (int i = 31; i <= 120; ++i) { in.now = i * 0.1f; in.lastSeenAge = in.now; a.Update(in); }
		assert(a.Current() == ArbiterGoal::Roam);
	}
	// 5. Weapon taken by someone else -> GoalGone without waiting for the commitment.
	{
		GoalArbiter a; a.Update(Base(0.f));
		ArbiterInput in = Base(0.3f); in.weaponGoalAvailable = false;
		ArbiterDecision d = a.Update(in);
		assert(d.reason == ArbiterReason::GoalGone && d.goal == ArbiterGoal::Roam);
	}
	// 6. With a primary, a small upgrade is ignored, a big one (pistol-tier -> SCAR) is taken after commitment.
	{
		GoalArbiter a; ArbiterInput in = Base(0.f); in.hasPrimary = true; in.heldWeaponValue = 0.8f;
		a.Update(in); assert(a.Current() == ArbiterGoal::Roam);    // gain 0.2 < 0.3
		in.now = 2.f; in.heldWeaponValue = 0.5f; a.Update(in); assert(a.Current() == ArbiterGoal::Weapon);
	}
	std::puts("BotGoalArbiterTests OK");
	return 0;
}
