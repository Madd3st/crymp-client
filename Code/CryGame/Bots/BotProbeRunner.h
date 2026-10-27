// DS-045: engine-free probe runner core. Parses a probe v1 JSON scenario into console setup commands, collects
// metrics from the existing *.bots.log lines and evaluates the scenario assertions. Header-only, C++17, no engine
// includes. The engine-side hooks are documented in Docs/Research/DeepSeek/AUDIT_FIXES.md; this module does not
// execute processes or touch the game.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace WarsBots {
namespace Probe {

// ------------------------------------------------------------------------------------------------ minimal JSON
struct Json
{
    enum Type { Null, Bool, Number, String, Array, Object } type = Null;
    bool boolean = false;
    double number = 0.0;
    std::string text;
    std::vector<Json> array;
    std::map<std::string, Json> object;

    const Json* Get(const char* key) const
    {
        if (type != Object) return nullptr;
        std::map<std::string, Json>::const_iterator it = object.find(key ? key : "");
        return it == object.end() ? nullptr : &it->second;
    }
    double NumberOr(double fallback = 0.0) const { return type == Number ? number : fallback; }
    bool BoolOr(bool fallback = false) const { return type == Bool ? boolean : fallback; }
    std::string StringOr(const char* fallback = "") const { return type == String ? text : std::string(fallback ? fallback : ""); }
};

class JsonParser
{
public:
    JsonParser(const std::string& text, std::string* error) : m_s(text), m_error(error) {}
    bool Parse(Json& out)
    {
        Skip();
        if (!Value(out)) return false;
        Skip();
        if (m_i != m_s.size()) return Fail("trailing characters");
        return true;
    }
private:
    const std::string& m_s;
    size_t m_i = 0;
    std::string* m_error;

