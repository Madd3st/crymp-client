// DS-098 offline probe: project native Shore spawns and capture destinations onto the C149 candidate navmesh.
// Uses the real WarsBots NavMeshCore (BotNavMeshCore.cpp + Detour), no engine. Read-only.
#include "CryGame/Bots/BotNavMeshCore.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using WarsBots::NavMeshCore;
using WarsBots::NavLinkFlag;
using WarsBots::NavLinkKinds;
using WarsBots::NavPathLink;

struct Pt { int id; float x, y, z; };

static std::vector<Pt> ReadPts(const char* path)
{
    std::vector<Pt> out;
    FILE* f = std::fopen(path, "rb");
    if (!f) return out;
    int id; float x, y, z;
    while (std::fscanf(f, "%d %f %f %f", &id, &x, &y, &z) == 4) out.push_back({id, x, y, z});
    std::fclose(f);
    return out;
}

int main(int argc, char** argv)
{
    if (argc < 4) { std::printf("usage: ds098_probe <navmesh> <links.txt> <spawns.txt>\n"); return 2; }
    NavMeshCore m; std::string err;
    if (!m.LoadFile(argv[1], &err)) { std::printf("load failed: %s\n", err.c_str()); return 1; }
    std::vector<char> txt;
    if (FILE* f = std::fopen(argv[2], "rb")) { int c; while ((c = std::fgetc(f)) != EOF) txt.push_back(char(c)); std::fclose(f); }
    const int kinds = m.LoadLinkKinds(txt.empty() ? "" : txt.data());
    unsigned short mask = 0;
    for (int k = 0; k < NavLinkKinds; ++k) mask |= NavLinkFlag(k);
    m.SetLinkMask(mask);
    std::printf("mesh polys=%d components=%d links=%d linkKinds=%d mask=0x%X\n",
                m.Polys(), m.Components(), m.Links(), kinds, unsigned(mask));

    const std::vector<Pt> spawns = ReadPts(argv[3]);
    int on = 0, off = 0, lower = 0, lowerOn = 0;
    std::vector<Pt> lowerPts;
    for (const Pt& p : spawns)
    {
        const float q[3] = {p.x, p.y, p.z};
        float s[3];
        const bool ok = m.Project(q, s, 1.5f, 1.0f, 3.0f);
        if (ok) ++on; else ++off;
        if (p.z < 99.5f) { ++lower; lowerPts.push_back(p); if (ok) ++lowerOn; }
        if (p.z < 99.5f || !ok)
            std::printf("spawn %d (%.2f %.2f %.2f): project=%s snapped=(%.2f %.2f %.2f) dz=%.2f\n",
                        p.id, p.x, p.y, p.z, ok ? "on" : "OFF", ok ? s[0] : 0.f, ok ? s[1] : 0.f, ok ? s[2] : 0.f,
                        ok ? s[2] - p.z : 0.f);
    }
    std::printf("spawns=%zu on=%d off=%d lower(z<99.5)=%d lowerOnMesh=%d\n",
                spawns.size(), on, off, lower, lowerOn);

    // The same lower points with a deep search box: does the hull floor exist in this mesh at all?
    for (const Pt& p : lowerPts)
    {
        const float q[3] = {p.x, p.y, p.z};
        float deep[3];
        const bool ok = m.Project(q, deep, 3.0f, 2.0f, 20.0f);
        std::printf("lower %d deepProject=%s snapped=(%.2f %.2f %.2f)\n", p.id,
                    ok ? "on" : "OFF", ok ? deep[0] : 0.f, ok ? deep[1] : 0.f, ok ? deep[2] : 0.f);
    }

    // Capture destinations exactly as b8 logs them (PS CAPTURE dest id=.. pos=..).
    struct Dest { int id; float x, y, z; };
    const Dest dests[] = {
        {64, 1960.98f, 1387.64f, 110.31f},
        {316, 2082.12f, 1284.04f, 124.56f},
        {344, 2133.21f, 1482.51f, 111.11f},
        {579, 2216.32f, 1334.99f, 124.51f},
        {654, 1794.54f, 1690.64f, 101.53f},
    };
    // Retained upper US spawn of C149 (BODY STATE at spawn 510) and the failed run's lower spawn 545.
    const float us[3] = {2110.500f, 1882.500f, 103.450f};
    const float lower545[3] = {2114.500f, 1887.875f, 90.575f};
    for (const Dest& d : dests)
    {
        const float g[3] = {d.x, d.y, d.z};
        std::vector<float> path; std::vector<NavPathLink> links; const char* why = "";
        const bool ok = m.FindPathLinks(us, g, path, links, &why);
        std::printf("US510 -> dest %d: path=%s reason=%s points=%zu links=%zu reachable=%d\n",
                    d.id, ok ? "OK" : "NO", why ? why : "", path.size() / 3, links.size(), m.Reachable(us, g) ? 1 : 0);
    }
    for (const Pt& p : lowerPts)
    {
        const float g[3] = {p.x, p.y, p.z};
        std::vector<float> path; std::vector<NavPathLink> links; const char* why = "";
        const bool ok = m.FindPathLinks(lower545, g, path, links, &why);
        std::printf("lower545 -> spawn %d: path=%s reason=%s\n", p.id, ok ? "OK" : "NO", why ? why : "");
    }
    std::vector<float> path; std::vector<NavPathLink> links; const char* why = "";
    std::printf("lower545 -> US510: path=%s reason=%s\n",
                m.FindPathLinks(lower545, us, path, links, &why) ? "OK" : "NO", why ? why : "");

    // Is ANY surface baked inside the submarine volume? Sample the hull footprint at several heights.
    const float zs[] = {90.4f, 95.0f, 99.4f, 99.5f};
    for (float z : zs)
    {
        int hits = 0; float lo = 1e9f, hi = -1e9f;
        for (float x = 2106.0f; x <= 2136.0f; x += 0.5f)
            for (float y = 1876.0f; y <= 1892.0f; y += 0.5f)
            {
                const float q[3] = {x, y, z}; float s[3];
                if (m.Project(q, s, 1.0f, 1.0f, 2.0f)) { ++hits; if (s[2] < lo) lo = s[2]; if (s[2] > hi) hi = s[2]; }
            }
        std::printf("hull-grid z=%.1f: hits=%d snappedZ=[%.2f..%.2f]\n", z, hits, hits ? lo : 0.f, hits ? hi : 0.f);
    }

    // Is ANY poly below the water plane anywhere? A bake with --zmin 99.5 would have none.
    int tot = 0, belowPlane = 0; float minZ = 1e9f;
    for (float x = 1440.0f; x <= 2580.0f; x += 20.0f)
        for (float y = 1010.0f; y <= 2150.0f; y += 20.0f)
        {
            const float q[3] = {x, y, 130.0f}; float s[3];
            if (!m.Project(q, s, 3.0f, 40.0f, 45.0f)) continue;
            ++tot;
            if (s[2] < minZ) minZ = s[2];
            if (s[2] < 99.5f) ++belowPlane;
        }
    std::printf("map-grid samples=%d minSnappedZ=%.2f below99.5=%d\n", tot, tot ? minZ : 0.f, belowPlane);

    return 0;
}

