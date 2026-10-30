"""Actual-caller regression for Shore US ship lower-bay ladder serialization & bilge-fall abort.

Compiles the real C++ methods from Code/CryGame/Bots/BotBrain.cpp, BotManager.cpp, and BotLadder.h:
  - BotBrain::ApplyLadderExecutor
  - BotBrain::UpdateLadder
  - BotBrain::CancelLadder
  - BotBrain::SteerCorners
  - BotBrain::YieldLadderTo (when present)
  - BotManager::ShouldYieldLadder (when present)

Verifies:
  1. Concurrent Shore lower-bay rush (#2 climbing ref=4693115 while #6 arrives at 1.92s):
     - Old (1ae65bb0): #6 starts m_ladder in parallel while #2 occupies the ladder, times out Grab at 5.57s
       (no_grab), and calls BanLink(4693115), permanently severing the lower bay.
     - New: #6 holds at the 2 m gate (m_ladderWaiting=true, trap reset, locomotion stopped, 0 bans) until #2
       completes LADDER done at 12.58s, then starts m_ladder with a fresh grab budget and climbs cleanly.
  2. Distance-ordered queue handoff across staggered 10 Hz ticks:
     - When #2 finishes while #4 (dist2=3.24) and #12 (dist2=1.44, closer to base) are both waiting, #4 yields
       even if its 10 Hz tick fires first, and closer #12 takes the ladder.
  3. Bilge water / off-platform drop abort (#4 at 1.85s..26.24s, pos=(2116.99,1883.08,87.84)):
     - Old (1ae65bb0): #4 stays in Approach in the bilge water (z=87.84, swim=1) until 26.24s timeout and bans
       ref=4693115.
     - New: UpdateLadder cancels immediately at 1.85s as movement_state with ban=false (0 bans).
  4. SteerCorners pre-ladder slowdown:
     - Approaching a NavLinkLadder corner within 4 m sets m_cornerSlow=true on new code (false on old), while
       NavLinkStrength/NavLinkJump keep m_cornerSlow=false.
"""
from pathlib import Path
import argparse
import subprocess
import sys


def read_source(repo: Path, rev: str, path: str) -> str:
    if rev == 'WORKTREE':
        return (repo / path).read_text(encoding='utf-8-sig')
    out = subprocess.run(['git', '-C', str(repo), 'show', f'{rev}:{path}'], capture_output=True, check=True)
    return out.stdout.decode('utf-8-sig', errors='replace')


def method(text: str, signature: str) -> str:
    start = text.index(signature)
    left = text.index('{', start)
    depth = 0
    for i in range(left, len(text)):
        if text[i] == '{':
            depth += 1
        elif text[i] == '}':
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
    raise ValueError('unclosed method: ' + signature)


