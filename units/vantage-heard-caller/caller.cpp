
#include "CryGame/Bots/BotVantage.h"
#include "CryGame/Bots/BotRoamFallback.h"
#include "CryGame/Bots/BotSniperReach.h"
#include "CryGame/Bots/BotWeaponRank.h"
#include "CryGame/Bots/BotLootClaim.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

using EntityId = unsigned;
constexpr float DEG2RAD(float d) { return d * 0.0174532925f; }
inline float cosf_wrap(float x) { return std::cos(x); }
inline float sinf_wrap(float x) { return std::sin(x); }

struct Vec3 {
    float x = 0.f, y = 0.f, z = 0.f;
    Vec3() = default;
    Vec3(float a, float b, float c) : x(a), y(b), z(c) {}
    Vec3 operator+(const Vec3& b) const { return Vec3(x + b.x, y + b.y, z + b.z); }
    Vec3 operator-(const Vec3& b) const { return Vec3(x - b.x, y - b.y, z - b.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    float GetLengthSquared() const { return x * x + y * y + z * z; }
    float GetLengthSquared2D() const { return x * x + y * y; }
    float GetLength() const { return std::sqrt(x * x + y * y + z * z); }
};

constexpr int STANCE_STAND = 0;
constexpr int STANCE_PRONE = 2;

enum class BotSkill { Beginner = 0, Normal, Veteran, Elite };
enum class BotStyle { Universal = 0, Assault, Marksman, Flanker, Defender, Hunter };
enum class BotGait { Walk = 0, Run, Sprint };

struct EntityClass {
    const char* name = "DSG1";
    const char* GetName() const { return name; }
};

struct IEntity {
    Vec3 pos;
    EntityClass cls;
    Vec3 GetWorldPos() const { return pos; }
    EntityClass* GetClass() { return &cls; }
};

struct IItem {
    IEntity entity;
    IEntity* GetEntity() { return &entity; }
};

struct IVehicle {};

struct CPlayer {
    EntityId id = 101;
    IEntity entity;
    IItem item;
    bool hasItem = true;
    IVehicle* vehicle = nullptr;
    IEntity* GetEntity() { return &entity; }
    EntityId GetEntityId() const { return id; }
    IItem* GetCurrentItem() { return hasItem ? &item : nullptr; }
    IVehicle* GetLinkedVehicle() { return vehicle; }
};

struct BotInstance {
    CPlayer* actor = nullptr;
    CPlayer* GetActor() { return actor; }
};

struct NavQueryBounds {
    float side = 1.5f, up = 1.0f, down = 2.6f;
};

struct NavMeshCore {
    bool projectOk = true;
    bool reachableOk = true;
    bool Project(const float* from, float* snap, float = 1.5f, float = 1.0f, float = 2.6f, bool = false) const {
        if (snap && from) { snap[0] = from[0]; snap[1] = from[1]; snap[2] = from[2]; }
        return projectOk;
    }
    bool Reachable(const float*, const float*, float = 2.6f, const NavQueryBounds* = nullptr) const {
        return reachableOk;
    }
};

struct Config {
    int stateCommit = 1;
    int tactics = 0;
    int psBuy = 0;
    int psDoors = 0;
    int psVehicles = 0;
    int psCapture = 0;
    int botPerception = 1;
    int weaponRank = 1;
    int vantage = 1;
    int highGround = 1;
};

struct BotConfig {
    static Config& Get() {
        static Config c;
        return c;
    }
};

namespace BotWeapons {
    EntityId FindDrop(CPlayer&, const Vec3&, float) { return 0; }
    void SelectForRange(CPlayer&, float) {}
}

struct CGameRules {
    int GetSpawnLocationCount() const { return 0; }
    EntityId GetSpawnLocation(int) const { return 0; }
};

struct ModePolicy {
    bool reviveBuy = false;
    bool captureGoals = false;
};

struct ModeInfo {
    ModePolicy Policy() const { return ModePolicy{}; }
};

struct BotMatch {
    static ModeInfo Mode(const CGameRules&) { return ModeInfo{}; }
};

struct BotNav {
    int stuckReports = 0;
    bool PickHotspot(unsigned, bool, Vec3&, const Vec3*, EntityId) { return false; }
    void ReportStuck(const Vec3&, EntityId) { ++stuckReports; }
};

struct BotManager {
    BotNav nav;
    BotNav& Nav() { return nav; }
};

struct CGame {
    CGameRules rules;
    BotManager mgr;
    CGameRules* GetGameRules() { return &rules; }
    BotManager& GetBotManager() { return mgr; }
};

struct EntitySystem {
    IEntity* GetEntity(EntityId) { return nullptr; }
};

struct Timer {
    float GetCurrTime() const { return 0.f; }
};

struct SystemEnv {
    EntitySystem entities;
    EntitySystem* pEntitySystem = &entities;
    Timer timer;
    Timer* pTimer = &timer;
};

static CGame g_gameInstance;
static CGame* g_pGame = &g_gameInstance;
static SystemEnv g_envInstance;
static SystemEnv* gEnv = &g_envInstance;

static WarsBots::VantageMap g_vantageMap;
static bool g_hasVantageMap = true;
static NavMeshCore g_navMesh;
static bool g_hasNavMesh = true;

WarsBots::VantageMap* CurrentVantage() {
    return (g_hasVantageMap && g_vantageMap.Count() > 0) ? &g_vantageMap : nullptr;
}

NavMeshCore* CurrentNavMesh() {
    return g_hasNavMesh ? &g_navMesh : nullptr;
}

struct Blackboard {
    EntityId enemy = 0;
    bool visible = false;
    float distance = 0.f;
    float waterZ = 0.f;
    bool blocked = false;
    float damagedUntil = -1.f;
};

bool WaterStartBounds(const Blackboard&, float, NavQueryBounds&) {
    return false;
}

struct CombatStub {
    bool hasHeard = false;
    Vec3 heardPos = Vec3(0.f, 0.f, 0.f);
    float heardAt = 0.f;
    bool Heard(float now, float maxAge, Vec3& out) const {
        if (!hasHeard || now - heardAt > maxAge) return false;
        out = heardPos;
        return true;
    }
};

struct ProfileStub {
    BotSkill skill = BotSkill::Elite;
    BotStyle style = BotStyle::Marksman;
};

struct TraitsStub {
    float camper = 0.9f;
    float prone = 0.8f;
};

struct IntentStub {
    bool move = false;
    Vec3 destination = Vec3(0.f, 0.f, 0.f);
    Vec3 aim = Vec3(0.f, 0.f, 0.f);
    BotGait gait = BotGait::Run;
    int stance = STANCE_STAND;
};

#define WB_LOG(...) ((void)0)
#define WBI(field, value) (m_intent.field = (value))
#define WBI_MOVETO(goal, look) do { m_intent.move = true; m_intent.destination = (goal); m_intent.aim = (look); } while (0)

using WarsBots::BotLootClaims;
using WarsBots::VantageMap;
using WarsBots::VantageOutput;
using WarsBots::VantagePhase;
using WarsBots::VSpot;
using WarsBots::BotVantage;
using WarsBots::RoamGoalZ;
using WarsBots::RoamFallbackFraction;

class BotBrain {
public:
    enum class State { SPAWNING, SEARCHING_ENEMY, MOVING_TO_WEAPON, ATTACKING };
    State m_state = State::SPAWNING;
    Blackboard m_blackboard;
    CombatStub m_combat;
    ProfileStub m_profile;
    TraitsStub m_traits;
    IntentStub m_intent;
    BotVantage m_vantage;

    float m_now = 0.f;
    float m_lootUntil = 0.f;
    EntityId m_upgradeTarget = 0;
    Vec3 m_lootPos = Vec3(0.f, 0.f, 0.f);
    int m_number = 1;
    bool m_psBought = false;
    float m_nextAliveBuy = 0.f;
    float m_lifeStartedAt = 0.f;
    Vec3 m_patrolGoal = Vec3(0.f, 0.f, 0.f);
    float m_patrolUntil = 0.f;
    bool m_detour = false;
    unsigned m_patrolStep = 0;
    unsigned m_rng = 7;
    unsigned m_life = 1;
    float m_phase = 0.f;
    float m_blockedSince = 0.f;
    float m_nextRepath = 0.f;
    float m_nextIdleSelect = 0.f;
    float m_nextHighRoll = 0.f;
    float m_highUntil = 0.f;
    bool m_highHolding = false;
    float randVal = 0.1f;

    Vec3 lastSteerGoal = Vec3(0.f, 0.f, 0.f);
    int steerCalls = 0;
    int executeCalls = 0;

    void Change(State s) { m_state = s; }
    void SetUpgradeTarget(EntityId id) { m_upgradeTarget = id; }
    bool ClaimUpgrade(EntityId) { return true; }
    bool UpdateUpgrade(BotInstance&, CPlayer&, void*, float, bool, bool) { return false; }
    bool PSTryBuy(BotInstance&, CPlayer&) { return false; }
    bool UpdatePSDoors(BotInstance&, CPlayer&, float) { return false; }
    bool UpdatePSInteract(BotInstance&, CPlayer&, float) { return false; }
    bool UpdatePSCapture(BotInstance&, CPlayer&, float) { return false; }
    Vec3 Steer(const Vec3&, const Vec3& goal) {
        ++steerCalls;
        lastSteerGoal = goal;
        return goal;
    }
    void TravelGait(float) {}
    void ExecuteIntent(CPlayer&, float) { ++executeCalls; }
    float Rand01() { return randVal; }

    void UpdateRoam(BotInstance& bot, CPlayer& actor, float decisionTime);
    bool UpdateVantage(CPlayer& actor, const Vec3& feet, float decisionTime);
};

void BotBrain::UpdateRoam(BotInstance& bot,CPlayer& actor,float decisionTime)
{
		// DS-034: SEARCHING_ENEMY used to be set here and MOVING_TO_WEAPON by UpdateUpgrade below in the same tick
		// (13,004 + 12,999 flips in playtest 3), and every Change() restarted m_stateTime (reaction, stuck checks).
		// The state is written once, after deciding whether this tick delegates to the weapon goal.
		const bool commit = BotConfig::Get().stateCommit == 1;
		if (!commit) Change(State::SEARCHING_ENEMY);
		m_blackboard.enemy=0; m_blackboard.visible=false; m_blackboard.distance=0;
		const Vec3 feet = actor.GetEntity()->GetWorldPos();
		if (UpdateVantage(actor,feet,decisionTime)) { Change(State::SEARCHING_ENEMY); return; } // owner 25.10: snipers hold roofs
		if (BotConfig::Get().tactics==1 && m_now<m_lootUntil && !m_upgradeTarget)
		{
			// fragmovies: take the dead enemy's gun/ammo before moving on
			const EntityId drop=BotWeapons::FindDrop(actor,m_lootPos,6.0f);
			m_lootUntil=0;
			// Shore 03.11: up to 4 shooters of one victim went for the same drop; the first one claims it
			if (drop && ClaimUpgrade(drop))
			{
				SetUpgradeTarget(drop); // DS-097: the budget starts in UpdateUpgrade when the bot actually walks to it
				WB_LOG(2,"[WarsBots] #%d LOOT item=%u at (%.1f,%.1f,%.1f) actor=%u.",m_number,drop,m_lootPos.x,m_lootPos.y,m_lootPos.z,actor.GetEntityId());
			}
		}
		if (BotConfig::Get().tactics==1 && m_upgradeTarget && UpdateUpgrade(bot,actor,nullptr,decisionTime,false,true)) return;
		if (commit) Change(State::SEARCHING_ENEMY);
		// DS-048 #3: PS-1 capture goal; roam only runs with no visible enemy, so combat still wins.
		// C-139 (owner: first money goes to a weapon at once; C-137: 0 buys because nobody died): also buy alive,
		// every 2 s for the first 20 s of a life; stock WarsBotsBuy refuses outside a buy zone.
		if (BotConfig::Get().psBuy==1 && !m_psBought && m_now>=m_nextAliveBuy && g_pGame && g_pGame->GetGameRules() &&
			BotMatch::Mode(*g_pGame->GetGameRules()).Policy().reviveBuy && m_now-m_lifeStartedAt<20.f)
		{ m_nextAliveBuy=m_now+2.f; if (PSTryBuy(bot,actor)) m_psBought=true; }
		// DS-067d: doors before capture (a closed spawn door starved the scan), capture before vehicles (unchanged).
		if (BotConfig::Get().psDoors==1 && UpdatePSDoors(bot,actor,decisionTime)) return;
		// DS-067f: a bot already in a vehicle must leave through the vehicle path before a capture goal runs from the
		// driver seat; the opportunistic enter search still runs after capture, so only the exit branch preempts.
		if (BotConfig::Get().psVehicles==1 && actor.GetLinkedVehicle() && UpdatePSInteract(bot,actor,decisionTime)) return;
		if (BotConfig::Get().psCapture==1 && g_pGame && g_pGame->GetGameRules() && BotMatch::Mode(*g_pGame->GetGameRules()).Policy().captureGoals && UpdatePSCapture(bot,actor,decisionTime)) return;
		if (BotConfig::Get().psVehicles==1 && UpdatePSInteract(bot,actor,decisionTime)) return;
		// C-037 hearing: a gunshot 6..50 m away that nobody can see is worth a look - roam toward it for a while.
		Vec3 heard;
		if (BotConfig::Get().botPerception==1 && m_vantage.Phase()!=VantagePhase::Travel && m_combat.Heard(m_now,4.0f,heard) &&
			(heard-feet).GetLengthSquared2D()>36.0f && (heard-feet).GetLengthSquared2D()<2500.0f &&
			(heard-m_patrolGoal).GetLengthSquared2D()>25.0f)
		{
			NavMeshCore* mesh = CurrentNavMesh();
			const float from[3] = {feet.x, feet.y, feet.z}, to[3] = {heard.x, heard.y, heard.z};
			float snap[3];
			NavQueryBounds water;
			const bool waterStart = WaterStartBounds(m_blackboard, feet.z, water);
			const NavQueryBounds* startBounds = waterStart ? &water : nullptr;
			const bool projected = !mesh || (startBounds
				? mesh->Project(from, snap, startBounds->side, startBounds->up, startBounds->down, true)
				: mesh->Project(from, snap));
			if (!mesh || !projected || mesh->Reachable(from, to, 2.6f, startBounds))
			{
				m_patrolGoal = heard; m_patrolUntil = m_now + 8.0f; m_detour = false;
				WB_LOG(2, "[WarsBots] #%d HEARD gunshot at (%.1f,%.1f,%.1f), investigating.", m_number, heard.x, heard.y, heard.z);
			}
		}
		if (m_vantage.Phase() != VantagePhase::Travel &&
			(m_now >= m_patrolUntil || (m_patrolGoal-feet).GetLengthSquared2D() < 4.0f))
		{
			// Roam between the map's IA spawn points (public map knowledge, like a human's),
			// in a per-bot order. After a blocked leg, one short local detour leg; SafeStep gates every move.
			CGameRules* rules = g_pGame->GetGameRules();
			const int count = rules ? rules->GetSpawnLocationCount() : 0;
			++m_patrolStep;
			IEntity* waypoint = nullptr;
			// Every other leg, go where experience on this map says fights happen.
			Vec3 hotspot(0,0,0);
			bool picked = false;
			const bool safe = m_profile.style == BotStyle::Marksman || m_profile.style == BotStyle::Defender;
			NavMeshCore* mesh = CurrentNavMesh();
			const float from[3] = {feet.x, feet.y, feet.z};
			float snap[3];
			// DS-087: project the swimming start with the bounded water box; dry floors keep the default box.
			NavQueryBounds water;
			const bool waterStart = WaterStartBounds(m_blackboard, feet.z, water);
			const NavQueryBounds* startBounds = waterStart ? &water : nullptr;
			const bool projected = !mesh || (startBounds
				? mesh->Project(from, snap, startBounds->side, startBounds->up, startBounds->down, true)
				: mesh->Project(from, snap));
			if (mesh && !projected) mesh = nullptr; // off the mesh: keep the old goal choice
			auto reachable = [&](const Vec3& p) { const float q[3] = {p.x, p.y, p.z}; return !mesh || mesh->Reachable(from, q, 2.6f, startBounds); };
			if (!m_detour && (m_patrolStep & 1) && g_pGame->GetBotManager().Nav().PickHotspot(m_rng + m_patrolStep, safe, hotspot, &feet, actor.GetEntityId()) &&
				(hotspot - feet).GetLengthSquared2D() > 64.0f && reachable(hotspot))
			{ m_patrolGoal = hotspot; m_patrolUntil = m_now + 30.0f; picked = true; }
			else if (count && !m_detour)
				for (int tries = 0; tries < count && !waypoint; ++tries, ++m_patrolStep)
				{
					IEntity* spawn = gEnv->pEntitySystem->GetEntity(rules->GetSpawnLocation(int((m_patrolStep * 7u + unsigned(m_number) * 3u) % unsigned(count))));
					if (spawn && (spawn->GetWorldPos() - feet).GetLengthSquared2D() > 64.0f && reachable(spawn->GetWorldPos())) waypoint = spawn;
				}
			if (picked) {}
			else if (waypoint)
			{
				m_patrolGoal = waypoint->GetWorldPos();
				m_patrolUntil = m_now + 30.0f;
			}
			else
			{
				const float angle = (m_patrolStep * 2.399963f) + m_phase;
				const Vec3 local = feet + Vec3(cosf(angle),sinf(angle),0) * (6.0f + (m_patrolStep%3)*2.0f);
				// DS-082 B1: the local fallback is not a path request. Walk only the reachable fraction of the leg
				// (C145 US t173..179 steered through the hull and logged NAV fallback/stuck); without a mesh keep the
				// old full-leg goal.
				float fraction = 1.0f;
				// DS-092: the local water leg is sampled and committed at the gated native water plane. The path goal is
				// projected with the dry box (up 1.0), so a goal at the swimmer's feet z is unreachable even in open
				// water; dry callers keep feet.z exactly (waterStart is false).
				const float goalZ = RoamGoalZ(mesh && waterStart, feet.z, m_blackboard.waterZ);
				if (mesh)
				{
					bool sampled[4];
					for (int i = 0; i < 4; ++i)
					{
						Vec3 p = feet + (local - feet) * (1.0f - 0.25f * float(i));
						p.z = goalZ;
						sampled[i] = reachable(p);
					}
					fraction = RoamFallbackFraction(true, sampled);
				}
				if (fraction > 0.f)
				{
					Vec3 goal = feet + (local - feet) * fraction;
					goal.z = goalZ;
					m_patrolGoal = goal;
				}
				else m_patrolGoal = feet;   // fraction 0: hold this leg at the feet, as before
				m_patrolUntil = m_now + 4.0f;
			}
			m_detour = false;
		}
		{ const Vec3 step=Steer(feet,m_patrolGoal); WBI_MOVETO(step,step+Vec3(0,0,1.3f)); }
		TravelGait((m_patrolGoal-feet).GetLength());
		ExecuteIntent(actor,decisionTime);
		if (bot.GetActor()!=&actor) return;
		if (m_blackboard.blocked && m_now - m_blockedSince > 1.5f && m_blockedSince > 0)
		{
			g_pGame->GetBotManager().Nav().ReportStuck(feet,actor.GetEntityId()); // learned: route around this spot next time
			m_detour=true; m_patrolUntil=m_now; m_blockedSince=0; m_nextRepath=0;
		}
		else if (m_blackboard.blocked && m_blockedSince <= 0) m_blockedSince=m_now;
		else if (!m_blackboard.blocked) m_blockedSince=0;

		return;

}

bool BotBrain::UpdateVantage(CPlayer& actor,const Vec3& feet,float decisionTime)
{
	VantageMap* map=CurrentVantage();
	// C-104 (C-100 probes, C-098 "no sniper ever held"): vantage below starts only with a DSG1/Gauss CURRENT, but every
	// pickup and fight re-selected the SCAR/FY71 at 20 m -> deadlock. Idle sniper-minded bots re-judge at 60 m.
	// DS-026: Defenders (camper 0.6) hold too; own timer so a fight right after it re-selects at once.
	if (BotConfig::Get().weaponRank==1 && BotConfig::Get().vantage==1 && m_traits.camper>=0.5f && m_now>=m_nextIdleSelect)
	{ m_nextIdleSelect=m_now+2.0f; BotWeapons::SelectForRange(actor,WarsBots::IdleHoldRange(true)); }
	IItem* held=actor.GetCurrentItem();
	const char* cls=held ? held->GetEntity()->GetClass()->GetName() : "";
	// wb_bot_high_ground: Veteran/Elite sometimes take the height with any rifle (owner fights from roofs)
	if (map && BotConfig::Get().highGround==1 && m_profile.skill>=BotSkill::Veteran && m_now>=m_nextHighRoll && m_now>=m_highUntil)
	{
		m_nextHighRoll=m_now+20.0f;
		if (Rand01()<(m_profile.skill==BotSkill::Elite?0.45f:0.25f))
		{ m_highUntil=m_now+70.0f; m_highHolding=false; WB_LOG(2,"[WarsBots] #%d HIGH GROUND start actor=%u.",m_number,actor.GetEntityId()); }
	}
	const bool high=map && BotConfig::Get().highGround==1 && m_now<m_highUntil;
	const bool sniper=high || (map && m_traits.camper>=0.5f &&   // T-8: Marksman ~0.9, Defender ~0.6
		(!strcmp(cls,"DSG1") || !strcmp(cls,"GaussRifle")));
	if (!sniper) { if (m_vantage.Phase()!=VantagePhase::None) m_vantage.Reset(map,m_number); return false; }
	if (m_now<m_blackboard.damagedUntil) m_vantage.OnHit(m_now);
	NavMeshCore* mesh=CurrentNavMesh();
	const float from[3]={feet.x,feet.y,feet.z};
	float snap[3];
	const bool onMesh=mesh && mesh->Project(from,snap);
	const VantageOutput o=m_vantage.Update(*map,m_number,m_now,feet.x,feet.y,feet.z,m_rng+unsigned(m_life),
		[&](const VSpot& s){ const float q[3]={s.x,s.y,s.z}; return !onMesh || mesh->Reachable(from,q); });
	if (o.relocated) WB_LOG(2,"[WarsBots] #%d VANTAGE relocate actor=%u.",m_number,actor.GetEntityId());
	// DS-041: the owner holds a high spot 8-15 s (median 10.8), not a sniper's 25-45 s
	if (high && o.phase==VantagePhase::Hold && !m_highHolding)
	{ m_highHolding=true; m_highUntil=std::min(m_highUntil,m_now+8.0f+7.0f*Rand01()); }
	if (o.phase==VantagePhase::None) return false;
	if (o.phase==VantagePhase::Travel)
	{
		if ((m_patrolGoal-Vec3(o.x,o.y,o.z)).GetLengthSquared()>1.0f)
			WB_LOG(2,"[WarsBots] #%d VANTAGE travel to (%.1f,%.1f,%.1f) actor=%u.",m_number,o.x,o.y,o.z,actor.GetEntityId());
		m_patrolGoal=Vec3(o.x,o.y,o.z); m_patrolUntil=m_now+5.0f; m_detour=false;
		return false;   // the normal roam path walks there
	}
	// hold: stand on the spot and watch the best view direction (the look policy sees no path -> keeps this view)
	const Vec3 eye=feet+Vec3(0,0,1.6f);
	const float yaw=DEG2RAD(o.lookYaw);
	if (o.move) { const Vec3 step=Steer(feet,Vec3(o.x,o.y,o.z)); WBI_MOVETO(step,eye+Vec3(sinf(yaw),cosf(yaw),0)*30.0f); WBI(gait,BotGait::Walk); }
	else { WBI(move,false); WBI(aim,eye+Vec3(sinf(yaw),cosf(yaw),-0.05f)*30.0f); if (m_traits.prone>0.5f) m_intent.stance=STANCE_PRONE; }
	ExecuteIntent(actor,decisionTime);
	return true;
}

static bool Same2D(const Vec3& a, const Vec3& b, float eps = 1e-3f) {
    return std::fabs(a.x - b.x) < eps && std::fabs(a.y - b.y) < eps && std::fabs(a.z - b.z) < eps;
}

int main(int argc, char** argv) {
    const bool expectOld = (argc > 1 && std::strcmp(argv[1], "old") == 0);

    // Single roof sniper spot at (40, 0, 145) with view 120, yaw 90.
    const char* spotsText =
        "# header\n"
        "40 0 145 4 0 0 0 120 90\n";
    const Vec3 roofSpot(40.f, 0.f, 145.f);
    const Vec3 gunshot(0.f, 20.f, 136.f); // 20 m from feet (0,0,136), > 5 m from roofSpot

    // Case 1 & 2: Active VantagePhase::Travel with a qualifying Heard gunshot in the same UpdateRoam tick.
    {
        g_vantageMap.Load(spotsText);
        BotConfig::Get() = Config{};
        CPlayer actor;
        actor.entity.pos = Vec3(0.f, 0.f, 136.f);
        actor.item.entity.cls.name = "DSG1";
        BotInstance bot{&actor};
        BotBrain brain;
        brain.m_now = 10.f;
        brain.m_combat.hasHeard = true;
        brain.m_combat.heardAt = 9.5f;
        brain.m_combat.heardPos = gunshot;

        brain.UpdateRoam(bot, actor, 0.1f);

        if (brain.m_vantage.Phase() != VantagePhase::Travel) {
            std::puts("FAIL: expected VantagePhase::Travel to be active");
            return 10;
        }
        const bool keptVantageGoal = Same2D(brain.m_patrolGoal, roofSpot) && Same2D(brain.lastSteerGoal, roofSpot);
        const bool overwrittenByHeard = Same2D(brain.m_patrolGoal, gunshot) && Same2D(brain.lastSteerGoal, gunshot);

        if (expectOld) {
            if (!overwrittenByHeard) {
                std::puts("FAIL: expected pre-fix revision to overwrite Vantage Travel goal with Heard gunshot");
                return 11;
            }
            std::puts("PASS [1/5]: pre-fix VantagePhase::Travel lost m_patrolGoal to Heard gunshot (RED reproduced)");
            return 0;
        }
        if (!keptVantageGoal) {
            std::puts("FAIL: active VantagePhase::Travel lost m_patrolGoal to Heard gunshot");
            return 12;
        }
        std::puts("PASS [2/5]: post-fix active VantagePhase::Travel preserved m_patrolGoal against Heard gunshot");
    }

    // Case 3: Without active Vantage (VantagePhase::None), Heard gunshot investigation works as before.
    {
        g_vantageMap.Load(spotsText);
        BotConfig::Get() = Config{};
        BotConfig::Get().vantage = 0;
        BotConfig::Get().highGround = 0;
        CPlayer actor;
        actor.entity.pos = Vec3(0.f, 0.f, 136.f);
        actor.item.entity.cls.name = "SCAR";
        BotInstance bot{&actor};
        BotBrain brain;
        brain.m_traits.camper = 0.0f;
        brain.m_patrolGoal = Vec3(100.f, 100.f, 136.f);
        brain.m_patrolUntil = 50.f;
        brain.m_now = 10.f;
        brain.m_combat.hasHeard = true;
        brain.m_combat.heardAt = 9.5f;
        brain.m_combat.heardPos = gunshot;

        brain.UpdateRoam(bot, actor, 0.1f);

        if (brain.m_vantage.Phase() != VantagePhase::None) {
            std::puts("FAIL: expected VantagePhase::None when vantage/highGround are off");
            return 20;
        }
        if (!Same2D(brain.m_patrolGoal, gunshot) || !Same2D(brain.lastSteerGoal, gunshot) ||
            std::fabs(brain.m_patrolUntil - 18.f) > 1e-3f) {
            std::puts("FAIL: Heard gunshot did not update m_patrolGoal when Vantage was inactive");
            return 21;
        }
        std::puts("PASS [3/5]: without active Vantage, Heard gunshot investigation works normally");
    }

    // Case 4a: After Vantage is cancelled (e.g. sniper eligibility ends), Heard investigation works again.
    {
        g_vantageMap.Load(spotsText);
        BotConfig::Get() = Config{};
        BotConfig::Get().highGround = 0;
        CPlayer actor;
        actor.entity.pos = Vec3(0.f, 0.f, 136.f);
        actor.item.entity.cls.name = "DSG1";
        BotInstance bot{&actor};
        BotBrain brain;
        brain.m_now = 10.f;
        brain.m_combat.hasHeard = true;
        brain.m_combat.heardAt = 9.5f;
        brain.m_combat.heardPos = gunshot;

        // Tick 1: enters VantagePhase::Travel toward roofSpot; Heard is suppressed.
        brain.UpdateRoam(bot, actor, 0.1f);
        if (brain.m_vantage.Phase() != VantagePhase::Travel || !Same2D(brain.m_patrolGoal, roofSpot)) {
            std::puts("FAIL: Tick 1 did not start Vantage Travel");
            return 30;
        }

        // Tick 2: bot switches to SCAR (no longer eligible for sniper vantage -> UpdateVantage resets m_vantage).
        actor.item.entity.cls.name = "SCAR";
        brain.m_now = 11.f;
        brain.m_combat.heardAt = 10.8f;
        brain.UpdateRoam(bot, actor, 0.1f);

        if (brain.m_vantage.Phase() != VantagePhase::None) {
            std::puts("FAIL: expected UpdateVantage to reset m_vantage to None after losing eligibility");
            return 31;
        }
        if (!Same2D(brain.m_patrolGoal, gunshot) || !Same2D(brain.lastSteerGoal, gunshot)) {
            std::puts("FAIL: Heard gunshot was not investigated after Vantage cancellation");
            return 32;
        }
    }

    // Case 4b: After Vantage completes Hold and leaves (e.g. after 2 kills or travel timeout with spot banned),
    // Heard investigation works again.
    {
        g_vantageMap.Load(spotsText);
        BotConfig::Get() = Config{};
        BotConfig::Get().highGround = 0;
        CPlayer actor;
        actor.entity.pos = Vec3(0.f, 0.f, 136.f);
        actor.item.entity.cls.name = "DSG1";
        BotInstance bot{&actor};
        BotBrain brain;
        brain.m_now = 10.f;
        brain.m_combat.hasHeard = true;
        brain.m_combat.heardAt = 9.5f;
        brain.m_combat.heardPos = gunshot;

        // Start Travel at t=10
        brain.UpdateRoam(bot, actor, 0.1f);
        if (brain.m_vantage.Phase() != VantagePhase::Travel) return 33;

        // Arrive at roofSpot at t=15 -> transitions to Hold (UpdateVantage returns true)
        actor.entity.pos = roofSpot;
        brain.m_now = 15.f;
        brain.UpdateRoam(bot, actor, 0.1f);
        if (brain.m_vantage.Phase() != VantagePhase::Hold) {
            std::puts("FAIL: expected VantagePhase::Hold upon arrival at roofSpot");
            return 34;
        }

        // Score two kills from Hold -> next tick relocates, bans the spot cluster, and with no other spot
        // returns to VantagePhase::None, re-enabling Heard investigation!
        brain.m_vantage.OnKill();
        brain.m_vantage.OnKill();
        actor.entity.pos = Vec3(0.f, 0.f, 136.f);
        brain.m_now = 16.f;
        brain.m_combat.heardAt = 15.8f;
        brain.UpdateRoam(bot, actor, 0.1f);

        if (brain.m_vantage.Phase() != VantagePhase::None) {
            std::puts("FAIL: expected VantagePhase::None after Hold relocation with all spots banned");
            return 35;
        }
        if (!Same2D(brain.m_patrolGoal, gunshot) || !Same2D(brain.lastSteerGoal, gunshot)) {
            std::puts("FAIL: Heard gunshot was not investigated after Vantage Hold completion");
            return 36;
        }
        std::puts("PASS [4/5]: after Vantage cancellation or Hold completion, Heard investigation is active again");
    }

    // Case 5: Real Travel timeout (> 40 s without reaching the spot) with no alternative reachable spot
    // ends Travel via BotVantage::Update and restores Heard gunshot investigation in UpdateRoam.
    {
        g_vantageMap.Load(spotsText);
        BotConfig::Get() = Config{};
        BotConfig::Get().highGround = 0;
        CPlayer actor;
        actor.entity.pos = Vec3(0.f, 0.f, 136.f);
        actor.item.entity.cls.name = "DSG1";
        BotInstance bot{&actor};
        BotBrain brain;
        brain.m_now = 10.f;
        brain.m_combat.hasHeard = true;
        brain.m_combat.heardAt = 9.5f;
        brain.m_combat.heardPos = gunshot;

        // 1. Start Travel toward the only available spot at t=10; Heard is suppressed while travelling.
        brain.UpdateRoam(bot, actor, 0.1f);
        if (brain.m_vantage.Phase() != VantagePhase::Travel ||
            brain.m_vantage.SpotIndex() != 0 ||
            !Same2D(brain.m_patrolGoal, roofSpot) ||
            !Same2D(brain.lastSteerGoal, roofSpot)) {
            std::puts("FAIL: Case 5 failed to start Travel to roofSpot");
            return 40;
        }

        // Still before the 40 s Travel limit (t=49.9, elapsed 39.9 s <= 40 s): Travel stays active.
        brain.m_now = 49.9f;
        brain.m_combat.heardAt = 49.5f;
        brain.UpdateRoam(bot, actor, 0.1f);
        if (brain.m_vantage.Phase() != VantagePhase::Travel ||
            !Same2D(brain.m_patrolGoal, roofSpot) ||
            !Same2D(brain.lastSteerGoal, roofSpot)) {
            std::puts("FAIL: Case 5 Travel ended before the 40 s timeout");
            return 41;
        }

        // 2 & 3. Do not reach the spot; advance time past the real 40 s Travel limit (t=50.5, elapsed 40.5 s > 40 s)
        // with no alternative spot in VantageMap (spot 0 is banned by BotVantage::Update on timeout).
        brain.m_now = 50.5f;
        brain.m_combat.heardAt = 50.2f;
        brain.UpdateRoam(bot, actor, 0.1f);

        // 4 & 5. Confirm Travel ended and fresh qualifying Heard updated m_patrolGoal and Steer target.
        if (brain.m_vantage.Phase() != VantagePhase::None || brain.m_vantage.SpotIndex() != -1) {
            std::puts("FAIL: expected VantagePhase::None and SpotIndex -1 after 40 s Travel timeout with no alternative spot");
            return 42;
        }
        if (!Same2D(brain.m_patrolGoal, gunshot) ||
            !Same2D(brain.lastSteerGoal, gunshot) ||
            std::fabs(brain.m_patrolUntil - 58.5f) > 1e-3f) {
            std::puts("FAIL: fresh Heard gunshot did not update m_patrolGoal and Steer target after Travel timeout");
            return 43;
        }
        std::puts("PASS [5/6]: after 40 s Travel timeout with no alternative spot, Travel ends and Heard updates m_patrolGoal and Steer");
    }

    // Case 6: Bot in VantagePhase::Travel approaching within 2.0 m horizontally of the spot
    // (e.g. stairs underneath at |dz| >= 1.5 m, or entering the 1.2..2.0 m ring around spot).
    // The random spawn/hotspot roam picker ((m_patrolGoal-feet).GetLengthSquared2D() < 4.0f)
    // must NOT overwrite m_patrolGoal while in VantagePhase::Travel.
    {
        g_vantageMap.Load(spotsText);
        BotConfig::Get() = Config{};
        BotConfig::Get().highGround = 0;
        CPlayer actor;
        // Start away at (0, 0, 136)
        actor.entity.pos = Vec3(0.f, 0.f, 136.f);
        actor.item.entity.cls.name = "DSG1";
        BotInstance bot{&actor};
        BotBrain brain;
        brain.m_now = 10.f;

        // Tick 1: start Travel toward roofSpot (40, 0, 145)
        brain.UpdateRoam(bot, actor, 0.1f);
        if (brain.m_vantage.Phase() != VantagePhase::Travel || !Same2D(brain.m_patrolGoal, roofSpot)) {
            std::puts("FAIL: Case 6 did not start Vantage Travel");
            return 50;
        }

        // Tick 2: bot climbs stairs underneath roofSpot at (38.5, 0, 139) -> 1.5 m 2D (< 2.0 m, len2D^2 = 2.25 < 4.0)
        // and dz = 6.0 m (> 1.5 m, so BotVantage::Update remains in Travel, not Hold).
        actor.entity.pos = Vec3(38.5f, 0.f, 139.f);
        brain.m_now = 12.f;
        brain.UpdateRoam(bot, actor, 0.1f);

        if (brain.m_vantage.Phase() != VantagePhase::Travel) {
            std::puts("FAIL: expected VantagePhase::Travel to remain active while under roofSpot");
            return 51;
        }
        if (!Same2D(brain.m_patrolGoal, roofSpot) || !Same2D(brain.lastSteerGoal, roofSpot)) {
            std::puts("FAIL: m_patrolGoal was overwritten by roam picker while approaching vantage spot in Travel phase");
            return 52;
        }
        std::puts("PASS [6/6]: VantagePhase::Travel preserved m_patrolGoal within 2m 2D approach radius of spot");
    }

    return 0;
}