    bool Fail(const char* why)
    {
        if (m_error) { char b[128]; std::snprintf(b, sizeof b, "%s at %u", why, unsigned(m_i)); *m_error = b; }
        return false;
    }
    void Skip()
    {
        while (m_i < m_s.size() && (m_s[m_i] == ' ' || m_s[m_i] == '\t' || m_s[m_i] == '\r' || m_s[m_i] == '\n')) ++m_i;
    }
    bool Eat(char c) { Skip(); if (m_i < m_s.size() && m_s[m_i] == c) { ++m_i; return true; } return false; }
    bool String(std::string& out)
    {
        out.clear();
        if (!Eat('"')) return false;
        while (m_i < m_s.size())
        {
            const char c = m_s[m_i++];
            if (c == '"') return true;
            if (c != '\\') { out += c; continue; }
            if (m_i >= m_s.size()) return Fail("bad escape");
            const char e = m_s[m_i++];
            switch (e)
            {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u':
                if (m_i + 4 > m_s.size()) return Fail("short unicode escape");
                m_i += 4; out += '?'; break; // probe ids/classes are ASCII; keep the parser total
            default: return Fail("bad escape");
            }
        }
        return Fail("unterminated string");
    }
    bool Number(double& out)
    {
        Skip();
        const char* start = m_s.c_str() + m_i;
        char* end = nullptr;
        out = std::strtod(start, &end);
        if (end == start) return Fail("bad number");
        m_i += size_t(end - start);
        return true;
    }
    bool Value(Json& out)
    {
        Skip();
        if (m_i >= m_s.size()) return Fail("unexpected end");
        const char c = m_s[m_i];
        if (c == '{')
        {
            out.type = Json::Object; ++m_i;
            Skip();
            if (Eat('}')) return true;
            for (;;)
            {
                std::string key;
                if (!String(key)) return false;
                if (!Eat(':')) return Fail("missing ':'");
                Json v;
                if (!Value(v)) return false;
                out.object[key] = v;
                if (Eat('}')) return true;
                if (!Eat(',')) return Fail("missing ','");
            }
        }
        if (c == '[')
        {
            out.type = Json::Array; ++m_i;
            Skip();
            if (Eat(']')) return true;
            for (;;)
            {
                Json v;
                if (!Value(v)) return false;
                out.array.push_back(v);
                if (Eat(']')) return true;
                if (!Eat(',')) return Fail("missing ','");
            }
        }
        if (c == '"') { out.type = Json::String; return String(out.text); }
        if (!std::strncmp(m_s.c_str() + m_i, "true", 4)) { m_i += 4; out.type = Json::Bool; out.boolean = true; return true; }
        if (!std::strncmp(m_s.c_str() + m_i, "false", 5)) { m_i += 5; out.type = Json::Bool; out.boolean = false; return true; }
        if (!std::strncmp(m_s.c_str() + m_i, "null", 4)) { m_i += 4; out.type = Json::Null; return true; }
        out.type = Json::Number;
        return Number(out.number);
    }
};

inline bool ParseJson(const std::string& text, Json& out, std::string* error = nullptr)
{
    JsonParser parser(text, error);
    return parser.Parse(out);
}

// ------------------------------------------------------------------------------------------------ scenario
struct BotSpec { std::string skill = "normal", style = "universal"; double x = 0, y = 0, z = 0; bool ai = true; };
struct DummySpec { double x = 0, y = 0, z = 0; int hp = 100; int team = 0; double shootEvery = 0; std::string weapon = "SOCOM"; };
struct ItemSpec { std::string cls; double x = 0, y = 0, z = 0; };
struct BurstSpec { int actor = 0; int shots = 1; double interval = 0.1; };
struct PeekSpec { int actor = 0; double x = 0, y = 0, z = 0; double expose = 0.4; double hide = 1.0; };
struct CloakSpec { int actor = 0; bool on = true; };
struct Assertion { std::string metric, op; double value = 0; };

struct Scenario
{
    std::string id, map;
    int seed = 0;
    double duration = 30.0;
    std::vector<BotSpec> bots;
    std::vector<DummySpec> dummies;
    std::vector<ItemSpec> items;
    std::vector<BurstSpec> bursts;
    std::vector<PeekSpec> peeks;
    std::vector<CloakSpec> cloaks;
    std::map<std::string, std::string> cvars;   // already formatted values
    std::vector<std::string> setup;              // raw console commands from the repo JSON "setup" array
    std::vector<Assertion> assertions;
    bool hasGoal = false;
    double goal[3] = { 0, 0, 0 };
    double goalRadius = 1.5;
};

inline std::string NumberText(double v)
{
    char b[64];
    std::snprintf(b, sizeof b, "%.6g", v);
    return b;
}

inline bool ParseScenario(const std::string& text, Scenario& out, std::string* error = nullptr)
{
    Json root;
    if (!ParseJson(text, root, error)) return false;
    if (root.type != Json::Object) { if (error) *error = "scenario is not an object"; return false; }
    out.id = root.Get("id") ? root.Get("id")->StringOr("probe") : "probe";
    out.map = root.Get("map") ? root.Get("map")->StringOr("multiplayer/ia/steelmill") : "multiplayer/ia/steelmill";
    out.seed = root.Get("seed") ? int(root.Get("seed")->NumberOr(0)) : 0;
    out.duration = root.Get("duration_s") ? root.Get("duration_s")->NumberOr(30.0) : 30.0;

    if (const Json* bots = root.Get("bots"))
        for (size_t i = 0; i < bots->array.size(); ++i)
        {
            const Json& b = bots->array[i];
            BotSpec s;
            if (const Json* p = b.Get("spawn")) if (p->array.size() >= 3) { s.x = p->array[0].NumberOr(); s.y = p->array[1].NumberOr(); s.z = p->array[2].NumberOr(); }
            if (const Json* v = b.Get("skill")) s.skill = v->StringOr("normal");
            if (const Json* v = b.Get("style")) s.style = v->StringOr("universal");
            if (const Json* v = b.Get("ai")) s.ai = v->BoolOr(true);
            out.bots.push_back(s);
        }
    if (const Json* dummies = root.Get("dummies"))
        for (size_t i = 0; i < dummies->array.size(); ++i)
        {
            const Json& d = dummies->array[i];
            DummySpec s;
            if (const Json* p = d.Get("pos")) if (p->array.size() >= 3) { s.x = p->array[0].NumberOr(); s.y = p->array[1].NumberOr(); s.z = p->array[2].NumberOr(); }
            if (const Json* v = d.Get("hp")) s.hp = int(v->NumberOr(100));
            if (const Json* v = d.Get("team")) s.team = int(v->NumberOr(0));
            if (const Json* v = d.Get("shoot_every_s")) s.shootEvery = v->NumberOr(0);
            if (const Json* v = d.Get("weapon")) s.weapon = v->StringOr("SOCOM");
            out.dummies.push_back(s);
        }
    if (const Json* items = root.Get("items"))
        for (size_t i = 0; i < items->array.size(); ++i)
        {
            const Json& it = items->array[i];
            ItemSpec s;
            if (const Json* v = it.Get("class")) s.cls = v->StringOr("");
            if (const Json* p = it.Get("pos")) if (p->array.size() >= 3) { s.x = p->array[0].NumberOr(); s.y = p->array[1].NumberOr(); s.z = p->array[2].NumberOr(); }
            out.items.push_back(s);
        }
    if (const Json* bursts = root.Get("bursts"))
        for (size_t i = 0; i < bursts->array.size(); ++i)
        {
            const Json& b = bursts->array[i];
            BurstSpec s;
            if (const Json* v = b.Get("actor")) s.actor = int(v->NumberOr(0));
            if (const Json* v = b.Get("shots")) s.shots = int(v->NumberOr(1));
            if (const Json* v = b.Get("interval")) s.interval = v->NumberOr(0.1);
            out.bursts.push_back(s);
        }
    if (const Json* peeks = root.Get("peeks"))
        for (size_t i = 0; i < peeks->array.size(); ++i)
        {
            const Json& p = peeks->array[i];
            PeekSpec s;
            if (const Json* v = p.Get("actor")) s.actor = int(v->NumberOr(0));
            if (const Json* pos = p.Get("pos")) if (pos->array.size() >= 3) { s.x = pos->array[0].NumberOr(); s.y = pos->array[1].NumberOr(); s.z = pos->array[2].NumberOr(); }
            if (const Json* v = p.Get("expose_s")) s.expose = v->NumberOr(0.4);
            if (const Json* v = p.Get("hide_s")) s.hide = v->NumberOr(1.0);
            out.peeks.push_back(s);
        }
    if (const Json* cloaks = root.Get("cloaks"))
        for (size_t i = 0; i < cloaks->array.size(); ++i)
        {
            const Json& c = cloaks->array[i];
            CloakSpec s;
            if (const Json* v = c.Get("actor")) s.actor = int(v->NumberOr(0));
            if (const Json* v = c.Get("on")) s.on = v->BoolOr(true);
            out.cloaks.push_back(s);
        }
    if (const Json* cv = root.Get("cvars"))
        for (std::map<std::string, Json>::const_iterator it = cv->object.begin(); it != cv->object.end(); ++it)
        {
            const Json& v = it->second;
            if (v.type == Json::Bool) out.cvars[it->first] = v.boolean ? "1" : "0";
            else if (v.type == Json::Number) out.cvars[it->first] = NumberText(v.number);
            else if (v.type == Json::String) out.cvars[it->first] = v.text;
        }
    if (const Json* setup = root.Get("setup"))
        for (size_t i = 0; i < setup->array.size(); ++i)
            if (setup->array[i].type == Json::String) out.setup.push_back(setup->array[i].text);
    // DS-047: the repo probe files use "assert"; the test scenario uses "assertions". Accept both.
    const Json* as = root.Get("assertions");
    if (!as) as = root.Get("assert");
    if (as)
        for (size_t i = 0; i < as->array.size(); ++i)
        {
            const Json& a = as->array[i];
            Assertion s;
            if (const Json* v = a.Get("metric")) s.metric = v->StringOr("");
            if (const Json* v = a.Get("op")) s.op = v->StringOr("eq");
            if (const Json* v = a.Get("value")) s.value = v->NumberOr(0);
            out.assertions.push_back(s);
        }
    if (const Json* g = root.Get("goal"))
        if (g->array.size() >= 3)
        {
            out.hasGoal = true;
            out.goal[0] = g->array[0].NumberOr(); out.goal[1] = g->array[1].NumberOr(); out.goal[2] = g->array[2].NumberOr();
        }
    if (const Json* r = root.Get("goal_radius")) out.goalRadius = r->NumberOr(1.5);
    return true;
}

inline std::vector<std::string> BuildSetupCommands(const Scenario& s)
{
    std::vector<std::string> out;
    for (std::map<std::string, std::string>::const_iterator it = s.cvars.begin(); it != s.cvars.end(); ++it)
        out.push_back(it->first + " " + it->second);
    // C-126 recipes carry wb_add/wb_teleport in a raw "setup" array; run it after the cvars.
    for (size_t i = 0; i < s.setup.size(); ++i)
        if (s.setup[i].compare(0, 12, "wb_probe_run") != 0)
            out.push_back(s.setup[i]);
    for (size_t i = 0; i < s.bots.size(); ++i)
    {
        const BotSpec& b = s.bots[i];
        out.push_back("wb_probe_spawn " + NumberText(b.x) + " " + NumberText(b.y) + " " + NumberText(b.z) + " " +
            b.skill + " " + b.style + " " + (b.ai ? "1" : "0"));
    }
    for (size_t i = 0; i < s.dummies.size(); ++i)
    {
        const DummySpec& d = s.dummies[i];
        out.push_back("wb_probe_dummy " + NumberText(d.x) + " " + NumberText(d.y) + " " + NumberText(d.z) + " " +
            NumberText(d.hp) + " " + NumberText(d.team) + " " + NumberText(d.shootEvery) + " " + d.weapon);
    }
    for (size_t i = 0; i < s.items.size(); ++i)
    {
        const ItemSpec& it = s.items[i];
        out.push_back("wb_probe_item " + it.cls + " " + NumberText(it.x) + " " + NumberText(it.y) + " " + NumberText(it.z));
    }
    if (s.hasGoal)
        out.push_back("wb_probe_goal " + NumberText(s.goal[0]) + " " + NumberText(s.goal[1]) + " " + NumberText(s.goal[2]) + " " + NumberText(s.goalRadius));
    for (size_t i = 0; i < s.bursts.size(); ++i)
        out.push_back("wb_probe_burst " + NumberText(s.bursts[i].actor) + " " + NumberText(s.bursts[i].shots) + " " + NumberText(s.bursts[i].interval));
    for (size_t i = 0; i < s.peeks.size(); ++i)
        out.push_back("wb_probe_peek " + NumberText(s.peeks[i].actor) + " " + NumberText(s.peeks[i].x) + " " + NumberText(s.peeks[i].y) + " " + NumberText(s.peeks[i].z) + " " + NumberText(s.peeks[i].expose) + " " + NumberText(s.peeks[i].hide));
    for (size_t i = 0; i < s.cloaks.size(); ++i)
        out.push_back("wb_probe_cloak " + NumberText(s.cloaks[i].actor) + " " + (s.cloaks[i].on ? "1" : "0"));
    out.push_back("wb_probe_run " + s.id + " " + NumberText(s.duration));
    return out;
}

// ------------------------------------------------------------------------------------------------ metrics
struct Metrics
{
    double start = -1, firstShot = -1, goalTime = -1, detectedAtM = -1, exposedS = 0;
    int shots = 0, hits = 0, kills = 0, deaths = 0, pickups = 0, revives = 0;
    int stalls = 0, stuckEvents = 0, trapMax = 0, linkAttempts = 0, vantageHolds = 0, glances = 0, heard = 0;
    int headshots = 0;
    std::vector<double> hitDistances, targetDistances;

