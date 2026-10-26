// DS-039 #3 tests: boat driver and water route builder (engine-free).
#include "CryGame/Bots/BotBoatDriver.h"
#include <cassert>
#include <cstdio>
#include <string>
using namespace WarsBots;

static BoatInput At(float x, float y, float yaw, float speed, float now = 0.f)
{
    BoatInput in; in.pos = {x, y}; in.yaw = yaw; in.speed = speed; in.now = now; return in;
}

int main()
{
    {
        BotBoatDriver d; d.SetRoute({{0, 0}, {100, 0}});
        BoatOutput o = d.Update(At(0, 0, 0, 0));
        assert(o.action == BoatAction::Forward && o.throttle > 0.9f && std::fabs(o.steer) < 0.01f);
    }
    {
        BotBoatDriver d; d.SetRoute({{0, 0}, {50, 0}, {50, 50}});
        BoatOutput o = d.Update(At(40, 0, 0, 8));
        assert(o.steer > 0.2f); // target is to the left (positive yaw)
        assert(o.brake || o.throttle > 0.f);
    }
    {
        BotBoatDriver d; d.SetRoute({{0, 0}, {100, 0}});
        d.Update(At(10, 0, 0, 0, 0.f));
        BoatOutput o = d.Update(At(10, 0, 0, 0, 2.1f));
        assert(o.action == BoatAction::Reverse && o.reason == std::string("stuck_reverse"));
    }
    {
        BotBoatDriver d; d.SetRoute({{0, 0}, {100, 0}});
        BoatOutput o = d.Update(At(98, 0, 0, 1.0f));
        assert(o.exit && o.action == BoatAction::Exit);
    }
    {
        BotBoatDriver d; d.SetRoute({{0, 0}, {100, 0}});
        BoatInput in = At(20, 0, 0, 8); in.depthAhead = 0.5f; in.depthLeft = 3.f; in.depthRight = 0.4f;
        BoatOutput o = d.Update(in);
        assert(o.brake && o.steer < 0.f && o.reason == std::string("shallow_ahead"));
    }
    {
        DepthGrid g; g.originX = 0; g.originY = 0; g.cell = 2.f; g.nx = 30; g.ny = 30;
        g.depth.assign(size_t(g.nx) * g.ny, 5.f);
        WaterRouteBuilder b;
        WaterRoute r = b.Build(g, {10, 10}, {50, 50});
        assert(r.ok && r.points.size() >= 2);
        assert(BoatDist(r.points.front(), {10, 10}) < 3.f);
    }
    {
        DepthGrid g; g.originX = 0; g.originY = 0; g.cell = 2.f; g.nx = 30; g.ny = 30;
        g.depth.assign(size_t(g.nx) * g.ny, 5.f);
        for (int y = 0; y < g.ny; ++y) if (y < 10 || y > 20) g.depth[size_t(y) * g.nx + 15] = -1.f;
        WaterRouteBuilder b;
        WaterRoute r = b.Build(g, {10, 10}, {50, 50});
        assert(r.ok);
        double minX = 1e9, maxX = -1e9;
        for (auto& p : r.points) { minX = std::min(minX, double(p.x)); maxX = std::max(maxX, double(p.x)); }
        assert(minX < 30.0 && maxX > 32.0);
    }
    {
        DepthGrid g; g.originX = 0; g.originY = 0; g.cell = 2.f; g.nx = 30; g.ny = 30;
        g.depth.assign(size_t(g.nx) * g.ny, 5.f);
        for (int y = 0; y < g.ny; ++y) g.depth[size_t(y) * g.nx + 15] = -1.f;
        WaterRouteBuilder b;
        WaterRoute r = b.Build(g, {10, 10}, {50, 50});
        assert(!r.ok && r.reason == std::string("no_water_route"));
    }
    std::printf("boat driver: all tests passed\n");
    return 0;
}

