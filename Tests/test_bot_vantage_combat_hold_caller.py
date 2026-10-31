"""Execute actual BotBrain::TryVantageCombat with real BotVantage.h.

Verifies:
  1. Post-fix (--expect new): when BotVantage is in VantagePhase::Hold and the target is at distance > 12m,
     breaking line of sight (visible == false) preserves the held vantage spot (m_intent.move == false,
     m_intent.combat == Aim, returns true) instead of returning false (which falls through to UpdateChasing).
  2. Pre-fix (--expect old): when visible == false, the old condition returned false, leading to UpdateChasing
     and running off the roof toward the ground target.
  3. When visible == true and distance > 12m, the bot fires from the held spot (returns true, move == false).
  4. When distance <= 12m (CQB threat on the roof), returns false so close-quarters combat takes over.
"""
from pathlib import Path
import argparse
import subprocess
import sys


def method(text, signature):
    start = text.index(signature)
    left = text.index('{', start)
    depth = 0
    for i in range(left, len(text)):
        depth += (text[i] == '{') - (text[i] == '}')
        if not depth:
            return text[start:i + 1]
    raise ValueError('Unclosed method: ' + signature)


CPP_HARNESS = r"""
#include "CryGame/Bots/BotVantage.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

using WarsBots::VantagePhase;
using EntityId = unsigned;

struct Vec3 {
    float x = 0.f, y = 0.f, z = 0.f;
    Vec3() = default;
    Vec3(float a, float b, float c) : x(a), y(b), z(c) {}
    Vec3 operator+(const Vec3& b) const { return Vec3(x + b.x, y + b.y, z + b.z); }
    Vec3 operator-(const Vec3& b) const { return Vec3(x - b.x, y - b.y, z - b.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    float GetLengthSquared() const { return x * x + y * y + z * z; }
    float GetLength() const { return std::sqrt(x * x + y * y + z * z); }
};

constexpr int STANCE_STAND = 0;
constexpr int STANCE_PRONE = 2;

enum class BotSkill { Beginner = 0, Normal, Veteran, Elite };
enum class BotGait { Walk = 0, Run, Sprint };
enum class BotCombatIntent { None = 0, Aim, Fire };

struct IEntity {
    Vec3 pos;
    Vec3 GetWorldPos() const { return pos; }
};

struct CPlayer {
    EntityId id = 101;
    IEntity entity;
    IEntity* GetEntity() { return &entity; }
    EntityId GetEntityId() const { return id; }
};

struct BotInstance {
    CPlayer* actor = nullptr;
};

struct BotConfigData {
    int vantage = 1;
    int highGround = 0;
};

struct BotConfig {
    static BotConfigData& Get() { static BotConfigData c; return c; }
};

struct BotTraits {
    float camper = 0.9f;
    float prone = 0.8f;
};

struct BotProfile {
    float reaction = 0.1f;
};

struct MockCombat {
    bool shotBlocked = false;
    float selectedAt = 0.f;
    bool ShotBlocked() const { return shotBlocked; }
    float SelectedAt() const { return selectedAt; }
};

enum class State { SEARCHING_ENEMY = 0, ATTACKING, CHASING_ENEMY };

struct BotIntent {
    bool move = true;
    Vec3 aim;
    int stance = STANCE_STAND;
    BotCombatIntent combat = BotCombatIntent::None;
};

#define WBI(field, val) m_intent.field = (val)

struct MockBotBrain {
    float m_now = 10.f;
    float m_stateTime = 9.f;
    State m_state = State::SEARCHING_ENEMY;

    BotTraits m_traits;
    BotProfile m_profile;
    MockCombat m_combat;
    BotIntent m_intent;
    WarsBots::BotVantage m_vantage;

    void Change(State s) { m_state = s; }
    void UpdateCombatPost(BotInstance&, CPlayer&, float) {}

    bool TryVantageCombat(BotInstance& bot, CPlayer& actor, const Vec3& aim, float distance, bool visible, float decisionTime);
};
"""

