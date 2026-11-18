
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
void D059Scan(Commander& c, CGameRules& rules, const std::vector<PsCapturePoint>& points, float now)
{
	std::vector<std::pair<int, Vec3>> alive;
	IActorIteratorPtr actors = g_pGame->GetIGameFramework()->GetIActorSystem()->CreateActorIterator();
	while (IActor* a = actors->Next())
	{
		if (a->GetHealth() <= 0 || !a->GetEntity()) continue;
		const int t = rules.GetTeam(a->GetEntityId());
		if (t != 1 && t != 2) continue;
		alive.push_back(std::make_pair(t, a->GetEntity()->GetWorldPos()));
		IInventory* inv = a->GetInventory();
		if (!c.teams[t].tac && inv && inv->GetCountOfClass("TACGun") > 0)
		{
			c.teams[t].tac = true;
			WB_LOG(1, "[WarsBots] PS ORDER tac team=%d by=%u t=%.1f.", t, unsigned(a->GetEntityId()), now);
		}
	}
	for (const PsCapturePoint& p : points)
	{
		if (p.ownerTeam < 1 || p.ownerTeam > 2 || !p.hasDestination) continue;
		const Vec3 at(p.x, p.y, p.z);
		for (const auto& a : alive)
			if (a.first != p.ownerTeam && (a.second - at).GetLengthSquared() < 40.f * 40.f) { c.teams[p.ownerTeam].lastEnemy[p.id] = now; break; }
	}
}
TeamPlanConfig PlanConfig(const TeamRuntime& tr, int psHold)
{
	TeamPlanConfig cfg;
	cfg.d059 = psHold == 1;
	cfg.teamHasTac = cfg.d059 && tr.tac;
	return cfg;
}

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
