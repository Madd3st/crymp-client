"""Fire lane (wb_bot_pro_fire, PR #17 Codex review): execute the REAL BotCombat::FriendlyHold, AllyInLane and
FiringPos from Code/CryGame/Bots/BotCombat.cpp with the real BotFireLane.h, in a mocked world of actors and vehicles.

Cases: a live human ally driving a vehicle in the lane, the shooter's own vehicle, a shooter on foot inside an
ally vehicle's box (fail closed), enemy/dead passengers, Instant Action (team 0), allies on foot, and the 0.5 s
hold. Also checks in the real text that Attack calls FriendlyHold only under wb_bot_pro_fire == 1 (OFF = no call).

Mock limits: the vehicle world transform is a translation only (no rotation); Kind/Classify mimic the real rules
(vehicle passengers are Invalid for targeting). Geometry unit tests: Tests/BotFireLaneTests.cpp.
"""
from pathlib import Path
import argparse
import re
import subprocess


def method(text, signature):
    start = text.index(signature)
    left = text.index('{', start)
    depth = 0
    for i in range(left, len(text)):
        depth += (text[i] == '{') - (text[i] == '}')
        if not depth:
            return text[start:i + 1]
    raise ValueError('Unclosed method: ' + signature)


CPP = r"""
#include "CryGame/Bots/BotFireLane.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>
#define WB_LOG(...) ((void)0)
using EntityId = uint32_t;
struct Vec3 {
	float x = 0, y = 0, z = 0;
	Vec3() {}
	Vec3(float a, float b, float c) : x(a), y(b), z(c) {}
	Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
	Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
	Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
	float GetLength() const { return std::sqrt(x * x + y * y + z * z); }
	Vec3 GetNormalizedSafe(const Vec3& f) const { const float l = GetLength(); return l > 1e-6f ? Vec3(x / l, y / l, z / l) : f; }
	bool IsValid() const { return std::isfinite(x) && std::isfinite(y) && std::isfinite(z); }
	bool IsZero() const { return x == 0 && y == 0 && z == 0; }
};
const Vec3 FORWARD_DIRECTION(0, 1, 0);
struct AABB { Vec3 min, max; };
struct Matrix34 {
	Vec3 t;
	Matrix34 GetInverted() const { Matrix34 m; m.t = Vec3(-t.x, -t.y, -t.z); return m; }
	Vec3 operator*(const Vec3& p) const { return p + t; }
	Vec3 TransformVector(const Vec3& v) const { return v; }
};
struct IEntity {
	Vec3 pos; AABB local; Matrix34 tm;
	Vec3 GetWorldPos() const { return pos; }
	void GetLocalBounds(AABB& b) const { b = local; }
	const Matrix34& GetWorldTM() const { return tm; }
};
struct IVehicle { IEntity hull; IEntity* GetEntity() { return &hull; } };
struct IActor {
	EntityId id = 0; int team = 0, health = 100; bool human = true; IVehicle* vehicle = nullptr; IEntity ent;
	IVehicle* GetLinkedVehicle() const { return vehicle; }
	EntityId GetEntityId() const { return id; }
	int GetHealth() const { return health; }
	IEntity* GetEntity() { return &ent; }
};
struct CActor : IActor {};
std::vector<IActor*> g_actors;
struct IActorIterator { size_t i = 0; IActor* Next() { return i < g_actors.size() ? g_actors[i++] : nullptr; } };
struct IActorIteratorPtr { std::shared_ptr<IActorIterator> p; IActorIterator* operator->() const { return p.get(); } };
struct IActorSystem { IActorIteratorPtr CreateActorIterator() { return IActorIteratorPtr{std::make_shared<IActorIterator>()}; } };
struct IGameFramework { IActorSystem s; IActorSystem* GetIActorSystem() { return &s; } };
struct CGameRules { int GetTeam(EntityId id) const { for (IActor* a : g_actors) if (a->id == id) return a->team; return 0; } };
struct CGame { IGameFramework fw; CGameRules rules; IGameFramework* GetIGameFramework() { return &fw; } CGameRules* GetGameRules() { return &rules; } };
CGame g_game; CGame* g_pGame = &g_game;
struct IFireMode { Vec3 muzzle; Vec3 GetFiringPos(const Vec3&) const { return muzzle; } };
struct IWeapon { IFireMode mode; int GetCurrentFireMode() const { return 0; } IFireMode* GetFireMode(int) { return &mode; } };
namespace WarsBots {
enum class ParticipantKind { None, Human, Bot };
enum class Relation { Self, NotParticipant, Spectator, Dead, Invalid, Ally, Enemy };
struct BotTargeting {
	static ParticipantKind Kind(IActor* a) { return a->human ? ParticipantKind::Human : ParticipantKind::Bot; }
	// Same order as BotTargeting.cpp: dead, then passengers Invalid, then the team test (team 0 = everyone hostile).
	static Relation Classify(CActor& self, IActor* other)
	{
		if (other->GetEntityId() == self.GetEntityId()) return Relation::Self;
		if (other->GetHealth() <= 0) return Relation::Dead;
		if (other->GetLinkedVehicle()) return Relation::Invalid;
		const int mine = self.team;
		return mine != 0 && mine == other->team ? Relation::Ally : Relation::Enemy;
	}
};
Vec3 Eyes(IActor& a) { return a.GetEntity()->GetWorldPos() + Vec3(0, 0, 1.7f); }
namespace {
@FIRING_POS@
@ALLY_IN_LANE@
}
class BotCombat {
public:
	float m_friendlyUntil = -100.f, m_nextNoFireLog = 0;
	bool FriendlyHold(CActor& actor, IActor& target, IWeapon& weapon, const Vec3& viewPos, const Vec3& fireDir,
		const Vec3& shotPoint, float now);
};
@FRIENDLY_HOLD@
} // namespace WarsBots
using namespace WarsBots;

static int fails = 0;
#define CHECK(c, what) do { if (!(c)) { std::printf("FAIL: %s\n", what); ++fails; } } while (0)

static IVehicle MakeVehicle(float x, float y)
{
	IVehicle v; v.hull.pos = Vec3(x, y, 0); v.hull.tm.t = Vec3(x, y, 0);
	v.hull.local.min = Vec3(-2, -1.5f, 0); v.hull.local.max = Vec3(2, 1.5f, 2.5f); // 4 x 3 x 2.5 m pickup
	return v;
}

// Shooter at the origin, muzzle 1.5 m up, enemy straight ahead; returns FriendlyHold.
static bool Shoot(BotCombat& c, CActor& shooter, IActor& enemy, float now, Vec3 muzzle = Vec3(0, 0, 1.5f), Vec3 dir = Vec3(1, 0, 0))
{
	IWeapon w; w.mode.muzzle = muzzle;
	const Vec3 shot = enemy.GetEntity()->GetWorldPos() + Vec3(0, 0, 1.5f);
	return c.FriendlyHold(shooter, enemy, w, muzzle, dir, shot, now);
}

int main()
{
	CActor shooter; shooter.id = 1; shooter.team = 1; shooter.human = false; shooter.ent.pos = Vec3(0, 0, 0);
	IActor enemy; enemy.id = 9; enemy.team = 2; enemy.human = false; enemy.ent.pos = Vec3(20, 0, 0);
	IActor ally; ally.id = 2; ally.team = 1; ally.human = true;
	IVehicle ahead = MakeVehicle(10, 0), aside = MakeVehicle(10, 6);

	{ // live human ally driving a pickup in the lane: hold (Classify alone would say Invalid)
		BotCombat c; ally.vehicle = &ahead; ally.ent.pos = ahead.hull.pos; g_actors = {&shooter, &ally, &enemy};
		CHECK(Shoot(c, shooter, enemy, 1.f), "human ally in a vehicle in the lane blocks the burst");
		CHECK(c.m_friendlyUntil > 1.49f && c.m_friendlyUntil < 1.51f, "hold set to now + 0.5 s");
		// the vehicle drives away: still held until 1.5 s, free after
		g_actors = {&shooter, &enemy};
		CHECK(Shoot(c, shooter, enemy, 1.3f), "0.5 s hold after the lane cleared");
		CHECK(!Shoot(c, shooter, enemy, 1.51f), "fire after the hold expired");
	}
	{ // vehicle beside the lane: no hold
		BotCombat c; ally.vehicle = &aside; ally.ent.pos = aside.hull.pos; g_actors = {&shooter, &ally, &enemy};
		CHECK(!Shoot(c, shooter, enemy, 1.f), "ally vehicle 6 m beside the lane does not block");
	}
	{ // the shooter's own vehicle (gunner/passenger with the ally driver): never blocks
		BotCombat c; ally.vehicle = &ahead; ally.ent.pos = ahead.hull.pos; shooter.vehicle = &ahead;
		g_actors = {&shooter, &ally, &enemy};
		CHECK(!Shoot(c, shooter, enemy, 1.f, Vec3(10, 0, 1.5f)), "own vehicle does not block its passenger's shot");
		shooter.vehicle = nullptr;
	}
	{ // shooter on foot inside an ally vehicle's box (flatbed, under a wing): fail closed in any direction
		BotCombat c; ally.vehicle = &ahead; ally.ent.pos = ahead.hull.pos; g_actors = {&shooter, &ally, &enemy};
		CHECK(Shoot(c, shooter, enemy, 1.f, Vec3(10, 0, 1.f)), "inside a foreign box, shooting forward: hold");
		BotCombat c2;
		IActor back; back.id = 8; back.team = 2; back.ent.pos = Vec3(-10, 0, 0); g_actors = {&shooter, &ally, &back};
		CHECK(Shoot(c2, shooter, back, 1.f, Vec3(10, 0, 1.f), Vec3(-1, 0, 0)), "inside a foreign box, shooting away: hold");
	}
	{ // enemy and dead passengers do not block
		BotCombat c; IActor foe; foe.id = 3; foe.team = 2; foe.vehicle = &ahead; foe.ent.pos = ahead.hull.pos;
		g_actors = {&shooter, &foe, &enemy};
		CHECK(!Shoot(c, shooter, enemy, 1.f), "enemy in a vehicle does not block");
		BotCombat c2; IActor dead = ally; dead.health = 0; dead.vehicle = &ahead; g_actors = {&shooter, &dead, &enemy};
		CHECK(!Shoot(c2, shooter, enemy, 1.f), "dead ally passenger does not block");
	}
	{ // Instant Action: team 0, nobody is an ally
		BotCombat c; CActor ia = shooter; ia.team = 0; IActor other = ally; other.team = 0; other.vehicle = &ahead;
		g_actors = {&ia, &other, &enemy};
		CHECK(!Shoot(c, ia, enemy, 1.f), "team 0: vehicle passenger is not an ally");
	}
	{ // allies on foot: on the line holds, 3 m aside fires
		BotCombat c; IActor foot = ally; foot.vehicle = nullptr; foot.ent.pos = Vec3(10, 0, 0); g_actors = {&shooter, &foot, &enemy};
		CHECK(Shoot(c, shooter, enemy, 1.f), "ally on foot in the lane blocks");
		BotCombat c2; foot.ent.pos = Vec3(10, 3, 0); g_actors = {&shooter, &foot, &enemy};
		CHECK(!Shoot(c2, shooter, enemy, 1.f), "ally on foot 3 m aside does not block");
	}
	{ // the lane stops 1 m short of the target: an enemy at the vehicle can still be shot
		BotCombat c; ally.vehicle = &ahead; ally.ent.pos = ahead.hull.pos;
		IActor close; close.id = 7; close.team = 2; close.ent.pos = Vec3(7.5f, 0, 0); g_actors = {&shooter, &ally, &close};
		CHECK(!Shoot(c, shooter, close, 1.f), "enemy 0.5 m in front of the ally vehicle is shootable");
	}
	if (fails) return 1;
	std::puts("PASS real fire-lane callers: ally driver, hold 0.5 s, own vehicle, inside foreign box, enemy/dead passengers, team 0, foot allies, range-1");
	return 0;
}
"""