MAIN_CPP = r"""
int main(int argc, char** argv)
{
    bool expectNew = true;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--expect-old") expectNew = false;

    const char* spotText = "1301.7 1318.3 161.5 4 0 0 0 200 292\n";

    // Test 1: VantagePhase::Hold, visible enemy 50m away -> handled on spot (move==false), fires.
    {
        MockBotBrain b;
        CPlayer actor; actor.entity.pos = Vec3(1301.7f, 1318.3f, 161.5f); // Roof spot
        BotInstance inst; inst.actor = &actor;
        WarsBots::VantageMap map;
        map.Load(spotText);
        b.m_vantage.Update(map, 5, 0.f, 1301.7f, 1318.3f, 161.5f, 0, [](const WarsBots::VSpot&){ return true; }); // Arrive & Hold
        assert(b.m_vantage.Phase() == VantagePhase::Hold);

        b.m_intent.move = true;
        Vec3 aim(1300.f, 1370.f, 136.f);
        bool handled = b.TryVantageCombat(inst, actor, aim, 50.f, true, 0.1f);

        assert(handled);
        assert(!b.m_intent.move);
        assert(b.m_intent.combat == BotCombatIntent::Fire);
        assert(b.m_state == State::ATTACKING);
        assert(b.m_intent.stance == STANCE_PRONE);
    }

    // Test 2: VantagePhase::Hold, enemy drops behind cover (visible==false), distance 50m.
    {
        MockBotBrain b;
        CPlayer actor; actor.entity.pos = Vec3(1301.7f, 1318.3f, 161.5f); // Roof spot
        BotInstance inst; inst.actor = &actor;
        WarsBots::VantageMap map;
        map.Load(spotText);
        b.m_vantage.Update(map, 5, 0.f, 1301.7f, 1318.3f, 161.5f, 0, [](const WarsBots::VSpot&){ return true; });
        assert(b.m_vantage.Phase() == VantagePhase::Hold);

        b.m_intent.move = true;
        Vec3 aim(1300.f, 1370.f, 136.f);
        bool handled = b.TryVantageCombat(inst, actor, aim, 50.f, false, 0.1f);

        if (expectNew)
        {
            if (!handled || b.m_intent.move || b.m_intent.combat != BotCombatIntent::Aim)
            {
                std::fprintf(stderr, "FAIL: expected TryVantageCombat to handle non-visible enemy on spot (handled=%d, move=%d)\n",
                    handled, b.m_intent.move);
                return 1;
            }
            assert(b.m_state == State::SEARCHING_ENEMY);
        }
        else
        {
            // Old code required visible==true; for visible==false it returned false
            if (handled)
            {
                std::fprintf(stderr, "FAIL: expected old code to return false when visible==false\n");
                return 1;
            }
        }
    }

    // Test 3: CQB threat within 12m (e.g. enemy jumped onto roof at 8m) -> returns false for CQB
    {
        MockBotBrain b;
        CPlayer actor; actor.entity.pos = Vec3(1301.7f, 1318.3f, 161.5f);
        BotInstance inst; inst.actor = &actor;
        WarsBots::VantageMap map;
        map.Load(spotText);
        b.m_vantage.Update(map, 5, 0.f, 1301.7f, 1318.3f, 161.5f, 0, [](const WarsBots::VSpot&){ return true; });
        assert(b.m_vantage.Phase() == VantagePhase::Hold);

        Vec3 aim(1305.f, 1318.3f, 161.5f);
        bool handled = b.TryVantageCombat(inst, actor, aim, 8.f, true, 0.1f);
        assert(!handled); // Must not handle at <= 12m; falls through to close-quarters combat
    }

    std::puts("PASS: bot vantage combat hold caller test passed");
    return 0;
}
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--expect', choices=['new', 'old'], default='new')
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parent.parent
    code_dir = repo_root / 'Code'
    brain_cpp = (code_dir / 'CryGame/Bots/BotBrain.cpp').read_text(encoding='utf-8')
    try_vantage = method(brain_cpp, 'bool BotBrain::TryVantageCombat')

    # Replace class qualifier for standalone struct MockBotBrain
    try_vantage_adapted = try_vantage.replace('bool BotBrain::TryVantageCombat', 'bool MockBotBrain::TryVantageCombat')

    units_dir = repo_root / 'units'
    units_dir.mkdir(parents=True, exist_ok=True)

    cpp_src = units_dir / 'test_bot_vantage_combat_hold_caller.cpp'
    exe = units_dir / 'test_bot_vantage_combat_hold_caller.exe'

    full_cpp = CPP_HARNESS + '\n' + try_vantage_adapted + '\n' + MAIN_CPP
    cpp_src.write_text(full_cpp, encoding='utf-8')

    cmd_compile = [
        'cl', '/nologo', '/EHsc', '/std:c++17',
        f'/I{code_dir}',
        str(cpp_src),
        f'/Fe:{exe}',
        f'/Fo:{units_dir}/'
    ]

    res = subprocess.run(cmd_compile, capture_output=True, text=True)
    if res.returncode != 0:
        print("Compile failed:\n" + res.stderr + "\n" + res.stdout)
        sys.exit(1)

    cmd_run = [str(exe)]
    if args.expect == 'old':
        cmd_run.append('--expect-old')

    run_res = subprocess.run(cmd_run, capture_output=True, text=True)
    print(run_res.stdout)
    if run_res.returncode != 0:
        print(run_res.stderr)
        sys.exit(run_res.returncode)


if __name__ == '__main__':
    main()
