// DS-045 probe runner tests: scenario JSON -> setup commands -> log metrics -> assertions (engine-free).
#include "CryGame/Bots/BotProbeRunner.h"
#include <cassert>
#include <cstdio>
#include <string>

using namespace WarsBots::Probe;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

static const char* kScenario = R"json({
  "schema": 1,
  "id": "exploit_demo",
  "map": "multiplayer/ia/steelmill",
  "seed": 4242,
  "duration_s": 20,
  "bots": [ { "skill": "veteran", "style": "assault", "spawn": [1, 2, 3], "ai": true } ],
  "dummies": [ { "pos": [4, 5, 6], "hp": 100, "team": 0, "shoot_every_s": 0.8, "weapon": "SCAR" } ],
  "items": [ { "class": "SCAR", "pos": [7, 8, 9] } ],
  "bursts": [ { "actor": 1, "shots": 3, "interval": 0.2 } ],
  "peeks": [ { "actor": 1, "pos": [1, 2, 3], "expose_s": 0.4, "hide_s": 1.0 } ],
  "cloaks": [ { "actor": 1, "on": false } ],
  "cvars": { "wb_bot_navmesh": 1, "wb_bot_goal": true, "wb_bot_name": "probe" },
  "goal": [10, 11, 12],
  "assertions": [
    { "metric": "kills", "op": "eq", "value": 0 },
    { "metric": "time_to_first_shot", "op": "gt", "value": 0.5 }
  ]
})json";

