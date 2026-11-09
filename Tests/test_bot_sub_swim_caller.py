"""SPEC-006a p. 4 actual-caller regression for the swim-out (Codex review of #41 736f4610, two BLOCKs).

Compiles the real BotBrain::SubSwimRescue, TickSubSwim, CancelSubSwim, SubSwimStep, SubSwimDescending,
BotLocomotion::Stop and CPlayer::SetWarsBotSwimDown from the source tree with small engine mocks and checks:
  1. a submerged bot under a hull whose start does not reach open water (the same per-bot reach that makes a PS task
     bot_unreachable) starts SUB SWIM from its own dispatch, without any Steer call, owns the tick, moves toward the
     open-water target and sets the native ACTION_CROUCH bit on the CPlayer;
  2. 30 s without a Steer call: TickSubSwim cancels (timeout), the CPlayer bit is cleared, cooldown set;
  3. BotLocomotion::Stop (BotBrain::Stop, canMove refusal) clears the CPlayer bit;
  4. flag ON -> OFF mid-swim, leaving the water, death/ResetLife pending clear: cancelled and bit cleared;
  5. physical success (the start reaches the open water again) ends the swim-out and clears the bit;
  6. a crouch bit set by anything else is never cleared by SetWarsBotSwimDown(false); a not-owned actor is untouched;
  7. flag OFF: no start.
Engine boundaries are mocked; this is not a game test.
"""
from pathlib import Path
import argparse
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_bot_shore_ladder_queue_caller import method  # noqa: E402

PRE = r'''
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() {}
    Vec3(float a, float b, float c) : x(a), y(b), z(c) {}
    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
};
enum { ACTION_CROUCH = 1 << 3, ACTION_MOVE = 1 << 1, STANCE_STAND = 0 };
struct Env { bool bServer = true; } g_env; Env* gEnv = &g_env;
struct BotManager { std::set<unsigned> owned; bool Owns(unsigned id) const { return owned.count(id) != 0; } };
struct Game { BotManager mgr; BotManager& GetBotManager() { return mgr; } } g_game; Game* g_pGame = &g_game;
static int g_logs = 0; static char g_last[256] = "";
#define WB_LOG(level, ...) (++g_logs, std::snprintf(g_last, sizeof(g_last), __VA_ARGS__))
#define WBI_MOVETO(d, l) (m_intent.move = true, m_intent.destination = (d), m_intent.aim = (l))
struct CPlayer {
    unsigned id = 1; unsigned m_actions = 0; bool m_warsBotSwimDown = false;
    unsigned GetEntityId() const { return id; }
    void SetWarsBotSwimDown(bool down);
    void SetWarsBotActions(bool, bool, int) { m_actions &= ~ACTION_MOVE; }
};
struct BotMovement { int stops = 0; void Stop(CPlayer&) { ++stops; } };
struct BotLocomotion {
    BotMovement m_movement; float m_nextLog = 0; int resets = 0;
    void Reset() { ++resets; }
    void Stop(CPlayer& actor);
};
struct NavQueryBounds { float up = 0; };
struct NavMeshCore {
    bool solidAbove = true, openWater = true, reach = false; int reachCalls = 0;
    bool SolidAbove(const float*) const { return solidAbove; }
    bool NearestOpenWater(const float* p, float, float* out) const { out[0] = 2118.6f; out[1] = 1867.3f; out[2] = 99.51f; (void)p; return openWater; }
    bool Reachable(const float*, const float*, float, const NavQueryBounds*) { ++reachCalls; return reach; }
};
static NavMeshCore g_mesh; static bool g_meshOn = true;
static NavMeshCore* CurrentNavMesh() { return g_meshOn ? &g_mesh : nullptr; }
namespace WarsBots { struct BotConfig { int subExit = 1; static BotConfig& Get() { static BotConfig c; return c; } }; }
using WarsBots::BotConfig;
struct BotBlackboard { bool swimming = true, waterMovement = true, waterValid = true; float waterZ = 99.5f; };
static bool WaterStartBounds(const BotBlackboard& bb, float feetZ, NavQueryBounds& out)
{ if (!bb.waterMovement || !bb.waterValid || bb.waterZ - feetZ <= 0.05f) return false; out.up = 2.8f; return true; }
struct BotIntent { bool move = false, swimDown = false; Vec3 destination, aim; };
struct BotBrain {
    int m_number = 3; unsigned m_actorId = 1; float m_now = 0;
    bool m_subSwim = false, m_subSwimDown = false, m_subSwimClear = false;
    float m_subSwimSince = 0, m_subSwimCooldown = 0, m_subSwimProbeAt = 0, m_nextRepath = 0, m_minRepath = 0;
    Vec3 m_subSwimTarget, m_pathGoal;
    BotBlackboard m_blackboard; BotIntent m_intent; BotLocomotion m_locomotion; int executes = 0;
    Vec3 SubSwimStep(const Vec3& feet);
    bool SubSwimDescending() const;
    void CancelSubSwim(CPlayer& actor, const char* why, const Vec3& feet);
    void TickSubSwim(CPlayer& actor, const Vec3& feet);
    bool SubSwimRescue(CPlayer& actor, float decisionTime, const Vec3& feet);
    // The real ExecuteIntent sets m_intent.swimDown = SubSwimDescending() and BotLocomotion::Execute calls
    // actor.SetWarsBotSwimDown(intent.swimDown && state.waterMovement) (wiring test checks both lines).
    void ExecuteIntent(CPlayer& actor, float) { ++executes; m_intent.swimDown = SubSwimDescending(); actor.SetWarsBotSwimDown(m_intent.swimDown && m_blackboard.waterMovement); }
};
'''