    void Begin(double t) { start = t; firstShot = -1; }
    void ObserveLine(const std::string& line)
    {
        const char* p = line.c_str();
        char* end = nullptr;
        const double t = std::strtod(p, &end);
        if (end == p) return;
        if (start < 0) start = t;
        if (std::strstr(p, "FIRE SHOT"))
        {
            ++shots;
            if (firstShot < 0) firstShot = t - start;
        }
        if (std::strstr(p, "FIRE HIT phase=before-Lua") || (std::strstr(p, "FIRE HIT") && !std::strstr(p, "after-Lua")))
        {
            ++hits;
            if (std::strstr(p, "head=1")) ++headshots;
            const double d = FindDouble(line, "distance=");
            if (d > 0) hitDistances.push_back(d);
        }
        if (std::strstr(p, "combat kill")) ++kills;
        if (std::strstr(p, "death observed")) ++deaths;
        if (std::strstr(p, "REAL PICKUP")) ++pickups;
        if (std::strstr(p, "revive result") && std::strstr(p, "alive=1")) ++revives;
        // DS-047: in PowerStruggle the Lua wave calls GameRules::RevivePlayer directly, so BotInstance::DoRevive
        // never logs "revive result". The external revive is visible as PS REVIVE SEEN; count it as a revive.
        if (std::strstr(p, "PS REVIVE SEEN")) ++revives;
        if (std::strstr(p, "blocked=1")) ++stalls;
        if (std::strstr(p, "stuck=1")) ++stuckEvents;
        if (std::strstr(p, "TRAP RECOVERY"))
        {
            const int l = int(FindDouble(line, "level="));
            if (l > trapMax) trapMax = l;
        }
        if (std::strstr(p, "LINK start")) ++linkAttempts;
        if (std::strstr(p, "VANTAGE")) ++vantageHolds;
        if (std::strstr(p, "GLANCE spot=")) ++glances;
        if (std::strstr(p, "HEARD gunshot")) ++heard;
        if (std::strstr(p, "PROBE GOAL reached")) goalTime = t - start;
        const double det = FindDouble(line, "detected_at_m=");
        if (det >= 0) detectedAtM = det;
        const double exp = FindDouble(line, "exposed_s=");
        if (exp >= 0) exposedS = exp;
        if (std::strstr(p, "TACTIC SAMPLE") && std::strstr(p, "visible=1"))
        {
            const double d = FindDouble(line, "distance=");
            if (d > 0.5) targetDistances.push_back(d);
        }
    }