int main()
{
    Scenario s;
    std::string err;
    CHECK(ParseScenario(kScenario, s, &err));
    CHECK(s.id == "exploit_demo");
    CHECK(s.map == "multiplayer/ia/steelmill");
    CHECK(s.seed == 4242);
    CHECK(s.duration == 20);
    CHECK(s.bots.size() == 1 && s.bots[0].skill == "veteran" && s.bots[0].style == "assault");
    CHECK(s.bots[0].x == 1 && s.bots[0].y == 2 && s.bots[0].z == 3 && s.bots[0].ai);
    CHECK(s.dummies.size() == 1 && s.dummies[0].hp == 100 && s.dummies[0].weapon == "SCAR");
    CHECK(s.items.size() == 1 && s.items[0].cls == "SCAR");
    CHECK(s.bursts.size() == 1 && s.bursts[0].shots == 3);
    CHECK(s.peeks.size() == 1 && s.peeks[0].expose == 0.4);
    CHECK(s.cloaks.size() == 1 && !s.cloaks[0].on);
    CHECK(s.cvars.size() == 3 && s.cvars["wb_bot_navmesh"] == "1" && s.cvars["wb_bot_goal"] == "1");
    CHECK(s.hasGoal && s.goal[0] == 10 && s.goal[1] == 11 && s.goal[2] == 12);
    CHECK(s.assertions.size() == 2 && s.assertions[0].metric == "kills" && s.assertions[0].value == 0);

    const std::vector<std::string> cmds = BuildSetupCommands(s);
    bool hasSpawn = false, hasDummy = false, hasItem = false, hasGoal = false, hasRun = false, hasNav = false, hasName = false;
    bool hasBurst = false, hasPeek = false, hasCloak = false;
    for (size_t i = 0; i < cmds.size(); ++i)
    {
        hasSpawn |= cmds[i] == "wb_probe_spawn 1 2 3 veteran assault 1";
        hasDummy |= cmds[i] == "wb_probe_dummy 4 5 6 100 0 0.8 SCAR";
        hasItem  |= cmds[i] == "wb_probe_item SCAR 7 8 9";
        hasGoal  |= cmds[i] == "wb_probe_goal 10 11 12 1.5";
        hasRun   |= cmds[i] == "wb_probe_run exploit_demo 20";
        hasNav   |= cmds[i] == "wb_bot_navmesh 1";
        hasName  |= cmds[i] == "wb_bot_name probe";
        hasBurst |= cmds[i] == "wb_probe_burst 1 3 0.2";
        hasPeek  |= cmds[i] == "wb_probe_peek 1 1 2 3 0.4 1";
        hasCloak |= cmds[i] == "wb_probe_cloak 1 0";
    }
    CHECK(hasSpawn && hasDummy && hasItem && hasGoal && hasRun && hasNav && hasName && hasBurst && hasPeek && hasCloak);
    CHECK(cmds.back() == "wb_probe_run exploit_demo 20");

    Metrics m;
    m.Begin(0.0);
    m.ObserveLine("0.00 [WarsBots] PROBE GOAL reached x=10 y=11 z=12 elapsed=0.0");
    m.ObserveLine("0.60 [WarsBots] FIRE SHOT shooter=1 target=2 projectile=3 weapon=4 class=SCAR mode=Single rpm=450.00 clip=29 remote=0 distance=12.5.");
    m.ObserveLine("0.70 [WarsBots] FIRE HIT phase=before-Lua target=2 shooter=1 projectile=3 weapon=4 class=SCAR actor=1 human=0 health=100 suit=3 energy=100.00 invulnerable=0 distance=12.5 material=6 head=1 part=1 damage=50.000.");
    m.ObserveLine("0.70 [WarsBots] FIRE HIT phase=after-Lua target=2 shooter=1 projectile=3 weapon=4 class=SCAR actor=1 human=0 health=50 suit=3 energy=50.00 invulnerable=0 distance=12.5 material=6 head=1 part=1 damage=50.000.");
    m.ObserveLine("1.00 [WarsBots] #1 combat kill shooter=1 target=2 via stock GameRules::KillPlayer.");
    m.ObserveLine("2.00 [WarsBots] #2 death observed EntityId=1 health=-4 lastHitShooter=2 KillPlayerObserved=1.");
    m.ObserveLine("3.00 [WarsBots] TRAP RECOVERY level=2 actor=1 age=8.00 pos=(0,0,0) blocked=1 speed=0.00 grounded=1 sink=0.");
    m.ObserveLine("3.10 [WarsBots] #1 LINK start kind=jump a=(0,0,0) b=(1,1,1) energy=100 actor=1.");
    m.ObserveLine("4.00 [WarsBots] TACTIC SAMPLE actor=1 skill=veteran style=assault weapon=SCAR suit=3 distance=18.50 action=ATTACKING target=2 visible=1.");
    m.ObserveLine("5.00 [WarsBots] MOTION INTENT: gait=1 strafe=0 requestedBase=2.50 actual=0.00 lateral=0.00 facingDot=0.00 blocked=1.");
    m.ObserveLine("6.00 [WarsBots] BODY LIVING actor=1 epoch=1 spawn=1 phase=now flying=0 stuck=1 squashed=0.");

    CHECK(m.shots == 1);
    CHECK(m.hits == 1); // the after-Lua duplicate is ignored
    CHECK(m.kills == 1);
    CHECK(m.deaths == 1);
    CHECK(m.trapMax == 2);
    CHECK(m.linkAttempts == 1);
    CHECK(m.stalls == 2); // the trap line also carries blocked=1
    CHECK(m.stuckEvents == 1);
    CHECK(std::fabs(m.firstShot - 0.60) < 1e-6);
    CHECK(std::fabs(m.goalTime - 0.0) < 1e-6);
    CHECK(std::fabs(m.Accuracy() - 100.0) < 1e-6);
    CHECK(std::fabs(m.HitDistanceP50() - 12.5) < 1e-6);
    CHECK(std::fabs(m.TargetDistanceP50() - 18.5) < 1e-6);

    const std::vector<AssertionResult> results = EvaluateAll(m, s.assertions);
    CHECK(results.size() == 2);
    CHECK(!results[0].pass); // kills == 1, assertion says 0
    CHECK(results[1].pass);  // first shot 0.6 > 0.5
    CHECK(std::fabs(results[0].actual - 1.0) < 1e-6);

    // DS-059/C143: per-bot kills, class pickups with first time, LINK done/failed, landing_z, unreachable.
    Metrics c143;
    c143.Begin(10.0);
    for (int i = 0; i < 3; ++i)
        c143.ObserveLine("10.50 [WarsBots] #1 combat kill shooter=1 target=2 via stock GameRules::KillPlayer.");
    c143.ObserveLine("10.60 [WarsBots] #2 combat kill shooter=2 target=3 via stock GameRules::KillPlayer.");
    c143.ObserveLine("11.00 [WarsBots] #1 LINK done kind=jump at=(1318.00,1369.00,138.90) actor=1.");
    c143.ObserveLine("11.50 [WarsBots] #1 LINK failed kind=jump reason=missed_landing at=(1318.00,1369.00,137.00) actor=1.");
    c143.ObserveLine("12.00 [WarsBots] REAL PICKUP actor=1 item=42 acquired=1 class=DSG1.");
    c143.ObserveLine("13.00 [WarsBots] REAL PICKUP actor=1 item=43 acquired=1 class=GaussRifle.");
    c143.ObserveLine("13.10 [WarsBots] REAL PICKUP actor=1 item=44 acquired=0 class=DSG1.");
    c143.ObserveLine("14.00 [WarsBots] GOAL SKIP unreachable kind=weapon actor=1 id=42 skipped=1.");
    CHECK(c143.Value("kills:1") == 3 && c143.Value("kills:2") == 1);
    CHECK(c143.Value("pickup:DSG1") == 1 && c143.Value("pickup:GaussRifle") == 1);
    CHECK(std::fabs(c143.FirstEventTime("pickup:DSG1") - 2.0) < 1e-6);
    CHECK(std::fabs(c143.FirstEventTime("pickup:GaussRifle") - 3.0) < 1e-6);
    CHECK(c143.Value("link:jump:done") == 1 && c143.Value("link:failed") == 1);
    CHECK(std::fabs(c143.Value("landing_z") - 138.90) < 1e-3);
    CHECK(c143.Value("unreachable") == 1);

    Assertion ratio; ratio.metric = "kills:1"; ratio.op = "ge_ratio_of"; ratio.other = "kills:2"; ratio.value = 3.0;
    AssertionResult rr;
    CHECK(Evaluate(c143, ratio, rr) && rr.pass && std::fabs(rr.actual - 3.0) < 1e-6);
    Assertion ratioFail = ratio; ratioFail.value = 4.0;
    CHECK(!Evaluate(c143, ratioFail, rr) && !rr.pass);

    // Legacy ratio semantics (Run-WarsBotsProbe.ps1:125): positive:0 passes, 0:0 fails, 2:1 fails.
    Metrics zeroDen;
    zeroDen.Begin(0.0);
    zeroDen.ObserveLine("0.50 [WarsBots] #1 combat kill shooter=1 target=2 via stock GameRules::KillPlayer.");
    zeroDen.ObserveLine("0.60 [WarsBots] #1 combat kill shooter=1 target=2 via stock GameRules::KillPlayer.");
    Assertion zeroRatio = ratio; zeroRatio.value = 3.0;
    CHECK(Evaluate(zeroDen, zeroRatio, rr) && rr.pass && rr.lhs == 2 && rr.rhs == 0 && std::fabs(rr.actual - 2.0) < 1e-6);
    Metrics none;
    none.Begin(0.0);
    CHECK(!Evaluate(none, zeroRatio, rr) && !rr.pass); // 0:0
    Metrics twoOne;
    twoOne.Begin(0.0);
    twoOne.ObserveLine("0.50 [WarsBots] #1 combat kill shooter=1 target=2 via stock GameRules::KillPlayer.");
    twoOne.ObserveLine("0.60 [WarsBots] #1 combat kill shooter=1 target=2 via stock GameRules::KillPlayer.");
    twoOne.ObserveLine("0.70 [WarsBots] #2 combat kill shooter=2 target=3 via stock GameRules::KillPlayer.");
    CHECK(!Evaluate(twoOne, zeroRatio, rr) && !rr.pass); // 2:1 < 3
    Assertion missingOther = ratio; missingOther.other = "does_not_exist";
    CHECK(!Evaluate(c143, missingOther, rr) && !rr.pass);
    Assertion invalidOther = ratio; invalidOther.other = "not_a_metric";
    CHECK(!Evaluate(c143, invalidOther, rr) && !rr.pass);

    // landing_z is supported by name but has no measured value before a LINK.
    Metrics noLink;
    CHECK(noLink.SupportsMetric("landing_z"));
    CHECK(std::isnan(noLink.Value("landing_z")));
    Assertion land; land.metric = "landing_z"; land.op = "ge"; land.value = 136.9;
    CHECK(!Evaluate(noLink, land, rr) && !rr.pass);
    CHECK(Evaluate(c143, land, rr) && rr.pass);

    // link_failed is the JSON export name; it must measure, not go null after a real LINK failed.
    Runtime linkRt;
    CHECK(linkRt.Load("{\"id\":\"link\",\"duration_s\":1,\"assert\":[{\"metric\":\"link_failed\",\"op\":\"ge\",\"value\":1}]}", &err));
    CHECK(linkRt.Start(0.0));
    linkRt.Observe("0.50 [WarsBots] #1 LINK failed kind=jump reason=missed_landing at=(0,0,0) actor=1.");
    const std::string linkJson = linkRt.Finish(1.0);
    CHECK(linkJson.find("\"link_failed\": 1") != std::string::npos);
    CHECK(linkJson.find("\"pass\": true") != std::string::npos);

    Assertion firstDsg; firstDsg.metric = "pickup:DSG1"; firstDsg.op = "le_seconds"; firstDsg.value = 2.5;
    CHECK(Evaluate(c143, firstDsg, rr) && rr.pass);
    Assertion firstDsgFail = firstDsg; firstDsgFail.value = 1.5;
    CHECK(!Evaluate(c143, firstDsgFail, rr) && !rr.pass);
    Assertion noPickup; noPickup.metric = "pickup:Missing"; noPickup.op = "le_seconds"; noPickup.value = 10.0;
    CHECK(!Evaluate(c143, noPickup, rr) && !rr.pass); // no event is a fail, not 0 s

    Scenario ratioParsed;
    CHECK(ParseScenario("{\"id\":\"ratio\",\"assert\":[{\"metric\":\"kills:1\",\"op\":\"ge_ratio_of\",\"other\":\"kills:2\",\"value\":3.0}]}", ratioParsed, &err));
    CHECK(ratioParsed.assertions.size() == 1 && ratioParsed.assertions[0].other == "kills:2");
    CHECK(!IsSupportedOperator("bogus"));

    // DS-059: every real C143 fixture must name a supported metric and operator on an empty Metrics.
    static const char* const kProbeFiles[] = {
        "Tools/Probes/duel_skill.json", "Tools/Probes/fence_hop.json", "Tools/Probes/roof_strength.json",
        "Tools/Probes/sniper_reach_dsg1.json", "Tools/Probes/sniper_reach_gauss.json"
    };
    for (size_t i = 0; i < sizeof kProbeFiles / sizeof kProbeFiles[0]; ++i)
    {
        FILE* pf = std::fopen(kProbeFiles[i], "rb");
        CHECK(pf != nullptr);   // missing fixture must fail this binary, even from an empty CWD
        if (!pf) continue;
        std::string text;
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, pf)) > 0) text.append(buf, n);
        std::fclose(pf);
        Scenario fs;
        CHECK(ParseScenario(text, fs, &err));
        for (size_t j = 0; j < fs.assertions.size(); ++j)
        {
            CHECK(Metrics().SupportsMetric(fs.assertions[j].metric));
            CHECK(IsSupportedOperator(fs.assertions[j].op));
        }
    }

    // Unknown metric must fail, not pass by default.
    Assertion unknown; unknown.metric = "does_not_exist"; unknown.op = "eq"; unknown.value = 0;
    AssertionResult ur;
    CHECK(!Evaluate(m, unknown, ur));
    CHECK(!ur.pass);

    // DS-053: repo recipes carry raw setup commands (wb_add/wb_teleport).
    Scenario recipe;
    CHECK(ParseScenario("{\"id\":\"r\",\"setup\":[\"wb_add 1 elite marksman\",\"wb_teleport 1 1 2 3\"],\"duration_s\":5}", recipe, &err));
    CHECK(recipe.setup.size() == 2 && recipe.setup[0] == "wb_add 1 elite marksman");
    {
        const std::vector<std::string> cmds = BuildSetupCommands(recipe);
        bool hasAdd = false, hasTp = false;
        for (size_t i = 0; i < cmds.size(); ++i) { hasAdd |= cmds[i] == "wb_add 1 elite marksman"; hasTp |= cmds[i] == "wb_teleport 1 1 2 3"; }
        CHECK(hasAdd && hasTp);
    }

    // Malformed JSON is rejected.
    Json j;
    CHECK(!ParseJson("{ \"id\": ", j, &err));
    CHECK(!err.empty());

    // A real probe file from the repo (if the test runs from the repo root).
    static const char* file = "Tools/Probes/exploit_height_camp.json";
    if (FILE* f = std::fopen(file, "rb"))
    {
        std::string text;
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, n);
        std::fclose(f);
        Scenario real;
        CHECK(ParseScenario(text, real, &err));
        CHECK(real.id == "exploit_height_camp");
        CHECK(!real.dummies.empty());
    }

    // DS-047: the repo probe files use "assert"; accept it as an alias of "assertions".
    Scenario alias;
    CHECK(ParseScenario("{\"id\":\"alias\",\"assert\":[{\"metric\":\"kills\",\"op\":\"eq\",\"value\":0}]}", alias, &err));
    CHECK(alias.assertions.size() == 1 && alias.assertions[0].metric == "kills");

    // DS-047: a PowerStruggle wave revive bypasses BotInstance::DoRevive; PS REVIVE SEEN must count.
    Metrics ps;
    ps.Begin(0.0);
    ps.ObserveLine("8.00 [WarsBots] PS REVIVE SEEN EntityId=1 health=100 source=wave.");
    CHECK(ps.revives == 1);

    // DS-047: engine-free runtime host: load -> start -> observe -> due -> result JSON.
    Runtime rt;
    CHECK(rt.Load("{\"id\":\"runtime\",\"duration_s\":2,\"assert\":[{\"metric\":\"revive\",\"op\":\"ge\",\"value\":1}]}", &err));
    CHECK(rt.Start(10.0));
    CHECK(rt.Running());
    CHECK(rt.Remaining(10.5) > 1.4 && rt.Remaining(10.5) < 1.6);
    rt.Observe("11.00 [WarsBots] PS REVIVE SEEN EntityId=1 health=100 source=wave.");
    CHECK(!rt.Due(11.9));
    CHECK(rt.Due(12.1));
    const std::string result = rt.Finish(12.1);
    CHECK(rt.finished && !rt.Running());
    CHECK(result.find("\"pass\": true") != std::string::npos);
    CHECK(result.find("\"revive\": 1") != std::string::npos);
    CHECK(result.find("\"metric\": \"revive\"") != std::string::npos);
    CHECK(result.find("\"id\": \"runtime\"") != std::string::npos);

    Runtime bad;
    CHECK(bad.Load("{\"id\":\"bad\",\"duration_s\":1,\"assert\":[{\"metric\":\"kills\",\"op\":\"ge\",\"value\":1}]}", &err));
    CHECK(bad.Start(0.0));
    CHECK(bad.Finish(1.0).find("\"pass\": false") != std::string::npos);

    if (failures) { std::printf("probe_runner: %d FAILURES\n", failures); return 1; }
    std::printf("probe_runner: all tests passed\n");
    return 0;
}



