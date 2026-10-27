// DS-048 #3: PS-1 capture goal. Engine-free planner: pick the nearest capturable point that is not owned by the
// bot's team, weighted by the point value (nCaptureIndex) and by power generation, then hold it until captured and
// move to the next. The engine adapter (BotPSCapture.cpp) supplies the live owner teams; no engine includes here.
#pragma once
#include <cmath>

namespace WarsBots {

struct PsCapturePoint
{
    int id = 0;              // entity id (flag/factory/alien energy point)
    int ownerTeam = 0;       // 0 = neutral
    int value = 1;           // nCaptureIndex 1..3 (100/200/300 PP per DS-028)
    int captureSeconds = 15; // informational
    bool capturable = true;
    bool power = false;      // AlienEnergyPoint: power is the first objective
    float x = 0, y = 0, z = 0;
};

struct PsCapturePlan
{
    bool valid = false;
    bool hold = false;       // already inside the capture radius
    int id = 0;
    float x = 0, y = 0, z = 0;
};

class BotPSCapture
{
public:
    void Reset() { m_target = 0; }
    int Target() const { return m_target; }

    // Nearest point not owned by `team`; value/power bonus -8 m/-6 m. Keeps the current target while it is still
    // capturable and not beaten by more than `switchMargin` metres (no oscillation between equal points).
    PsCapturePlan Update(const PsCapturePoint* points, int count, int team, float bx, float by, float bz,
                         float holdRadius = 6.f, float switchMargin = 15.f)
    {
        PsCapturePlan plan;
        if (!points || count <= 0 || team == 0) { m_target = 0; return plan; }
        int best = 0, current = 0;
        float bestScore = 1e9f, bestDist = 0, currentScore = 1e9f, currentDist = 0;
        for (int i = 0; i < count; ++i)
        {
            const PsCapturePoint& p = points[i];
            if (!p.capturable || p.ownerTeam == team) continue;
            const float dx = p.x - bx, dy = p.y - by, dz = p.z - bz;
            const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
            const float score = d - 8.0f * float(p.value > 0 ? p.value : 1) - (p.power ? 6.0f : 0.0f);
            if (p.id == m_target) { current = p.id; currentScore = score; currentDist = d; }
            if (score < bestScore) { best = p.id; bestScore = score; bestDist = d; }
        }
        if (!best) { m_target = 0; return plan; }
        // Hysteresis: keep a valid current target unless the alternative is clearly better.
        if (current && best != current && currentScore <= bestScore + switchMargin)
        {
            best = current; bestScore = currentScore; bestDist = currentDist;
        }
        m_target = best;
        for (int i = 0; i < count; ++i)
        {
            if (points[i].id != best) continue;
            plan.valid = true;
            plan.id = best;
            plan.x = points[i].x; plan.y = points[i].y; plan.z = points[i].z;
            plan.hold = bestDist <= holdRadius;
            break;
        }
        return plan;
    }

private:
    int m_target = 0;
};

} // namespace WarsBots
