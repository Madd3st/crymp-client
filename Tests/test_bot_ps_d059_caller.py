"""D-059 actual-caller regression (Codex BLOCK 1588030a, item 3: teamHasTac must be a real native signal).

Compiles the real D059Scan and PlanConfig from Code/CryGame/Bots/BotPSCapture.cpp with the real BotTeamOrders.h and
small engine mocks (actor iterator, inventory, game rules), then feeds the resulting config into the real
PlanTeamOrders and checks:
  1. a living team member with a TACGun in the native inventory sets the team's TAC (sticky for the level): the other
     team, a dead holder and other classes do not; a later plan with that config frees the prototype defender;
  2. TAC stays after the holder loses the gun or dies (sticky, PS ORDER tac logged once);
  3. wb_bot_ps_hold 0: PlanConfig is stage 1 (d059 false, teamHasTac false) even with TAC seen;
  4. an enemy within 40 m of an owned point refreshes its contact time, a team mate or an enemy at 41 m does not.
Engine boundaries are mocked; this is not a game test.
"""
from pathlib import Path
import argparse
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_bot_shore_ladder_queue_caller import method  # noqa: E402

PRE = r'''
#include <cstdio>
#include <cstring>
#include <cmath>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "CryGame/Bots/BotTeamOrders.h"
using namespace WarsBots;
struct Vec3 { float x = 0, y = 0, z = 0; Vec3() {} Vec3(float a, float b, float c) : x(a), y(b), z(c) {}
    Vec3 operator-(const Vec3& b) const { return Vec3(x - b.x, y - b.y, z - b.z); }
    float GetLengthSquared() const { return x * x + y * y + z * z; } };
static int g_tacLogs = 0;
#define WB_LOG(level, fmt, ...) (std::strstr(fmt, "PS ORDER tac") ? ++g_tacLogs : 0)
struct PsCapturePoint { int id = 0, kind = 0, ownerTeam = 0; bool hasDestination = true; float x = 0, y = 0, z = 0; };
struct IInventory { std::map<std::string, int> items;
    int GetCountOfClass(const char* c) const { auto i = items.find(c); return i == items.end() ? 0 : i->second; } };
struct IEntity { Vec3 pos; Vec3 GetWorldPos() const { return pos; } };
struct IActor { unsigned id = 0; int health = 100, team = 0; IEntity ent; IInventory inv;
    int GetHealth() const { return health; } IEntity* GetEntity() { return &ent; } unsigned GetEntityId() const { return id; }
    IInventory* GetInventory() { return &inv; } };
static std::vector<IActor> g_actors;
struct IActorIterator { size_t i = 0; IActor* Next() { return i < g_actors.size() ? &g_actors[i++] : nullptr; } };
using IActorIteratorPtr = std::unique_ptr<IActorIterator>;
struct IActorSystem { IActorIteratorPtr CreateActorIterator() { return IActorIteratorPtr(new IActorIterator()); } } g_as;
struct IGameFramework { IActorSystem* GetIActorSystem() { return &g_as; } } g_fw;
struct CGame { IGameFramework* GetIGameFramework() { return &g_fw; } } g_game, *g_pGame = &g_game;
struct CGameRules { int GetTeam(unsigned id) const { for (const IActor& a : g_actors) if (a.id == id) return a.team; return 0; } };
struct TeamRuntime { std::map<int, float> lastEnemy; bool tac = false; };
struct Commander { TeamRuntime teams[3]; };
'''

