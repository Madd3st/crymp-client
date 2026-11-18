"""SPEC-005b actual-caller regression for hold arrival (Codex review of #44 605137be, BLOCK XY-only arrival).

Compiles the real BotBrain::UpdatePSHold from the source tree with the real BotPsHold.h / BotTeamOrders.h /
BotPSCapture.h headers and small engine mocks (the commander/hold boundary and the navmesh), with the position already
chosen, and checks:
  1. same XY, the chosen position 3 m above the feet (upper floor / platform): keep the route (Steer to the upper
     position), never stand on the lower floor;
  2. the same position at its height: stop and watch the approach (no Steer);
  3. two floors with a detour: from beside the building the route goes to the upper position (Steer goal = it);
  4. back after combat (reason=back) under the upper position: Steer again until the height is reached, then hold;
  5. above the position (a roof over it): also not arrived;
  6. one floor, a 0.4 m snap difference: arrived (normal arrival keeps working).
Engine boundaries are mocked; this is not a game test (Codex: E:/WarsBots/QA/PR44-605137be/Tests/elevated-arrival.cpp).
"""
from pathlib import Path
import argparse
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_bot_shore_ladder_queue_caller import method  # noqa: E402

PRE = r'''
#include <cstdio>
#include <cmath>
#include <vector>
#include "CryGame/Bots/BotPsHold.h"
#include "CryGame/Bots/BotPSCapture.h"
#include "CryGame/Bots/BotTeamOrders.h"
using namespace WarsBots;
struct Vec3 { float x = 0, y = 0, z = 0; Vec3() {} Vec3(float a, float b, float c) : x(a), y(b), z(c) {}
    Vec3 operator+(const Vec3& b) const { return Vec3(x + b.x, y + b.y, z + b.z); }
    Vec3 operator-(const Vec3& b) const { return Vec3(x - b.x, y - b.y, z - b.z); }
    Vec3 operator*(float a) const { return Vec3(x * a, y * a, z * a); }
    float GetLength() const { return std::sqrt(x * x + y * y + z * z); }
    void NormalizeSafe(const Vec3& f) { const float l = GetLength(); if (l > .001f) { x /= l; y /= l; z /= l; } else *this = f; } };
struct CPlayer { unsigned GetEntityId() const { return 7; } };
struct PsHoldInfo { bool contested = false, approach = false; float ax = 0, ay = 0, az = 0; };
bool PSHoldInfo(int, int, PsHoldInfo&) { return true; }
void PSHoldClaim(int, unsigned, const float*) {}
void PSHoldOthers(int, unsigned, std::vector<float>&) {}
struct NavMeshCore { bool Ready() { return true; } bool Project(const float*, float*, float = 0, float = 0, float = 0) { return false; }
    bool FindPath(const float*, const float*, std::vector<float>&) { return false; } } g_mesh;
NavMeshCore* CurrentNavMesh() { return &g_mesh; }
bool EyeRayClear(CPlayer&, const Vec3&, const Vec3&) { return true; }
float Rand01() { return .5f; }
static int g_logs = 0;
#define WB_LOG(...) (++g_logs)
#define WBI(field, value) (m_intent.field = (value))
#define WBI_MOVETO(d_, l_) (m_intent.move = true, m_intent.destination = (d_))
struct Intent { bool move = false; Vec3 aim, destination; };
class BotBrain { public:
    int m_number = 1, m_psHoldTarget = 64; unsigned m_actorId = 7; float m_now = 10;
    bool m_psHoldCapture = false, m_psHoldHas = true, m_psHoldBack = false;
    HoldTimer m_psHoldTimer; Vec3 m_psHoldPos = Vec3(17.5f, 0, 103); float m_psHoldDir[2] = {1, 0};
    struct { float damagedUntil = -1; } m_blackboard; Intent m_intent; int steers = 0; Vec3 steerGoal;
    BotBrain() { m_psHoldTimer.Assign(10, 30); }
    void ResetPSHold(int) { m_psHoldTarget = 0; m_psHoldHas = false; m_psHoldTimer.Reset(); }
    Vec3 Steer(const Vec3&, const Vec3& g) { ++steers; steerGoal = g; return g; }
    void TravelGait(float) {}
    bool UpdatePSHold(CPlayer&, const PsCapturePoint&, int, const Vec3&);
};
'''

