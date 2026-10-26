// Engine-free boat driver + sea-route builder (DS-037 #3 / DS-039 #3).
// Mirrors the CryEngine 2 contract: OnAction has only digital throttle/steer/brake, so the driver returns one
// combined command per 10 Hz tick. Coordinates are Crysis X/Y, yaw radians (forward = cos/sin), speed m/s.
//
// Contract (BOATS_SHORE.md): lookahead 12 m, brake 8 m before a sharp waypoint, reverse after 2 s stuck,
// exit within 6 m of the landing at < 2 m/s, never drive into water shallower than 1.2 m.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <queue>
#include <vector>

namespace WarsBots {

struct BoatVec2 { float x = 0, y = 0; };

inline float BoatDist(const BoatVec2& a, const BoatVec2& b)
{
    const float dx = a.x - b.x, dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}
inline float BoatWrap(float a)
{
    while (a > 3.14159265f) a -= 6.2831853f;
    while (a < -3.14159265f) a += 6.2831853f;
    return a;
}

enum class BoatAction { None = 0, Forward, Reverse, TurnLeft, TurnRight, Brake, Exit };

struct BoatInput {
    BoatVec2 pos;
    float yaw = 0.f;          // rad, forward = (cos yaw, sin yaw)
    float speed = 0.f;        // m/s, unsigned
    float depthAhead = 5.f;   // m, down-ray 3 m ahead
    float depthLeft = 5.f;    // m, 4 m to port
    float depthRight = 5.f;   // m, 4 m to starboard
    bool grounded = true;     // false when the hull is already off the water/beached
    float now = 0.f;
};

struct BoatOutput {
    BoatAction action = BoatAction::None;
    float throttle = 0.f;     // 0..1
    float steer = 0.f;        // -1 left .. +1 right
    bool brake = false;
    bool exit = false;
    const char* reason = "none";
};

struct BoatParams {
    float lookahead = 12.f;
    float brakeDistance = 8.f;
    float landingRadius = 6.f;
    float landingSpeed = 2.f;
    float minDepth = 1.2f;
    float cruiseSpeed = 11.f;
    float turnSlowSpeed = 5.f;
    float stuckTime = 2.f;
    float reverseTime = 1.f;
    float headingDeadband = 0.08f; // rad
};

class BotBoatDriver {
public:
    explicit BotBoatDriver(const BoatParams& p = BoatParams()) : m_p(p) {}

    void Reset()
    {
        m_route.clear(); m_index = 0; m_stuckSince = -1.f; m_reverseUntil = -1.f; m_last = BoatOutput();
    }
    void SetRoute(const std::vector<BoatVec2>& route)
    {
        m_route = route; m_index = 0; m_stuckSince = -1.f; m_reverseUntil = -1.f;
    }
    bool HasRoute() const { return m_route.size() >= 2; }
    const BoatOutput& Last() const { return m_last; }

