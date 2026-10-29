#include "CryGame/Bots/BotNavMeshCore.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>
using namespace WarsBots;

static void Query(NavMeshCore& m, const char* tag, const float* a, const float* b)
{
    const char* why = nullptr;
    std::vector<float> p;
    bool ok = m.FindPath(a, b, p, &why);
    printf("%s walk-only path=%d reason=%s corners=%zu\n", tag, ok ? 1 : 0, why ? why : "-", p.size() / 3);
    std::vector<float> p2;
    std::vector<NavPathLink> links;
    why = nullptr;
    ok = m.FindPathLinks(a, b, p2, links, &why);
    printf("%s all-kinds path=%d reason=%s corners=%zu links=%zu\n", tag, ok ? 1 : 0, why ? why : "-",
        p2.size() / 3, links.size());
    for (const NavPathLink& l : links)
        printf("  link kind=%s corner=%d a=(%.3f,%.3f,%.3f) b=(%.3f,%.3f,%.3f)\n", NavLinkName(l.kind), l.corner,
            l.a[0], l.a[1], l.a[2], l.b[0], l.b[1], l.b[2]);
}

int main(int argc, char** argv)
{
    if (argc < 4) { printf("usage: probe <navmesh> <links.txt> [project x y z | lowerx lowery lowerz upperx uppery upperz]\n"); return 2; }
    NavMeshCore m;
    std::string err;
    if (!m.LoadFile(argv[1], &err)) { printf("load failed: %s\n", err.c_str()); return 3; }
    if (std::string(argv[2]) == "--project" && argc >= 6)
    {
        float p[3] = { (float)atof(argv[3]), (float)atof(argv[4]), (float)atof(argv[5]) };
        float o[3] = { 0, 0, 0 };
        const bool on = m.Project(p, o, 1.5f, 1.0f, 3.0f);
        printf("project=%d snap=(%.3f,%.3f,%.3f)\n", on ? 1 : 0, o[0], o[1], o[2]);
        return 0;
    }
    std::ifstream f(argv[2]);
    std::stringstream ss;
    ss << f.rdbuf();
    const int kinds = m.LoadLinkKinds(ss.str().c_str());
    printf("mesh polys=%d comps=%d links=%d kinds=%d\n", m.Polys(), m.Components(), m.Links(), kinds);
    float a[3] = { 2114.50f, 1887.88f, 90.38f }, b[3] = { 2111.88f, 1882.50f, 103.25f };
    if (argc >= 9) for (int k = 0; k < 3; ++k) { a[k] = (float)atof(argv[3 + k]); b[k] = (float)atof(argv[6 + k]); }
    m.SetLinkMask(0);
    printf("mask0 comps=%d reachable=%d\n", m.Components(), m.Reachable(a, b) ? 1 : 0);
    Query(m, "mask0", a, b);
    m.SetLinkMask(2);
    printf("mask2 comps=%d reachable=%d\n", m.Components(), m.Reachable(a, b) ? 1 : 0);
    m.SetLinkMask(32);
    printf("mask32 comps=%d reachable=%d\n", m.Components(), m.Reachable(a, b) ? 1 : 0);
    m.SetLinkMask(0xffff);
    printf("all comps=%d reachable=%d\n", m.Components(), m.Reachable(a, b) ? 1 : 0);
    Query(m, "all", a, b);
    return 0;
}
