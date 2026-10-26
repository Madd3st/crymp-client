// Offline link audit (DS-037 #3): flags baked off-mesh links that cannot be executed by the native
// drop/jump/Strength jump, and writes a filtered *.links.txt that disables them (BotNavMeshCore matches
// link kinds by end points, so removing a line sets that link's kind to unknown -> never used).
//
// Rules (hard = removed by default; soft = reported, removed only with --strict):
//   flat_strength      strength with |dz| < 0.8 m: a level gap, not a climb; the bot runs off the edge.
//   strength_not_up    strength with dz < 0.3 m: landing is not on a higher part; should be jump/drop.
//   rise_too_high      strength with dz > 3.5 m: beyond the native Strength jump arc.
//   long_gap           strength/jump with horizontal gap > 3.5 m (soft; a running Strength jump can carry ~5 m).
//   drop_up            drop with dz > 0.3 m: a drop must go down.
//   zero_length        horizontal gap < 0.25 m and |dz| < 0.25 m.
//   duplicate          same directed endpoints (0.1 m) and kind as an earlier line; the later line is dropped.
//
// Usage: link_audit <out_dir> <Map.links.txt>...
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

struct Link {
    int line = 0;
    float x0, y0, z0, x1, y1, z1;
    int bidir = 0;
    std::string kind;
    float gap = 0, dz = 0;
    std::vector<std::string> hard, soft;
    std::string raw;
};

static std::string Base(const std::string& path)
{
    size_t a = path.find_last_of("/\\");
    std::string n = a == std::string::npos ? path : path.substr(a + 1);
    size_t d = n.find(".links.txt");
    return d == std::string::npos ? n : n.substr(0, d);
}

static bool Parse(const std::string& path, std::vector<Link>& out)
{
    std::ifstream in(path);
    if (!in) return false;
    std::string line;
    int n = 0;
    while (std::getline(in, line))
    {
        ++n;
        std::istringstream ss(line);
        Link l; l.line = n; l.raw = line;
        if (!(ss >> l.x0 >> l.y0 >> l.z0 >> l.x1 >> l.y1 >> l.z1 >> l.bidir >> l.kind)) continue;
        const float dx = l.x1 - l.x0, dy = l.y1 - l.y0;
        l.gap = std::sqrt(dx * dx + dy * dy);
        l.dz = l.z1 - l.z0;
        if (l.kind == "strength")
        {
            if (std::fabs(l.dz) < 0.8f) l.soft.push_back("flat_strength");
            if (std::fabs(l.dz) < 0.8f && l.gap > 3.5f) l.hard.push_back("flat_strength_long");
            if (l.dz < 0.3f) l.soft.push_back("strength_not_up");
            if (l.dz > 3.5f) l.soft.push_back("rise_too_high");
        }
        if (l.kind == "drop" && l.dz > 0.3f) l.hard.push_back("drop_up");
        if ((l.kind == "strength" && l.gap > 9.0f) || (l.kind == "jump" && l.gap > 3.5f)) l.soft.push_back("long_gap");
        if (l.gap < 0.25f && std::fabs(l.dz) < 0.25f) l.hard.push_back("zero_length");
        out.push_back(l);
    }
    // duplicates: same directed endpoints (0.1 m) and kind; later line is the duplicate
    std::map<std::string, int> seen;
    for (Link& l : out)
    {
        char key[256];
        std::snprintf(key, sizeof key, "%s|%.1f|%.1f|%.1f|%.1f|%.1f|%.1f", l.kind.c_str(),
            l.x0, l.y0, l.z0, l.x1, l.y1, l.z1);
        auto it = seen.find(key);
        if (it != seen.end()) l.hard.push_back("duplicate_of_" + std::to_string(it->second));
        else seen[key] = l.line;
    }
    return true;
}

static bool Has(const std::vector<std::string>& v, const char* prefix)
{
    for (const std::string& s : v) if (s.rfind(prefix, 0) == 0) return true;
    return false;
}

int main(int argc, char** argv)
{
    if (argc < 3) { std::printf("usage: link_audit <out_dir> <Map.links.txt>...\n"); return 2; }
    const std::string outDir = argv[1];
    std::ofstream csv(outDir + "/bad_links.csv");
    csv << "map,line,kind,x0,y0,z0,x1,y1,z1,gap,dz,hard,soft,raw\n";
    for (int a = 2; a < argc; ++a)
    {
        std::vector<Link> links;
        if (!Parse(argv[a], links)) { std::printf("cannot read %s\n", argv[a]); continue; }
        const std::string map = Base(argv[a]);
        std::ofstream filtered(outDir + "/" + map + ".links.filtered.txt");
        std::ofstream strict(outDir + "/" + map + ".links.strict.txt");
        int hardCount = 0, softCount = 0, kept = 0, keptStrict = 0;
        std::map<std::string, int> byReason;
        for (const Link& l : links)
        {
            const bool hard = !l.hard.empty();
            const bool soft = !l.soft.empty();
            if (hard || soft)
            {
                csv << map << ',' << l.line << ',' << l.kind << ','
                    << l.x0 << ',' << l.y0 << ',' << l.z0 << ',' << l.x1 << ',' << l.y1 << ',' << l.z1 << ','
                    << l.gap << ',' << l.dz << ',';
                for (size_t i = 0; i < l.hard.size(); ++i) csv << (i ? "+" : "") << l.hard[i];
                csv << ',';
                for (size_t i = 0; i < l.soft.size(); ++i) csv << (i ? "+" : "") << l.soft[i];
                csv << ',' << l.raw << '\n';
            }
            for (const std::string& r : l.hard) ++byReason[r];
            for (const std::string& r : l.soft) ++byReason["soft:" + r];
            if (hard) ++hardCount;
            if (soft) ++softCount;
            if (!hard) { filtered << l.raw << '\n'; ++kept; }
            if (!hard && !soft && l.soft.empty()) { strict << l.raw << '\n'; ++keptStrict; }
        }
        std::printf("%s: lines=%zu hard=%d soft=%d kept=%d strict_kept=%d\n", map.c_str(), links.size(),
            hardCount, softCount, kept, keptStrict);
        for (const auto& kv : byReason) std::printf("  %s=%d\n", kv.first.c_str(), kv.second);
    }
    return 0;
}