    double Accuracy() const { return shots ? 100.0 * hits / shots : 0.0; }
    double HitDistanceP50() const { return Percentile(hitDistances, 0.5); }
    double HitDistanceP90() const { return Percentile(hitDistances, 0.9); }
    double TargetDistanceP50() const { return Percentile(targetDistances, 0.5); }
    double TargetDistanceP90() const { return Percentile(targetDistances, 0.9); }

    double Value(const std::string& metric) const
    {
        if (metric == "shots") return shots;
        if (metric == "hits") return hits;
        if (metric == "kills") return kills;
        if (metric == "deaths") return deaths;
        if (metric == "pickup") return pickups;
        if (metric == "revive") return revives;
        if (metric == "time_to_first_shot") return firstShot;
        if (metric == "goal_time") return goalTime;
        if (metric == "stalls") return stalls;
        if (metric == "stuck_events") return stuckEvents;
        if (metric == "trap_level_max") return trapMax;
        if (metric == "link_attempts") return linkAttempts;
        if (metric == "vantage_holds") return vantageHolds;
        if (metric == "glances") return glances;
        if (metric == "heard") return heard;
        if (metric == "detected_at_m") return detectedAtM;
        if (metric == "exposed_s") return exposedS;
        if (metric == "accuracy") return Accuracy();
        if (metric == "headshots") return headshots;
        if (metric == "hit_distance_p50") return HitDistanceP50();
        if (metric == "hit_distance_p90") return HitDistanceP90();
        if (metric == "target_distance_p50") return TargetDistanceP50();
        if (metric == "target_distance_p90") return TargetDistanceP90();
        return std::numeric_limits<double>::quiet_NaN();
    }

private:
    static double FindDouble(const std::string& line, const char* key)
    {
        const size_t n = std::strlen(key);
        const size_t p = line.find(key);
        if (p == std::string::npos) return -1;
        const char* s = line.c_str() + p + n;
        char* end = nullptr;
        const double v = std::strtod(s, &end);
        return end == s ? -1 : v;
    }
    static double Percentile(std::vector<double> v, double q)
    {
        if (v.empty()) return 0;
        std::sort(v.begin(), v.end());
        const size_t i = size_t(std::floor((v.size() - 1) * q));
        return v[i];
    }
};

struct AssertionResult { std::string metric, op; double expected = 0, actual = 0; bool pass = false; };

inline bool Evaluate(const Metrics& m, const Assertion& a, AssertionResult& r)
{
    r.metric = a.metric; r.op = a.op; r.expected = a.value;
    r.actual = m.Value(a.metric);
    if (std::isnan(r.actual)) { r.pass = false; return false; }
    const double e = 1e-6;
    if (a.op == "lt") r.pass = r.actual < a.value - e;
    else if (a.op == "le") r.pass = r.actual <= a.value + e;
    else if (a.op == "gt") r.pass = r.actual > a.value + e;
    else if (a.op == "ge") r.pass = r.actual >= a.value - e;
    else if (a.op == "ne") r.pass = std::fabs(r.actual - a.value) > e;
    else r.pass = std::fabs(r.actual - a.value) <= e;
    return r.pass;
}

inline std::vector<AssertionResult> EvaluateAll(const Metrics& m, const std::vector<Assertion>& assertions)
{
    std::vector<AssertionResult> out;
    for (size_t i = 0; i < assertions.size(); ++i)
    {
        AssertionResult r;
        Evaluate(m, assertions[i], r);
        out.push_back(r);
    }
    return out;
}

// ------------------------------------------------------------------------------------------------ runtime
// DS-047: engine-free half of the probe runner host. The console adapter (BotManager) parses the scenario
// file, executes BuildSetupCommands, feeds every line that WarsBots::Log writes through Observe() and calls
// Finish() when Due(). Header-only and unit-tested in Tests/BotProbeRunnerTests.cpp.
inline std::string JsonQuote(const std::string& s)
{
    std::string out = "\"";
    for (size_t i = 0; i < s.size(); ++i)
    {
        const unsigned char c = (unsigned char)s[i];
        if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", unsigned(c)); out += b; }
        else out += char(c);
    }
    out += '"';
    return out;
}

inline void JsonNumber(std::string& out, double v)
{
    if (std::isnan(v)) { out += "null"; return; }
    char b[64];
    std::snprintf(b, sizeof b, "%.6g", v);
    out += b;
}

struct Runtime
{
    Scenario scenario;
    Metrics metrics;
    bool loaded = false, active = false, finished = false;
    double start = 0, end = 0;
    std::vector<AssertionResult> results;