MAIN = r'''
int main() {
    int fails = 0;
    auto check = [&](bool c, const char* m) { if (!c) { std::printf("FAIL: %s\n", m); ++fails; } };
    const Vec3 pit(2116.75f, 1882.61f, 87.60f);   // Codex 59c1391: start_off_mesh under the deck
    g_game.mgr.owned = {1};
    CPlayer a; BotBrain b;

    // 1. own dispatch, no Steer: start, own the tick, move, native bit set
    b.m_now = 10.f;
    check(b.SubSwimRescue(a, 0.1f, pit), "submerged under the hull, no reach: the swim-out owns the tick");
    check(b.m_subSwim && std::strstr(g_last, "SUB SWIM start") && std::strstr(g_last, "reason=rescue"), "SUB SWIM start reason=rescue");
    check(b.m_intent.move && std::fabs(b.m_intent.destination.x - 2118.6f) < 0.01f && std::fabs(b.m_intent.destination.y - 1867.3f) < 0.01f, "moves toward the open-water target");
    check(std::fabs(b.m_intent.destination.z - pit.z) < 0.01f, "XY course at the feet height (descent is the native bit)");
    check((a.m_actions & ACTION_CROUCH) && a.m_warsBotSwimDown, "CPlayer ACTION_CROUCH set (descend under the hull)");
    // next ticks keep driving without Steer; open sky releases the bit
    b.m_now = 10.6f; g_mesh.solidAbove = false;
    check(b.SubSwimRescue(a, 0.1f, pit) && !(a.m_actions & ACTION_CROUCH), "open sky: still driving, bit released (buoyancy)");
    g_mesh.solidAbove = true; b.m_now = 11.2f; b.SubSwimRescue(a, 0.1f, pit);
    check(a.m_actions & ACTION_CROUCH, "under the hull again: bit set");

    // 2. 30 s without any Steer call: TickSubSwim cancels, the CPlayer bit is cleared
    b.m_now = 10.f + 30.2f; b.TickSubSwim(a, pit);
    check(!b.m_subSwim && std::strstr(g_last, "reason=timeout"), "timeout cancels from the per-tick check");
    check(!(a.m_actions & ACTION_CROUCH) && !a.m_warsBotSwimDown, "timeout clears the CPlayer bit");
    check(b.m_subSwimCooldown > b.m_now, "cooldown after the timeout");
    check(!b.SubSwimRescue(a, 0.1f, pit) && !b.m_subSwim, "no restart during the cooldown");

    // 3. BotLocomotion::Stop clears the CPlayer bit
    b.m_subSwimCooldown = 0; b.m_subSwimProbeAt = 0; b.m_now = 80.f;
    check(b.SubSwimRescue(a, 0.1f, pit) && (a.m_actions & ACTION_CROUCH), "restart after the cooldown");
    b.m_locomotion.Stop(a);
    check(!(a.m_actions & ACTION_CROUCH) && b.m_locomotion.m_movement.stops == 1, "BotLocomotion::Stop clears the bit");

    // 4a. flag ON -> OFF mid-swim
    b.m_now = 80.6f; b.SubSwimRescue(a, 0.1f, pit); check(a.m_actions & ACTION_CROUCH, "driving again");
    BotConfig::Get().subExit = 0; b.m_now = 80.7f; b.TickSubSwim(a, pit);
    check(!b.m_subSwim && std::strstr(g_last, "reason=flag_off") && !(a.m_actions & ACTION_CROUCH), "flag OFF: cancelled, bit cleared");
    check(!b.SubSwimRescue(a, 0.1f, pit), "flag OFF: no start");
    BotConfig::Get().subExit = 1;
    // 4b. leaving the water
    b.m_subSwimCooldown = 0; b.m_subSwimProbeAt = 0; b.m_now = 200.f; b.SubSwimRescue(a, 0.1f, pit);
    b.m_blackboard.swimming = false; b.m_now = 200.1f; b.TickSubSwim(a, pit);
    check(!b.m_subSwim && std::strstr(g_last, "reason=left_water") && !(a.m_actions & ACTION_CROUCH), "left the water: cancelled, bit cleared");
    b.m_blackboard.swimming = true;
    // 4c. death / ResetLife without the actor: the pending clear is done on the next owned update
    b.m_subSwimCooldown = 0; b.m_subSwimProbeAt = 0; b.m_now = 300.f; b.SubSwimRescue(a, 0.1f, pit);
    check(a.m_actions & ACTION_CROUCH, "driving before death");
    b.m_subSwim = false; b.m_subSwimClear = true;   // what ResetLife does
    b.CancelSubSwim(a, "death", pit);
    check(!(a.m_actions & ACTION_CROUCH) && !b.m_subSwimClear, "ResetLife pending clear: bit cleared on the next owned update");

    // 5. physical success: the start reaches the open water again
    b.m_subSwimCooldown = 0; b.m_subSwimProbeAt = 0; b.m_now = 400.f; b.SubSwimRescue(a, 0.1f, pit);
    g_mesh.reach = true; b.m_now = 400.6f;
    check(!b.SubSwimRescue(a, 0.1f, pit) && !b.m_subSwim && std::strstr(g_last, "SUB SWIM done"), "reach restored: done, the task resumes");
    check(!(a.m_actions & ACTION_CROUCH), "done clears the bit");
    // a bot that already reaches the open water never starts
    b.m_now = 401.2f; check(!b.SubSwimRescue(a, 0.1f, pit) && !b.m_subSwim, "a normal path exists: no swim-out");
    g_mesh.reach = false;
    // not under a solid: no start
    g_mesh.solidAbove = false; b.m_now = 402.f; check(!b.SubSwimRescue(a, 0.1f, pit) && !b.m_subSwim, "open sky: no swim-out");
    g_mesh.solidAbove = true;

    // 6. foreign crouch bit stays; not-owned actor untouched
    CPlayer other; other.m_actions = ACTION_CROUCH; other.id = 1;
    other.SetWarsBotSwimDown(false);
    check(other.m_actions & ACTION_CROUCH, "a crouch set elsewhere is not cleared");
    CPlayer human; human.id = 9; human.SetWarsBotSwimDown(true);
    check(!(human.m_actions & ACTION_CROUCH), "not owned: no bit");

    // 7. cold OFF: no start
    BotConfig::Get().subExit = 0; BotBrain c; CPlayer ac; c.m_now = 1.f;
    check(!c.SubSwimRescue(ac, 0.1f, pit) && !c.m_subSwim && !(ac.m_actions & ACTION_CROUCH), "cold OFF: no swim-out");

    if (fails) { std::printf("SPEC-006a swim-out caller FAILED (%d)\n", fails); return 1; }
    std::printf("SPEC-006a swim-out caller PASS\n");
    return 0;
}
'''