MAIN = r'''
int main() {
    int fails = 0;
    auto check = [&](bool c, const char* m) { if (!c) { std::printf("FAIL: %s\n", m); ++fails; } };
    CPlayer a; PsCapturePoint point; point.id = 64; point.kind = int(PsKind::Energy); point.hasDestination = true;
    // 1. same XY, upper position 3 m above: keep the route
    { BotBrain b; b.UpdatePSHold(a, point, 1, Vec3(17.5f, 0, 100));
      check(b.steers == 1 && b.m_intent.move && std::fabs(b.steerGoal.z - 103.f) < 0.01f, "under the upper position: Steer to it, no stand"); }
    // 2. at its height: hold and watch
    { BotBrain b; b.UpdatePSHold(a, point, 1, Vec3(17.5f, 0, 103));
      check(b.steers == 0 && !b.m_intent.move, "on the position: stop and watch"); }
    // 3. two floors with a detour: from beside the building the route goes to the upper position
    { BotBrain b; b.UpdatePSHold(a, point, 1, Vec3(25.f, 4.f, 100));
      check(b.steers == 1 && b.m_intent.move && std::fabs(b.steerGoal.x - 17.5f) < 0.01f && std::fabs(b.steerGoal.z - 103.f) < 0.01f, "beside the building: route to the upper floor");
      b.m_intent = Intent(); b.UpdatePSHold(a, point, 1, Vec3(18.0f, 0.5f, 100.2f));   // on the stairs below
      check(b.steers == 2 && b.m_intent.move, "below on the way: still routing");
      b.m_intent = Intent(); b.UpdatePSHold(a, point, 1, Vec3(17.8f, 0.3f, 102.7f));   // arrived upstairs
      check(b.steers == 2 && !b.m_intent.move, "upstairs: arrived, hold"); }
    // 4. back after combat under the upper position: Steer again until the height is reached
    { BotBrain b; b.m_psHoldBack = true; const int logs = g_logs;
      b.UpdatePSHold(a, point, 1, Vec3(17.5f, 0, 100));
      check(g_logs == logs + 1 && !b.m_psHoldBack, "reason=back logged once");
      check(b.steers == 1 && b.m_intent.move, "back after combat below: route up");
      b.m_intent = Intent(); b.UpdatePSHold(a, point, 1, Vec3(17.5f, 0, 103));
      check(b.steers == 1 && !b.m_intent.move, "back on the position: hold"); }
    // 5. above the position (a roof over it): not arrived
    { BotBrain b; b.UpdatePSHold(a, point, 1, Vec3(17.5f, 0, 106));
      check(b.steers == 1 && b.m_intent.move, "on a roof over the position: route down to it"); }
    // 6. one floor, 0.4 m snap difference: arrived
    { BotBrain b; b.UpdatePSHold(a, point, 1, Vec3(17.0f, 0.6f, 102.6f));
      check(b.steers == 0 && !b.m_intent.move, "one floor, small snap offset: arrived"); }
    if (fails) { std::printf("SPEC-005b hold arrival caller FAILED (%d)\n", fails); return 1; }
    std::printf("SPEC-005b hold arrival caller PASS\n");
    return 0;
}
'''


def build(repo: Path, out: Path) -> int:
    brain = (repo / 'Code/CryGame/Bots/BotBrain.cpp').read_text(encoding='utf-8-sig')
    t = PRE + method(brain, 'bool BotBrain::UpdatePSHold(') + '\n' + MAIN
    out.mkdir(parents=True, exist_ok=True)
    unit, exe = out / 'ps_hold_arrival_caller.cpp', out / 'ps_hold_arrival_caller.exe'
    unit.write_text(t, encoding='utf-8')
    comp = subprocess.run(['cl', '/nologo', '/std:c++17', '/EHsc', '/I', str(repo), '/I', str(repo / 'Code'), str(unit),
                           '/Fo:' + str(out) + '\\', '/Fe:' + str(exe)], capture_output=True)
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
    ap.add_argument('--repo', type=Path, default=root)
    ap.add_argument('--out', type=Path, default=root / 'units' / 'ps-hold-arrival-caller')
    a = ap.parse_args()
    return build(a.repo.resolve(), a.out.resolve())


if __name__ == '__main__':
    raise SystemExit(main())