    bool Load(const std::string& text, std::string* error = nullptr)
    {
        loaded = ParseScenario(text, scenario, error);
        return loaded;
    }

    bool Start(double now, double durationOverride = 0)
    {
        if (!loaded) return false;
        metrics = Metrics();
        metrics.Begin(now);
        start = now;
        end = now + (durationOverride > 0 ? durationOverride : scenario.duration);
        active = true; finished = false; results.clear();
        return true;
    }

    void Stop() { active = false; }
    void Observe(const std::string& line) { if (active) metrics.ObserveLine(line); }
    bool Running() const { return active; }
    bool Due(double now) const { return active && now >= end; }
    double Remaining(double now) const { return active && end > now ? end - now : 0; }

    // Evaluates the assertions and returns the result JSON. Call once when Due().
    std::string Finish(double now)
    {
        results = EvaluateAll(metrics, scenario.assertions);
        active = false; finished = true;
        return Json(now);
    }

    std::string Json(double now) const
    {
        bool pass = true;
        for (size_t i = 0; i < results.size(); ++i) pass = pass && results[i].pass;
        std::string out = "{\n  \"schema\": 1,\n  \"id\": ";
        out += JsonQuote(scenario.id);
        out += ",\n  \"map\": "; out += JsonQuote(scenario.map);
        out += ",\n  \"started_at\": "; JsonNumber(out, start);
        out += ",\n  \"duration_s\": "; JsonNumber(out, end - start);
        out += ",\n  \"elapsed_s\": "; JsonNumber(out, now - start);
        out += ",\n  \"pass\": "; out += pass ? "true" : "false";
        out += ",\n  \"metrics\": {";
        static const char* const names[] = {
            "shots", "hits", "kills", "deaths", "pickup", "revive", "time_to_first_shot", "goal_time",
            "stalls", "stuck_events", "trap_level_max", "link_attempts", "vantage_holds", "glances", "heard",
            "detected_at_m", "exposed_s", "accuracy", "headshots", "hit_distance_p50", "hit_distance_p90",
            "target_distance_p50", "target_distance_p90"
        };
        for (size_t i = 0; i < sizeof names / sizeof names[0]; ++i)
        {
            if (i) out += ", ";
            out += '"'; out += names[i]; out += "\": ";
            JsonNumber(out, metrics.Value(names[i]));
        }
        out += "},\n  \"assertions\": [";
        for (size_t i = 0; i < results.size(); ++i)
        {
            const AssertionResult& r = results[i];
            out += i ? ",\n    {" : "\n    {";
            out += "\"metric\": "; out += JsonQuote(r.metric);
            out += ", \"op\": "; out += JsonQuote(r.op);
            out += ", \"expected\": "; JsonNumber(out, r.expected);
            out += ", \"actual\": "; JsonNumber(out, r.actual);
            out += ", \"pass\": "; out += r.pass ? "true" : "false";
            out += "}";
        }
        out += "\n  ]\n}\n";
        return out;
    }
};

} // namespace Probe
} // namespace WarsBots