    BoatOutput Update(const BoatInput& in)
    {
        BoatOutput o;
        if (m_route.size() < 2) { o.reason = "no_route"; m_last = o; return o; }

        // Advance the nearest segment; keep the index monotonic so a wake does not send the boat backwards.
        while (m_index + 1 < m_route.size() &&
               BoatDist(in.pos, m_route[m_index]) + BoatDist(in.pos, m_route[m_index + 1]) <
               BoatDist(m_route[m_index], m_route[m_index + 1]) + 0.5f)
            ++m_index;
        const BoatVec2& final = m_route.back();
        const float toFinal = BoatDist(in.pos, final);

        if (toFinal <= m_p.landingRadius && in.speed <= m_p.landingSpeed)
        {
            o.action = BoatAction::Exit; o.exit = true; o.brake = true; o.reason = "landing";
            m_last = o; return o;
        }

        // Lookahead point along the polyline.
        BoatVec2 target = m_route[m_index];
        float remaining = m_p.lookahead;
        for (size_t i = m_index; i + 1 < m_route.size() && remaining > 0.f; ++i)
        {
            const BoatVec2& a = m_route[i];
            const BoatVec2& b = m_route[i + 1];
            const float seg = BoatDist(a, b);
            if (seg >= remaining) { const float t = remaining / std::max(1e-4f, seg); target = {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; remaining = 0.f; }
            else { remaining -= seg; target = b; }
        }

        // Stuck / reverse.
        if (m_reverseUntil > in.now)
        {
            o.action = BoatAction::Reverse; o.throttle = 0.8f; o.steer = 0.f; o.reason = "reverse";
            m_last = o; return o;
        }
        if (in.speed < 0.5f) { if (m_stuckSince < 0.f) m_stuckSince = in.now; }
        else m_stuckSince = -1.f;
        if (m_stuckSince >= 0.f && in.now - m_stuckSince >= m_p.stuckTime)
        {
            m_reverseUntil = in.now + m_p.reverseTime; m_stuckSince = -1.f;
            o.action = BoatAction::Reverse; o.throttle = 0.8f; o.reason = "stuck_reverse";
            m_last = o; return o;
        }

        // Heading error to the lookahead point.
        const float want = std::atan2(target.y - in.pos.y, target.x - in.pos.x);
        const float err = BoatWrap(want - in.yaw);
        float steer = std::max(-1.f, std::min(1.f, err / 0.8f));
        if (std::fabs(err) < m_p.headingDeadband) steer = 0.f;

        // Shallow water: stop and steer to the deeper side.
        if (in.depthAhead < m_p.minDepth)
        {
            o.action = BoatAction::Brake; o.brake = true; o.throttle = 0.f;
            o.steer = (in.depthLeft >= in.depthRight) ? -0.8f : 0.8f;
            o.reason = "shallow_ahead";
            m_last = o; return o;
        }
        if (in.depthLeft < m_p.minDepth && in.depthRight >= m_p.minDepth) steer = 0.7f;
        if (in.depthRight < m_p.minDepth && in.depthLeft >= m_p.minDepth) steer = -0.7f;

        // Brake for the final landing and for sharp turns; throttle otherwise.
        const bool nearLanding = toFinal <= m_p.brakeDistance;
        const bool sharpTurn = std::fabs(err) > 1.0f;
        if (nearLanding || in.speed > m_p.cruiseSpeed)
            { o.action = BoatAction::Brake; o.brake = true; o.throttle = 0.f; o.reason = nearLanding ? "landing_brake" : "overspeed"; }
        else if (sharpTurn && in.speed > m_p.turnSlowSpeed)
            { o.action = BoatAction::Brake; o.brake = true; o.throttle = 0.2f; o.reason = "turn_brake"; }
        else
            { o.action = BoatAction::Forward; o.throttle = 1.f; o.reason = "cruise"; }

        o.steer = steer;
        m_last = o;
        return o;
    }

private:
    BoatParams m_p;
    std::vector<BoatVec2> m_route;
    size_t m_index = 0;
    float m_stuckSince = -1.f, m_reverseUntil = -1.f;
    BoatOutput m_last;
};

// ---------------------------------------------------------------------------------------------- water route builder

struct DepthGrid {
    float originX = 0.f, originY = 0.f, cell = 4.f;
    int nx = 0, ny = 0;
    std::vector<float> depth; // metres, row-major

    float At(int ix, int iy) const
    {
        if (ix < 0 || iy < 0 || ix >= nx || iy >= ny) return -1.f;
        return depth[size_t(iy) * size_t(nx) + size_t(ix)];
    }
    BoatVec2 Centre(int ix, int iy) const { return {originX + (ix + 0.5f) * cell, originY + (iy + 0.5f) * cell}; }
    bool ToCell(const BoatVec2& p, int& ix, int& iy) const
    {
        ix = int(std::floor((p.x - originX) / cell));
        iy = int(std::floor((p.y - originY) / cell));
        return ix >= 0 && iy >= 0 && ix < nx && iy < ny;
    }
};

struct WaterRoute {
    bool ok = false;
    const char* reason = "none";
    std::vector<BoatVec2> points;
};

class WaterRouteBuilder {
public:
    struct Params {
        float minDepth = 1.2f;
        float clearance = 6.f;   // keep 6 m from land
        float resample = 10.f;
    };

    WaterRouteBuilder() : m_p() {}
    explicit WaterRouteBuilder(const Params& p) : m_p(p) {}