def build(repo: Path, out: Path) -> int:
    rd = lambda p: (repo / p).read_text(encoding='utf-8-sig')
    brain, loco, player = rd('Code/CryGame/Bots/BotBrain.cpp'), rd('Code/CryGame/Bots/BotLocomotion.cpp'), rd('Code/CryGame/Actors/Player/Player.cpp')
    real = [method(player, 'void CPlayer::SetWarsBotSwimDown('), method(loco, 'void BotLocomotion::Stop('),
            method(brain, 'Vec3 BotBrain::SubSwimStep('), method(brain, 'bool BotBrain::SubSwimDescending()'),
            method(brain, 'void BotBrain::CancelSubSwim('), method(brain, 'void BotBrain::TickSubSwim('),
            method(brain, 'bool BotBrain::SubSwimRescue(')]
    t = PRE + '\n'.join(real) + MAIN
    out.mkdir(parents=True, exist_ok=True)
    unit, exe = out / 'sub_swim_caller.cpp', out / 'sub_swim_caller.exe'
    unit.write_text(t, encoding='utf-8')
    comp = subprocess.run(['cl', '/nologo', '/std:c++17', '/EHsc', str(unit), '/Fo:' + str(out) + '\\', '/Fe:' + str(exe)], capture_output=True)
    (out / 'compile.log').write_bytes(comp.stdout + comp.stderr)
    if comp.returncode:
        sys.stdout.buffer.write(comp.stdout + comp.stderr)
        raise RuntimeError('Caller fixture must compile; compiler failure is not semantic RED')
    run = subprocess.run([str(exe)], capture_output=True)
    sys.stdout.buffer.write(run.stdout + run.stderr)
    (out / 'run.log').write_bytes(run.stdout + run.stderr)
    return run.returncode


def main() -> int:
    ap = argparse.ArgumentParser(); root = Path(__file__).resolve().parents[1]
    ap.add_argument('--out', type=Path, default=root / 'units' / 'sub-swim-caller')
    return build(root, ap.parse_args().out.resolve())


if __name__ == '__main__':
    raise SystemExit(main())
