
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
Vec3 FiringPos(IWeapon& weapon, const Vec3& probableHit, const Vec3& fallback)
{
	IFireMode* mode = weapon.GetFireMode(weapon.GetCurrentFireMode());
	const Vec3 pos = mode ? mode->GetFiringPos(probableHit) : Vec3(0,0,0);
	return pos.IsValid() && !pos.IsZero() ? pos : fallback;
}
EntityId AllyInLane(CActor& actor, const Vec3& muzzle, const Vec3& fireDirection, float range)
{
	if (!muzzle.IsValid() || !fireDirection.IsValid()) return 0;
	const Vec3 dir = fireDirection.GetNormalizedSafe(FORWARD_DIRECTION);
	IActorIteratorPtr iterator = g_pGame->GetIGameFramework()->GetIActorSystem()->CreateActorIterator();
	CGameRules* rules = g_pGame->GetGameRules();
	const int team = rules ? rules->GetTeam(actor.GetEntityId()) : 0;
	while (IActor* other = iterator->Next())
	{
		// Classify keeps passengers out of targeting (Invalid); for the fire line an ally in a vehicle is the vehicle box.
		IVehicle* vehicle = other->GetLinkedVehicle();
		if (vehicle && team != 0 && other->GetEntityId() != actor.GetEntityId() && other->GetHealth() > 0 && vehicle != actor.GetLinkedVehicle() &&
			BotTargeting::Kind(other) != ParticipantKind::None && rules->GetTeam(other->GetEntityId()) == team)
		{
			// Local bounds in the vehicle frame (a world AABB of a turned tank or a VTOL is far too big); the lane stops
			// 1 m short of the target, so an enemy at the vehicle (C4, hijack) can still be shot (PR #17 review).
			AABB box;
			IEntity* hull = vehicle->GetEntity();
			hull->GetLocalBounds(box);
			const Matrix34 toLocal = hull->GetWorldTM().GetInverted();
			const Vec3 p = toLocal*muzzle, d = toLocal.TransformVector(dir).GetNormalizedSafe(FORWARD_DIRECTION);
			if (box.min.x < box.max.x && LaneHitsBox(LaneVec{p.x,p.y,p.z}, LaneVec{d.x,d.y,d.z}, range-1.0f,
				LaneVec{box.min.x,box.min.y,box.min.z}, LaneVec{box.max.x,box.max.y,box.max.z}))
				return other->GetEntityId();
			continue;
		}
		if (BotTargeting::Classify(actor, other) != Relation::Ally) continue;
		const Vec3 feet = other->GetEntity()->GetWorldPos(), head = Eyes(*other);
		if (FriendlyInLane(LaneVec{muzzle.x,muzzle.y,muzzle.z}, LaneVec{dir.x,dir.y,dir.z}, range,
			LaneVec{feet.x,feet.y,feet.z}, LaneVec{head.x,head.y,head.z}))
			return other->GetEntityId();
	}
	return 0;
}
}
class BotCombat {
public:
	float m_friendlyUntil = -100.f, m_nextNoFireLog = 0;
	bool FriendlyHold(CActor& actor, IActor& target, IWeapon& weapon, const Vec3& viewPos, const Vec3& fireDir,
		const Vec3& shotPoint, float now);
};
bool BotCombat::FriendlyHold(CActor& actor, IActor& target, IWeapon& weapon, const Vec3& viewPos, const Vec3& fireDir,
	const Vec3& shotPoint, float now)
{
	if (now < m_friendlyUntil)
	{
		if (now>=m_nextNoFireLog) { m_nextNoFireLog=now+0.5f; WB_LOG(2,"[WarsBots] NOFIRE actor=%u target=%u reason=friendly_hold.",actor.GetEntityId(),target.GetEntityId()); }
		return true;
	}
	const Vec3 aimEnd = viewPos + fireDir.GetNormalizedSafe(FORWARD_DIRECTION)*(shotPoint-viewPos).GetLength();
	const Vec3 firingPos = FiringPos(weapon,aimEnd,viewPos);
	if (const EntityId ally=AllyInLane(actor,firingPos,aimEnd-firingPos,(aimEnd-firingPos).GetLength()))
	{
		m_friendlyUntil=now+0.5f;
		if (now>=m_nextNoFireLog) { m_nextNoFireLog=now+0.5f; WB_LOG(2,"[WarsBots] NOFIRE actor=%u target=%u reason=friendly ally=%u.",actor.GetEntityId(),target.GetEntityId(),ally); }
		return true;
	}
	return false;
}
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