    // A* over the depth grid; cost prefers deeper water, clearance erodes the mask. Returns a smoothed route.
    WaterRoute Build(const DepthGrid& grid, const BoatVec2& start, const BoatVec2& goal) const
    {
        WaterRoute out;
        if (grid.nx <= 0 || grid.ny <= 0 || int(grid.depth.size()) < grid.nx * grid.ny)
            { out.reason = "bad_grid"; return out; }
        int sx, sy, gx, gy;
        if (!grid.ToCell(start, sx, sy) || !grid.ToCell(goal, gx, gy)) { out.reason = "off_grid"; return out; }
        if (!Passable(grid, sx, sy) || !Passable(grid, gx, gy)) { out.reason = "start_or_goal_land"; return out; }

        const int n = grid.nx * grid.ny;
        std::vector<float> g(n, std::numeric_limits<float>::max());
        std::vector<int> prev(n, -1);
        std::vector<char> closed(n, 0);
        struct Node { float f; int i; };
        struct Cmp { bool operator()(const Node& a, const Node& b) const { return a.f > b.f; } };
        std::priority_queue<Node, std::vector<Node>, Cmp> open;
        const int s = sy * grid.nx + sx, gcell = gy * grid.nx + gx;
        g[s] = 0.f; open.push({Heur(sx, sy, gx, gy), s});
        static const int dx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
        static const int dy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
        while (!open.empty())
        {
            const Node cur = open.top(); open.pop();
            if (closed[cur.i]) continue;
            closed[cur.i] = 1;
            if (cur.i == gcell) break;
            const int cx = cur.i % grid.nx, cy = cur.i / grid.nx;
            for (int k = 0; k < 8; ++k)
            {
                const int ax = cx + dx[k], ay = cy + dy[k];
                if (!Passable(grid, ax, ay)) continue;
                const int ai = ay * grid.nx + ax;
                const float step = (k < 4 ? 1.f : 1.41421356f) * grid.cell;
                const float depth = std::max(0.1f, grid.At(ax, ay));
                const float cost = step * (1.f + 2.f / (depth + 0.5f));
                if (g[cur.i] + cost < g[ai])
                {
                    g[ai] = g[cur.i] + cost; prev[ai] = cur.i;
                    open.push({g[ai] + Heur(ax, ay, gx, gy) * grid.cell, ai});
                }
            }
        }
        if (prev[gcell] < 0) { out.reason = "no_water_route"; return out; }

        std::vector<BoatVec2> cells;
        for (int i = gcell; i >= 0; i = prev[i])
        {
            cells.push_back(grid.Centre(i % grid.nx, i / grid.nx));
            if (i == s) break;
        }
        std::reverse(cells.begin(), cells.end());
        out.points = Smooth(grid, cells);
        out.points = Resample(out.points, m_p.resample);
        out.ok = out.points.size() >= 2;
        out.reason = out.ok ? "ok" : "too_short";
        return out;
    }

private:
    Params m_p;
    static float Heur(int ax, int ay, int bx, int by)
    { return std::sqrt(float((ax - bx) * (ax - bx) + (ay - by) * (ay - by))); }

    bool Passable(const DepthGrid& grid, int ix, int iy) const
    {
        if (grid.At(ix, iy) < m_p.minDepth) return false;
        const int r = int(std::ceil(m_p.clearance / grid.cell));
        for (int y = -r; y <= r; ++y)
            for (int x = -r; x <= r; ++x)
                if (grid.At(ix + x, iy + y) < 0.f) return false; // outside grid counts as land
        return true;
    }
    bool ClearLine(const DepthGrid& grid, const BoatVec2& a, const BoatVec2& b) const
    {
        const float d = BoatDist(a, b);
        const int steps = std::max(1, int(d / (grid.cell * 0.5f)));
        for (int i = 1; i < steps; ++i)
        {
            const float t = float(i) / float(steps);
            int ix, iy;
            if (!grid.ToCell({a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}, ix, iy) || !Passable(grid, ix, iy))
                return false;
        }
        return true;
    }
    std::vector<BoatVec2> Smooth(const DepthGrid& grid, const std::vector<BoatVec2>& in) const
    {
        if (in.size() < 3) return in;
        std::vector<BoatVec2> out; out.push_back(in.front());
        size_t i = 0;
        while (i + 1 < in.size())
        {
            size_t best = i + 1;
            for (size_t j = in.size() - 1; j > i + 1; --j)
                if (ClearLine(grid, in[i], in[j])) { best = j; break; }
            out.push_back(in[best]); i = best;
        }
        return out;
    }
    static std::vector<BoatVec2> Resample(const std::vector<BoatVec2>& in, float step)
    {
        if (in.size() < 2 || step <= 0.f) return in;
        std::vector<BoatVec2> out; out.push_back(in.front());
        for (size_t i = 1; i < in.size(); ++i)
        {
            const BoatVec2& a = out.back(); const BoatVec2& b = in[i];
            const float d = BoatDist(a, b);
            const int n = std::max(1, int(d / step));
            for (int k = 1; k <= n; ++k)
            {
                const float t = float(k) / float(n);
                out.push_back({a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t});
            }
        }
        return out;
    }
};

} // namespace WarsBots
