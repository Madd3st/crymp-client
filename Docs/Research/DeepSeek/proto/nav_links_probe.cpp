// Offline probe: which spawn -> DSG1/Gauss routes contain navmesh links, and where they stop.
// Engine-free; uses the same BotNavMeshCore as the game. Read-only research tool (DS-033).
#include "CryGame/Bots/BotNavMeshCore.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace WarsBots;

static bool ReadFile(const char* path, std::vector<unsigned char>& out)
{
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return false; }
    out.resize(size_t(n));
    size_t got = fread(out.data(), 1, out.size(), f);
    fclose(f);
    return got == out.size();
}

struct Spawn { std::string name; float p[3]; };

int main(int argc, char** argv)
{
    if (argc < 4) { printf("usage: nav_links_probe <navmesh> <links.txt> <spawns.csv>\n"); return 2; }
    NavMeshCore mesh;
    std::string err;
    if (!mesh.LoadFile(argv[1], &err)) { printf("load failed: %s\n", err.c_str()); return 3; }
    std::vector<unsigned char> linkText;
    if (!ReadFile(argv[2], linkText)) { printf("links read failed\n"); return 3; }
    linkText.push_back(0);
    const int kinds = mesh.LoadLinkKinds((const char*)linkText.data());
    const unsigned short all = (unsigned short)(NavLinkFlag(NavLinkDrop) | NavLinkFlag(NavLinkJump) | NavLinkFlag(NavLinkStrength));
    mesh.SetLinkMask(all);
    printf("mesh polys=%d components=%d links=%d kinds=%d mismatches=%d mask=0x%x\n",
        mesh.Polys(), mesh.Components(), mesh.Links(), kinds, mesh.LinkMismatches(), mesh.LinkMask());

    const float dsg1[3] = {1307.2332f, 1330.6285f, 159.11365f};
    const float gauss[3] = {1225.4445f, 1331.7963f, 149.26045f};
    float proj[3];
    printf("project DSG1=%d ", mesh.Project(dsg1, proj) ? 1 : 0);
    if (mesh.Project(dsg1, proj)) printf("(%.2f,%.2f,%.2f)", proj[0], proj[1], proj[2]);
    printf(" Gauss=%d ", mesh.Project(gauss, proj) ? 1 : 0);
    if (mesh.Project(gauss, proj)) printf("(%.2f,%.2f,%.2f)", proj[0], proj[1], proj[2]);
    printf("\n");

    std::vector<Spawn> spawns;
    std::ifstream in(argv[3]);
    std::string line;
    while (std::getline(in, line))
    {
        if (line.empty() || line[0] == '#') continue;
        for (char& ch : line) if (ch == ',') ch = ' ';
        std::stringstream ss(line);
        Spawn s;
        if (!(ss >> s.name >> s.p[0] >> s.p[1] >> s.p[2])) continue;
        spawns.push_back(s);
    }
    printf("spawns=%zu\n", spawns.size());

    const char* goals[2] = {"DSG1", "Gauss"};
    const float* goalPos[2] = {dsg1, gauss};
    for (int g = 0; g < 2; ++g)
    {
        int ok = 0, withLinks = 0, totalLinks = 0;
        for (const Spawn& s : spawns)
        {
            std::vector<float> xyz; std::vector<NavPathLink> links; const char* reason = "?";
            const bool found = mesh.FindPathLinks(s.p, goalPos[g], xyz, links, &reason);
            if (found) { ++ok; totalLinks += int(links.size()); if (!links.empty()) ++withLinks; }
            double len = 0; for (size_t i = 0; i + 3 < xyz.size(); i += 3) { float dx = xyz[i+3]-xyz[i], dy = xyz[i+4]-xyz[i+1], dz = xyz[i+5]-xyz[i+2]; len += sqrt(dx*dx+dy*dy+dz*dz); }
            printf("%s %s ok=%d reason=%s corners=%zu links=%zu len=%.1f", s.name.c_str(), goals[g], found ? 1 : 0,
                reason, xyz.size() / 3, links.size(), len);
            for (const NavPathLink& l : links)
                printf(" [%s c%d (%.1f,%.1f,%.1f)->(%.1f,%.1f,%.1f)]", NavLinkName(l.kind), l.corner,
                    l.a[0], l.a[1], l.a[2], l.b[0], l.b[1], l.b[2]);
            printf("\n");
        }
        printf("SUMMARY %s ok=%d/%zu withLinks=%d totalLinks=%d\n", goals[g], ok, spawns.size(), withLinks, totalLinks);
    }

    // Nearest reachable points on the DSG1 roof: if the item itself is off-mesh but the roof is walkable, this shows it.
    printf("ROOF scan (x,y around DSG1, z=159.1):\n");
    for (int dy = -2; dy <= 2; ++dy)
        for (int dx = -2; dx <= 2; ++dx)
        {
            float p[3] = {dsg1[0] + dx * 1.0f, dsg1[1] + dy * 1.0f, dsg1[2]};
            float o[3];
            if (mesh.Project(p, o) && fabsf(o[2] - 159.1f) < 1.0f)
                printf("  (%d,%d) -> (%.1f,%.1f,%.1f)\n", dx, dy, o[0], o[1], o[2]);
        }
    return 0;
}