CPP_TEMPLATE = r'''
#define WARSBOTS_EXPERIMENTAL_SPAWNER 1
#include "CryGame/Bots/BotLadder.h"
#include "CryGame/Bots/BotCorners.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using namespace WarsBots;
using EntityId = unsigned;
using std::max;
using std::min;

#define WB_LOG(...) ((void)0)
template<class T> T fabs_tpl(T v) { return (T)std::fabs((double)v); }

enum EStance { STANCE_STAND = 0 };
enum ENanoMode { NANOMODE_DEFENSE = 3 };
enum class BotGait { Walk, Run, Sprint };
enum class BotJump { None, Normal, Strength };
enum class BotCombatIntent { None, Aim, Fire };

struct Vec3 {
    float x = 0.f, y = 0.f, z = 0.f;
    Vec3() = default;
    Vec3(float a, float b, float c) : x(a), y(b), z(c) {}
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    float len() const { return std::sqrt(x * x + y * y + z * z); }
    float GetLength2D() const { return std::sqrt(x * x + y * y); }
    float GetLengthSquared2D() const { return x * x + y * y; }
};

struct CoherentBool {
    bool v = false;
    bool Value() const { return v; }
    void operator=(bool b) { v = b; }
};
struct CoherentInt {
    int v = 0;
    int Value() const { return v; }
    void operator=(int x) { v = x; }
};

struct SPlayerStats {
    CoherentBool isOnLadder, isFrozen;
    CoherentInt inFreefall;
    bool isExitingLadder = false, isRagDoll = false, inZeroG = false, isGrabbed = false;
    int spectatorMode = 0, flyMode = 0;
    Vec3 ladderTop, ladderBottom, ladderOrientation;
};

struct Entity {
    Vec3 p;
    Vec3 GetWorldPos() const { return p; }
};

struct CPlayer {
    enum ELadderActionType { eLAT_None, eLAT_ReachedEnd, eLAT_ExitTop, eLAT_Jump, eLAT_StrafeLeft, eLAT_StrafeRight, eLAT_Use, eLAT_Die };
    EntityId id = 1;
    Entity e;
    SPlayerStats st;
    bool ladderUsable = false;
    bool swimming = false;
    int grabs = 0, leaves = 0, stopActions = 0;
    Vec3 exitTarget;
    SPlayerStats* GetPlayerStats() { return &st; }
    Entity* GetEntity() { return &e; }
    EntityId GetEntityId() const { return id; }
    bool IsFrozen() const { return st.isFrozen.Value(); }
    void* GetLinkedEntity() { return nullptr; }
    bool ShouldSwim() const { return swimming; }
    bool IsLadderUsable() { return ladderUsable; }
    void RequestGrabOnLadder(ELadderActionType) { ++grabs; st.isOnLadder = true; }
    void RequestLeaveLadder(ELadderActionType) { ++leaves; st.isOnLadder = false; st.isExitingLadder = false; }
    void RestoreWarsBotLadderGraph() {}
    void SetWarsBotLadderExitTarget(const Vec3& t) { exitTarget = t; }
    void SetWarsBotActions(bool, bool move, int) { if (!move) ++stopActions; }
};

struct NavQueryBounds { float side = 1.5f, up = 1.0f, down = 3.0f; };
struct NavPathLink { int corner = 0, kind = 0; float a[3] = {}, b[3] = {}; unsigned long long ref = 0; };
enum NavLinkKind { NavLinkDrop = 0, NavLinkJump, NavLinkStrength, NavLinkLift, NavLinkLadder };

struct NavMeshCore {
    int bans = 0;
    unsigned long long lastBannedRef = 0;
    void BanLink(unsigned long long ref) { ++bans; lastBannedRef = ref; }
    bool Project(const float* q, float* s, float = 0.2f, float = 0.6f, float = 0.6f) const {
        s[0] = q[0]; s[1] = q[1]; s[2] = q[2];
        return true;
    }
    bool RayClear(const float*, const float*, const NavQueryBounds* = nullptr) const { return true; }
};

namespace WarsBots {
struct BotConfig {
    int ladder = 1;
    static BotConfig& Get() { static BotConfig c; return c; }
};
}

struct BotBlackboard {
    int health = 100;
    Vec3 position, velocity;
    bool grounded = true, canMove = true, suitActive = true, swimming = false, waterMovement = false;
    int mode = NANOMODE_DEFENSE;
    float energy = 100.f, waterZ = -1e6f;
    bool waterValid = false;
};

static bool WaterStartBounds(const BotBlackboard&, float, NavQueryBounds&) { return false; }

struct BotCombat {
    int stopFires = 0;
    void StopFire() { ++stopFires; }
};

struct BotIntent {
    bool move = false;
    Vec3 destination, aim;
    BotGait gait = BotGait::Run;
    BotJump jump = BotJump::None;
    int suitMode = NANOMODE_DEFENSE;
    BotCombatIntent combat = BotCombatIntent::None;
    bool linkJump = false, meshPath = false;
    float arrive = 0.8f;
    struct Why { short suitMode = 0, jump = 0, gait = 0, aim = 0, move = 0; } why;
};
#define WBI(field, value) (m_intent.field = (value), m_intent.why.field = (short)__LINE__)

struct MockLoco {
    int stops = 0, looks = 0, executes = 0, ladderExecutes = 0;
    void Stop(CPlayer&) { ++stops; }
    void Look(CPlayer&, const Vec3&) { ++looks; }
    bool Execute(CPlayer&, const BotIntent&, BotBlackboard&, float, float) { ++executes; return true; }
    bool ExecuteLadder(CPlayer&, const Vec3&, const Vec3&, float, bool) { ++ladderExecutes; return true; }
};

struct MockTrap {
    int resets = 0;
    void Reset() { ++resets; }
};

struct MockLink {
    bool active = false;
    bool Active() const { return active; }
};

struct BotBrain;
struct BotInstance {
    bool ai = true;
    BotBrain* brain = nullptr;
    bool AIEnabled() const { return ai; }
    BotBrain& Brain() const { return *brain; }
};

struct BotManager {
    std::unordered_map<EntityId, std::unique_ptr<BotInstance>> m_bots;
    const BotInstance* Find(EntityId id) const {
        auto it = m_bots.find(id);
        return it != m_bots.end() ? it->second.get() : nullptr;
    }
    bool AIEnabled(EntityId id) const;
    bool ShouldYieldLadder(unsigned long long ref, const Vec3& a, float myDist2, int myNumber) const;
};

struct MockGame {
    BotManager mgr;
    BotManager& GetBotManager() { return mgr; }
};
static MockGame g_game;
static MockGame* g_pGame = &g_game;
static NavMeshCore g_mesh;
static NavMeshCore* CurrentNavMesh() { return &g_mesh; }

struct BotBrain {
    BotLadder m_ladder;
    NavPathLink m_ladderNow;
    bool m_ladderNativeLocked = false;
    bool m_ladderCancelPending = false;
    bool m_ladderWaiting = false;
    MockLink m_link;
    std::vector<Vec3> m_path;
    std::vector<NavPathLink> m_pathLinks;
    size_t m_pathIndex = 0, m_cornerFor = size_t(-1);
    CornerPlan m_cornerPlan;
    Vec3 m_pathStart = Vec3(0, 0, 0);
    bool m_cornerSlow = false;
    bool m_meshPathActive = true;
    float m_nextRepath = 0.f, m_minRepath = 0.f, m_now = 0.f, m_trapStepUntil = 0.f;
    int m_number = 1;
    bool m_running = true, m_verbose = false;
    BotIntent m_intent;
    BotBlackboard m_blackboard;
    MockLoco m_locomotion;
    BotCombat m_combat;
    MockTrap m_trap;
    std::string lastCancelReason;
    bool lastCancelBan = false;
    int cancelCount = 0;

    const NavPathLink* LinkAtCorner(size_t corner) const {
        for (const NavPathLink& l : m_pathLinks)
            if (size_t(l.corner) == corner) return &l;
        return nullptr;
    }

    bool YieldLadderTo(unsigned long long ref, const Vec3& a, float myDist2, int myNumber) const;
    bool ApplyLadderExecutor(CPlayer& actor);
    void CancelLadderBody(CPlayer& actor, const char* why, bool ban);
    void CancelLadder(CPlayer& actor, const char* why, bool ban) {
        lastCancelReason = why ? why : "";
        lastCancelBan = ban;
        ++cancelCount;
        CancelLadderBody(actor, why, ban);
    }
    void UpdateLadder(CPlayer& actor, float dt);
    Vec3 SteerCorners(NavMeshCore& mesh, const Vec3& feet);
};

@YIELD_LADDER_TO@
@MANAGER_METHODS@
@APPLY_LADDER_EXECUTOR@
@CANCEL_LADDER@
@UPDATE_LADDER@
@STEER_CORNERS@

static void RegisterBot(EntityId id, int number, BotBrain& b, CPlayer& a, const Vec3& pos, const NavPathLink& link) {
    a.id = id;
    a.e.p = pos;
    a.st.ladderBottom = Vec3(link.a[0], link.a[1], link.a[2]);
    a.st.ladderTop = Vec3(link.b[0], link.b[1], link.b[2]);
    a.st.ladderOrientation = Vec3(0.f, 1.f, 0.f);
    b.m_number = number;
    b.m_running = true;
    b.m_meshPathActive = true;
    b.m_intent.move = true;
    b.m_blackboard.health = 100;
    b.m_blackboard.position = pos;
    b.m_blackboard.grounded = true;
    b.m_path = { Vec3(link.a[0], link.a[1], link.a[2]), Vec3(link.b[0], link.b[1], link.b[2]) };
    b.m_pathLinks = { link };
    b.m_pathIndex = 0;
    auto inst = std::make_unique<BotInstance>();
    inst->ai = true;
    inst->brain = &b;
    g_game.mgr.m_bots[id] = std::move(inst);
}

int main(int argc, char** argv) {
    const bool expectNew = argc > 1 && std::strcmp(argv[1], "new") == 0;
    int fails = 0;
    auto check = [&](bool cond, const char* msg) {
        if (!cond) { std::printf("FAIL: %s\n", msg); ++fails; }
    };
    auto expect = [&](bool newCond, const char* msg) {
        check(expectNew ? newCond : !newCond, msg);
    };

    const NavPathLink shoreLadder{ 0, NavLinkLadder, {2116.12f, 1880.62f, 90.16f}, {2117.50f, 1880.60f, 99.76f}, 4693115ull };

    // 1. Shore lower-bay rush: #2 climbs at 0.31s..12.58s while #6 arrives at 1.92s (blocked by #2 until 12.58s).
    {
        g_game.mgr.m_bots.clear();
        g_mesh = NavMeshCore();
        BotBrain b2, b6;
        CPlayer a2, a6;
        RegisterBot(65481, 2, b2, a2, Vec3(2117.25f, 1880.62f, 90.16f), shoreLadder);
        RegisterBot(65461, 6, b6, a6, Vec3(2114.50f, 1887.88f, 90.58f), shoreLadder);

        b2.m_now = 0.31f;
        check(b2.ApplyLadderExecutor(a2) && b2.m_ladder.Active(), "#2 starts Shore ship ladder at 0.31s");

        // #6 reaches the 2 m gate at 1.92s while #2 is on the ladder (a6.ladderUsable=false while #2 occupies rungs).
        a6.e.p = Vec3(2115.64f, 1881.45f, 89.55f);
        b6.m_blackboard.position = a6.e.p;
        a6.ladderUsable = false;
        b6.m_now = 1.92f;
        const bool handled = b6.ApplyLadderExecutor(a6);
        check(handled, "#6 is handled by ApplyLadderExecutor at 1.92s");
        expect(!b6.m_ladder.Active() && !b6.m_intent.move && b6.m_locomotion.stops == 1 && b6.m_trap.resets == 1,
            "#6 waits at gate while #2 owns ref=4693115");

        // Advance to 5.57s while #2 is still climbing.
        for (float t : {2.55f, 3.50f, 4.50f, 5.57f}) {
            b6.m_now = t;
            b6.m_intent.move = true;
            if (b6.m_ladder.Active()) b6.UpdateLadder(a6, 0.1f);
            else b6.ApplyLadderExecutor(a6);
        }
        expect(g_mesh.bans == 0, "ref=4693115 is NOT banned at 5.57s while #6 waits for #2");

        if (expectNew) {
            // #2 finishes LADDER done at 12.58s at the upper landing -> #6 starts with a fresh timer at 12.60s and grabs.
            b2.m_ladder.Reset();
            b2.m_ladderNow = NavPathLink();
            b2.m_pathIndex = 1;
            b2.m_blackboard.position = Vec3(2117.53f, 1880.60f, 99.81f);
            b6.m_now = 12.60f;
            b6.m_intent.move = true;
            check(b6.ApplyLadderExecutor(a6) && b6.m_ladder.Active(),
                "#6 starts ladder immediately after #2 finishes at 12.58s");
            a6.e.p = Vec3(2116.12f, 1880.62f, 90.16f);
            b6.m_blackboard.position = a6.e.p;
            a6.ladderUsable = true;
            b6.m_now = 12.70f;
            b6.UpdateLadder(a6, 0.1f);
            b6.m_now = 12.80f;
            b6.UpdateLadder(a6, 0.1f);
            check(a6.grabs == 1 && g_mesh.bans == 0, "#6 grabs free ladder with fresh timer and 0 bans");
        }
    }

    // 2. Distance-ordered queue handoff: closer #12 (d2=1.44) goes before further #4 (d2=3.24) even if #4 ticks first.
    if (expectNew) {
        g_game.mgr.m_bots.clear();
        g_mesh = NavMeshCore();
        BotBrain b2, b4, b12;
        CPlayer a2, a4, a12;
        RegisterBot(65481, 2, b2, a2, Vec3(2116.12f, 1880.62f, 90.16f), shoreLadder);
        RegisterBot(65471, 4, b4, a4, Vec3(2114.32f, 1880.62f, 90.16f), shoreLadder);  // d = 1.80m (d2 = 3.24)
        RegisterBot(65431, 12, b12, a12, Vec3(2117.32f, 1880.62f, 90.16f), shoreLadder); // d = 1.20m (d2 = 1.44)

        b2.m_now = 0.31f;
        b2.ApplyLadderExecutor(a2);
        b4.m_now = 1.15f; b4.ApplyLadderExecutor(a4);
        b12.m_now = 1.60f; b12.ApplyLadderExecutor(a12);
        check(!b4.m_ladder.Active() && !b12.m_ladder.Active(), "both #4 and #12 wait while #2 climbs");

        // #2 finishes at the upper landing; #4's 10 Hz tick fires 20ms before #12's tick.
        b2.m_ladder.Reset();
        b2.m_ladderNow = NavPathLink();
        b2.m_pathIndex = 1;
        b2.m_blackboard.position = Vec3(2117.53f, 1880.60f, 99.81f);
        b4.m_now = 12.60f; b4.m_intent.move = true;
        b4.ApplyLadderExecutor(a4);
        check(!b4.m_ladder.Active() && !b4.m_intent.move, "further #4 yields to closer waiting #12");

        b12.m_now = 12.62f; b12.m_intent.move = true;
        b12.ApplyLadderExecutor(a12);
        check(b12.m_ladder.Active(), "closer #12 starts ladder on its tick");
    }

    // 3. Shore #4 bilge water fall (pos=(2116.99,1883.08,87.84), swim=1): immediate movement_state cancel, 0 bans.
    {
        g_game.mgr.m_bots.clear();
        g_mesh = NavMeshCore();
        BotBrain b4;
        CPlayer a4;
        RegisterBot(65471, 4, b4, a4, Vec3(2114.20f, 1880.42f, 90.16f), shoreLadder);
        b4.m_now = 1.15f;
        check(b4.ApplyLadderExecutor(a4) && b4.m_ladder.Active(), "#4 starts ladder at 1.15s");

        // At 1.85s, #4 falls off the platform into the ship bilge water (z=87.84, swimming=true).
        a4.e.p = Vec3(2116.99f, 1883.08f, 87.84f);
        a4.swimming = true;
        b4.m_blackboard.position = a4.e.p;
        b4.m_blackboard.swimming = true;
        b4.m_blackboard.grounded = false;
        b4.m_now = 1.85f;
        b4.UpdateLadder(a4, 0.1f);

        if (!expectNew) {
            // On old code, #4 stays active in Approach at 1.85s and times out at 26.24s, banning the link.
            b4.m_now = 26.24f;
            b4.UpdateLadder(a4, 0.1f);
        }
        expect(b4.lastCancelReason == "movement_state" && !b4.lastCancelBan && g_mesh.bans == 0,
            "#4 bilge water fall cancels as movement_state at 1.85s without banning ref=4693115");
    }

    // 4. SteerCorners pre-ladder slowdown: NavLinkLadder within 4 m sets m_cornerSlow=true; Strength link stays false.
    {
        BotBrain b;
        b.m_path = { Vec3(2116.12f, 1880.62f, 90.16f), Vec3(2117.50f, 1880.60f, 99.76f) };
        b.m_pathLinks = { shoreLadder };
        b.m_pathIndex = 0;
        b.m_cornerSlow = false;
        const Vec3 target = b.SteerCorners(g_mesh, Vec3(2114.06f, 1880.42f, 90.12f));
        check(std::fabs(target.x - 2116.12f) < 1e-3f, "SteerCorners returns exact ladder take-off corner");
        expect(b.m_cornerSlow, "SteerCorners sets m_cornerSlow=true within 4 m of NavLinkLadder");

        if (expectNew) {
            b.m_cornerSlow = false;
            b.m_pathLinks[0].kind = NavLinkStrength;
            b.SteerCorners(g_mesh, Vec3(2114.06f, 1880.42f, 90.12f));
            check(!b.m_cornerSlow, "SteerCorners keeps m_cornerSlow=false for NavLinkStrength run-up");
        }
    }

    if (fails) {
        std::printf("Shore ladder queue caller controls FAILED (%d)\n", fails);
        return 1;
    }
    std::printf("Shore ladder queue caller controls PASS (%s)\n", expectNew ? "new" : "old");
    return 0;
}
'''