def main():
    root = Path(__file__).resolve().parents[1]
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', type=Path, default=root / 'units/fire-lane-caller')
    args = ap.parse_args()
    combat = (root / 'Code/CryGame/Bots/BotCombat.cpp').read_text(encoding='utf-8-sig')
    out = args.out.resolve()
    out.relative_to(root)
    out.mkdir(parents=True, exist_ok=True)

    # OFF/ON wiring: Attack is the only caller and calls it only under wb_bot_pro_fire == 1.
    attack = method(combat, 'bool BotCombat::Attack(')
    calls = re.findall(r'FriendlyHold\(', combat)
    if len(calls) != 2 or not re.search(r'BotConfig::Get\(\)\.proFire==1 && FriendlyHold\(', attack):
        print('FAIL wiring: Attack must call FriendlyHold once, gated by wb_bot_pro_fire == 1 (definition + one call)')
        raise SystemExit(2)

    cpp = (CPP.replace('@FIRING_POS@', method(combat, 'Vec3 FiringPos('))
              .replace('@ALLY_IN_LANE@', method(combat, 'EntityId AllyInLane('))
              .replace('@FRIENDLY_HOLD@', method(combat, 'bool BotCombat::FriendlyHold(')))
    unit = out / 'caller.cpp'
    exe = out / 'caller.exe'
    unit.write_text(cpp, encoding='utf-8')
    compiled = subprocess.run(['cl', '/nologo', '/std:c++17', '/EHsc', '/I', str(root / 'Code'), str(unit),
                               '/Fo:' + str(out) + '/', '/Fe:' + str(exe)], capture_output=True)
    print((compiled.stdout + compiled.stderr).decode(errors='replace'))
    if compiled.returncode:
        raise SystemExit(compiled.returncode)
    result = subprocess.run([str(exe)], capture_output=True)
    print((result.stdout + result.stderr).decode(errors='replace'))
    raise SystemExit(result.returncode)


if __name__ == '__main__':
    main()
