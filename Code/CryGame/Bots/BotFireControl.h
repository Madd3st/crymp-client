// Native trigger policy only: never changes weapon rate, spread or damage.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstring>
namespace WarsBots {
struct BotFireControl {
    bool pulse;
    float holdSeconds, startInterval;
    static BotFireControl Plan(const char* mode, float rpm, float reaction, float burst, float gap, float spinUp=0.0f) {
        const bool pulse=mode && (!std::strcmp(mode,"Single") || !std::strcmp(mode,"Shotgun"));
        const float native=std::isfinite(rpm) && rpm>0 ? 60.0f/rpm : 0.0f;
        // A human trigger release per single/pump shot; native CanFire still enforces
        // ammunition, pump/bolt cycle, readiness and reload. Unknown modes keep bursts.
        const float human=std::max(0.24f,std::min(0.38f,reaction));
        BotFireControl plan=pulse ? BotFireControl{true,0.0f,std::max(native,human)} :
            BotFireControl{false,burst,std::max(gap,burst+0.1f)};
        if (std::isfinite(spinUp) && spinUp>0) {
            plan.pulse=false;
            plan.holdSeconds=std::max(plan.holdSeconds,spinUp+0.1f);
            plan.startInterval=std::max(plan.startInterval,plan.holdSeconds+0.1f);
        }
        return plan;
    }
};
}