def build_and_run(repo: Path, rev: str, expect: str, out: Path) -> int:
    brain = read_source(repo, rev, 'Code/CryGame/Bots/BotBrain.cpp')
    manager = read_source(repo, rev, 'Code/CryGame/Bots/BotManager.cpp')
    ladder_h = read_source(repo, rev, 'Code/CryGame/Bots/BotLadder.h')
    corners_h = read_source(repo, rev, 'Code/CryGame/Bots/BotCorners.h')

    apply_ladder = method(brain, 'bool BotBrain::ApplyLadderExecutor(')
    cancel_ladder = method(brain, 'void BotBrain::CancelLadder(').replace(
        'void BotBrain::CancelLadder(', 'void BotBrain::CancelLadderBody(')
    update_ladder = method(brain, 'void BotBrain::UpdateLadder(')
    steer_corners = method(brain, 'Vec3 BotBrain::SteerCorners(')

    if 'bool BotBrain::YieldLadderTo(' in brain:
        yield_ladder = method(brain, 'bool BotBrain::YieldLadderTo(')
    else:
        yield_ladder = 'bool BotBrain::YieldLadderTo(unsigned long long, const Vec3&, float, int) const { return false; }'

    ai_enabled = method(manager, 'bool BotManager::AIEnabled(')
    if 'bool BotManager::ShouldYieldLadder(' in manager:
        should_yield = method(manager, 'bool BotManager::ShouldYieldLadder(')
    else:
        should_yield = 'bool BotManager::ShouldYieldLadder(unsigned long long, const Vec3&, float, int) const { return false; }'

    cpp = CPP_TEMPLATE
    cpp = cpp.replace('@YIELD_LADDER_TO@', yield_ladder)
    cpp = cpp.replace('@MANAGER_METHODS@', ai_enabled + '\n' + should_yield)
    cpp = cpp.replace('@APPLY_LADDER_EXECUTOR@', apply_ladder)
    cpp = cpp.replace('@CANCEL_LADDER@', cancel_ladder)
    cpp = cpp.replace('@UPDATE_LADDER@', update_ladder)
    cpp = cpp.replace('@STEER_CORNERS@', steer_corners)

    out.mkdir(parents=True, exist_ok=True)
    inc_dir = out / 'include' / 'CryGame' / 'Bots'
    inc_dir.mkdir(parents=True, exist_ok=True)
    (inc_dir / 'BotLadder.h').write_text(ladder_h, encoding='utf-8')
    (inc_dir / 'BotCorners.h').write_text(corners_h, encoding='utf-8')

    unit = out / 'shore_ladder_queue_caller.cpp'
    exe = out / 'shore_ladder_queue_caller.exe'
    unit.write_text(cpp, encoding='utf-8')

    comp = subprocess.run(
        ['cl', '/nologo', '/std:c++17', '/EHsc', '/I', str(out / 'include'), str(unit),
         '/Fo:' + str(out) + '\\', '/Fe:' + str(exe)],
        capture_output=True
    )
    sys.stdout.buffer.write(comp.stdout + comp.stderr)
    (out / 'compile.log').write_bytes(comp.stdout + comp.stderr)
    if comp.returncode != 0:
        raise RuntimeError('Caller fixture must compile; compiler failure is not semantic RED')

    run = subprocess.run([str(exe), expect], capture_output=True)
    sys.stdout.buffer.write(run.stdout + run.stderr)
    (out / 'run.log').write_bytes(run.stdout + run.stderr)
    return run.returncode


def main() -> int:
    ap = argparse.ArgumentParser()
    root = Path(__file__).resolve().parents[1]
    ap.add_argument('--repo', type=Path, default=root)
    ap.add_argument('--revision', default='WORKTREE')
    ap.add_argument('--expect', choices=('old', 'new'), default='new')
    ap.add_argument('--out', type=Path, default=root / 'units' / 'shore-ladder-queue-caller')
    args = ap.parse_args()
    return build_and_run(args.repo.resolve(), args.revision, args.expect, args.out.resolve())


if __name__ == '__main__':
    raise SystemExit(main())