MAIN = r'''
static IActor A(unsigned id, int team, Vec3 p, bool tacGun = false)
{ IActor a; a.id = id; a.team = team; a.ent.pos = p; if (tacGun) a.inv.items["TACGun"] = 1; return a; }
int main() {
    int fails = 0;
    auto check = [&](bool c, const char* m) { if (!c) { std::printf("FAIL: %s\n", m); ++fails; } };
    CGameRules rules; Commander c;
    std::vector<PsCapturePoint> points(1); points[0].id = 344; points[0].kind = 2; points[0].ownerTeam = 1;
    // 1. no TACGun anywhere, a dead holder, another class, the other team's gun: team 1 has no TAC
    g_actors = {A(1, 1, Vec3(500, 0, 0)), A(2, 1, Vec3(600, 0, 0), true), A(3, 1, Vec3(700, 0, 0)), A(4, 2, Vec3(800, 0, 0), true)};
    g_actors[1].health = 0; g_actors[2].inv.items["TACGun_Fleet"] = 1;
    D059Scan(c, rules, points, 1.f);
    check(!c.teams[1].tac, "dead holder / TACGun_Fleet / the other team's gun: no TAC for team 1");
    check(c.teams[2].tac && g_tacLogs == 1, "team 2 member with a TACGun: TAC for team 2, PS ORDER tac logged");
    // plan with the real PlanConfig: taken prototype, a neutral point left, 4 bots -> one defender until TAC
    std::vector<TeamTarget> t(2); t[0].id = 344; t[0].kind = PsKind::Prototype; t[0].owner = 1; t[0].reachable = true;
    t[1].id = 96; t[1].kind = PsKind::Bunker; t[1].owner = 0; t[1].reachable = true;
    std::vector<TeamBot> bots(4); for (unsigned i = 0; i < 4; ++i) bots[i].id = 100 + i;
    auto holds = [&](const TeamPlanConfig& cfg) { int h = 0; for (const TeamOrder& o : PlanTeamOrders(t, bots, 1, 0.f, TeamEvent::None, {}, cfg)) h += o.hold; return h; };
    check(holds(PlanConfig(c.teams[1], 1)) == 1, "no TAC yet: the prototype keeps one defender");
    // 2. a living team-1 member buys/picks up a TACGun: TAC, the prototype defender is free
    g_actors[0].inv.items["TACGun"] = 1;
    D059Scan(c, rules, points, 2.f);
    check(c.teams[1].tac && g_tacLogs == 2, "living member with a TACGun: TAC for team 1, logged once");
    check(PlanConfig(c.teams[1], 1).teamHasTac && holds(PlanConfig(c.teams[1], 1)) == 0, "with TAC: no prototype defender");
    g_actors[0].inv.items.clear(); g_actors[0].health = 0;
    D059Scan(c, rules, points, 3.f);
    check(c.teams[1].tac && g_tacLogs == 2, "TAC is sticky for the level, no second log");
    // 3. wb_bot_ps_hold 0: stage 1 config even with TAC seen
    const TeamPlanConfig off = PlanConfig(c.teams[1], 0);
    check(!off.d059 && !off.teamHasTac, "flag OFF: stage 1 config");
    // 4. contact time: an enemy within 40 m refreshes, a mate or an enemy at 41 m does not
    Commander k;
    g_actors = {A(5, 1, Vec3(10, 0, 0)), A(6, 2, Vec3(41, 0, 0))};
    D059Scan(k, rules, points, 5.f);
    check(k.teams[1].lastEnemy.find(344) == k.teams[1].lastEnemy.end(), "mate near, enemy at 41 m: no contact");
    g_actors[1].ent.pos = Vec3(39, 0, 0);
    D059Scan(k, rules, points, 6.f);
    check(k.teams[1].lastEnemy[344] == 6.f, "enemy at 39 m: contact time refreshed");
    if (fails) { std::printf("D-059 caller FAILED (%d)\n", fails); return 1; }
    std::printf("D-059 TAC/contact caller PASS\n");
    return 0;
}
'''


def build(repo: Path, out: Path) -> int:
    cap = (repo / 'Code/CryGame/Bots/BotPSCapture.cpp').read_text(encoding='utf-8-sig')
    t = (PRE + method(cap, 'void D059Scan(Commander& c, CGameRules& rules,') + '\n'
         + method(cap, 'TeamPlanConfig PlanConfig(const TeamRuntime& tr, int psHold)') + '\n' + MAIN)
    out.mkdir(parents=True, exist_ok=True)
    unit, exe = out / 'ps_d059_caller.cpp', out / 'ps_d059_caller.exe'
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
    ap.add_argument('--out', type=Path, default=root / 'units' / 'ps-d059-caller')
    a = ap.parse_args()
    return build(a.repo.resolve(), a.out.resolve())


if __name__ == '__main__':
    raise SystemExit(main())
