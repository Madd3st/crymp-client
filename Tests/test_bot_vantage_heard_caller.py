"""Execute actual BotBrain::UpdateRoam + BotBrain::UpdateVantage with real BotVantage.h.

Verifies the priority contract between VantagePhase::Travel and Heard gunshot investigation
in UpdateRoam using the real C++ methods extracted from Code/CryGame/Bots/BotBrain.cpp:
  1. Pre-fix (--revision ed72987c --expect old): active VantagePhase::Travel loses m_patrolGoal
     and Steer target to a qualifying Heard gunshot in the same UpdateRoam tick.
  2. Post-fix (--expect new): active VantagePhase::Travel preserves its vantage m_patrolGoal
     and Steer target when a qualifying Heard gunshot is present.
  3. Without active Vantage (VantagePhase::None), Heard gunshot investigation still updates
     m_patrolGoal and Steer target.
  4. After Vantage finishes or is cancelled (ineligible/expired, travel timeout, or hold end),
     Heard gunshot investigation is available again.
"""
from pathlib import Path
import argparse
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
#include "CryGame/Bots/BotVantage.h"
#include "CryGame/Bots/BotRoamFallback.h"
#include "CryGame/Bots/BotSniperReach.h"
#include "CryGame/Bots/BotWeaponRank.h"
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

struct SystemEnv {
    EntitySystem entities;
    EntitySystem* pEntitySystem = &entities;
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

@ACTUAL_ROAM@

@ACTUAL_VANTAGE@

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
            std::puts("PASS [1/4]: pre-fix VantagePhase::Travel lost m_patrolGoal to Heard gunshot (RED reproduced)");
            return 0;
        }
        if (!keptVantageGoal) {
            std::puts("FAIL: active VantagePhase::Travel lost m_patrolGoal to Heard gunshot");
            return 12;
        }
        std::puts("PASS [2/4]: post-fix active VantagePhase::Travel preserved m_patrolGoal against Heard gunshot");
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
        std::puts("PASS [3/4]: without active Vantage, Heard gunshot investigation works normally");
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
        std::puts("PASS [4/4]: after Vantage cancellation or completion, Heard investigation is active again");
    }

    return 0;
}
"""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--repo', type=Path, default=Path(__file__).resolve().parents[1])
    ap.add_argument('--revision')
    ap.add_argument('--out', type=Path)
    ap.add_argument('--expect', choices=['new', 'old'], default='new')
    args = ap.parse_args()

    root = args.repo.resolve()
    out = (args.out or root / 'units/vantage-heard-caller').resolve()
    out.mkdir(parents=True, exist_ok=True)

    if args.revision:
        source = subprocess.check_output(
            ['git', '-C', str(root), 'show', args.revision + ':Code/CryGame/Bots/BotBrain.cpp']
        ).decode('utf-8-sig')
    else:
        source = (root / 'Code/CryGame/Bots/BotBrain.cpp').read_text(encoding='utf-8-sig')

    roam_code = method(source, 'void BotBrain::UpdateRoam(')
    vantage_code = method(source, 'bool BotBrain::UpdateVantage(')
    cpp = CPP.replace('@ACTUAL_ROAM@', roam_code).replace('@ACTUAL_VANTAGE@', vantage_code)

    src = out / 'caller.cpp'
    exe = out / 'caller.exe'
    src.write_text(cpp, encoding='utf-8')

    comp = subprocess.run(
        ['cl', '/nologo', '/std:c++17', '/EHsc', '/I', str(root / 'Code'),
         str(src), '/Fo:' + str(out) + '/', '/Fe:' + str(exe)],
        capture_output=True
    )
    (out / 'compile.log').write_bytes(comp.stdout + comp.stderr)
    if comp.returncode:
        raise RuntimeError(
            'Compiler failure is not semantic RED: ' + (comp.stdout + comp.stderr).decode(errors='replace')
        )

    run = subprocess.run([str(exe), args.expect], capture_output=True)
    (out / 'run.log').write_bytes(run.stdout + run.stderr)
    print((run.stdout + run.stderr).decode(errors='replace').strip())
    return run.returncode


if __name__ == '__main__':
    raise SystemExit(main())
