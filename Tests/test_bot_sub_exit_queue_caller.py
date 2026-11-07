"""SPEC-006a actual-caller regression for the ladder queue (Codex review of #38 db097209).

Compiles the real BotBrain::ApplyLadderExecutor, UpdateLadderQueue, CancelLadderQueue, LadderQueued, LadderQueuedAhead,
YieldLadderTo and BotManager::LadderQueueAhead/ShouldYieldLadder from the source tree with the mocks of
test_bot_shore_ladder_queue_caller.py, and checks the state paths:
  1. a bot entering the queue with a running UNSTICK chain (jump/strength/back-off): the chain is reset and its jump
     intent cleared; walking to the slot keeps the normal intent (move to the slot), standing stops the bot;
  2. ON -> OFF with a live queue: the next executor tick cancels it, the bot is no longer "queued" (spread/ahead);
  3. interruption by combat (Fire) or a path change cancels; a stale queue (no refresh for > 0.5 s) is not "queued";
  4. cold OFF never queues; the 25 s timeout cancels and cools down.
Engine boundaries are mocked; this is not a game test.
"""
from pathlib import Path
import argparse
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_bot_shore_ladder_queue_caller as base  # noqa: E402

MAIN = r'''
static void Reg(EntityId id, int number, BotBrain& b, CPlayer& a, const Vec3& pos, const NavPathLink& link) {
    a.id = id; a.e.p = pos;
    b.m_number = number; b.m_running = true; b.m_meshPathActive = true; b.m_intent.move = true;
    b.m_blackboard.health = 100; b.m_blackboard.position = pos; b.m_blackboard.grounded = true;
    b.m_path = { Vec3(link.a[0], link.a[1], link.a[2]), Vec3(link.b[0], link.b[1], link.b[2]) };
    b.m_pathLinks = { link }; b.m_pathIndex = 0;
    auto inst = std::make_unique<BotInstance>(); inst->ai = true; inst->brain = &b;
    g_game.mgr.m_bots[id] = std::move(inst);
}

int main() {
    int fails = 0;
    auto check = [&](bool c, const char* m) { if (!c) { std::printf("FAIL: %s\n", m); ++fails; } };
    const NavPathLink ladder{ 0, NavLinkLadder, {2116.12f, 1880.62f, 90.16f}, {2117.50f, 1880.60f, 99.76f}, 4693115ull };
    WarsBots::BotConfig::Get().ladder = 1;
    WarsBots::BotConfig::Get().subExit = 1;
    g_game.mgr.m_bots.clear();
    BotBrain climber, q; CPlayer ac, aq;
    Reg(2, 2, climber, ac, Vec3(2116.2f, 1880.6f, 90.2f), ladder);
    Reg(6, 6, q, aq, Vec3(2111.0f, 1880.6f, 90.2f), ladder);           // 5.1 m from the base, inside the 7 m queue area
    climber.m_now = q.m_now = 1.0f;
    check(climber.ApplyLadderExecutor(ac) && climber.m_ladder.Active(), "#2 starts the ladder");

    // 1. queue entry with a running UNSTICK chain: walking to the 2.5 m slot
    q.m_unstickSteps.step = 2; q.m_unstickStuck = true; q.m_intent.jump = BotJump::Strength; q.m_intent.unstickStrength = true;
    const bool owned = q.ApplyLadderExecutor(aq);
    check(!owned, "walking to the slot: the executor leaves this frame's intent to the normal pipeline");
    check(q.LadderQueued() && q.m_subQueueRef == ladder.ref, "#6 is queued");
    check(q.m_unstickSteps.step == 0 && q.m_unstickSteps.resets > 0 && !q.m_unstickStuck, "running UNSTICK chain reset on queue entry");
    check(q.m_intent.jump == BotJump::None && !q.m_intent.unstickStrength, "unstick jump intent cleared");
    check(q.m_intent.move && std::fabs(q.m_intent.destination.x - (2116.12f - 2.5f)) < 0.2f, "moves to the 2.5 m slot on its approach line");
    check(q.m_unstickSensor.resets > 0, "SPEC-004 sensor muted in the queue");
    // standing in the slot
    q.m_now = 2.0f; q.m_blackboard.position = Vec3(2116.12f - 2.5f, 1880.6f, 90.2f); q.m_intent.move = true;
    const int stops = q.m_locomotion.stops;
    check(q.ApplyLadderExecutor(aq) && q.m_subQueueStanding && q.m_locomotion.stops > stops, "standing in the slot stops the bot");
    check(q.LadderQueued(), "still queued while standing");

    // 2. ON -> OFF with a live queue
    WarsBots::BotConfig::Get().subExit = 0;
    check(!q.LadderQueued(), "flag OFF: readers no longer see a queue");
    q.m_now = 2.2f; q.m_intent.move = true;
    q.ApplyLadderExecutor(aq);
    check(q.m_subQueueRef == 0, "flag OFF: the next executor tick cancels the queue");
    check(g_game.mgr.LadderQueueAhead(ladder.ref, Vec3(2116.12f, 1880.62f, 90.16f), 49.0f, 9) == 1, "flag OFF: only the climber is ahead");
    WarsBots::BotConfig::Get().subExit = 1;

    // 3a. interruption by combat
    q.m_now = 3.0f; q.m_blackboard.position = Vec3(2111.0f, 1880.6f, 90.2f); q.m_intent = BotIntent(); q.m_intent.move = true;
    q.ApplyLadderExecutor(aq); check(q.LadderQueued(), "re-queued");
    q.m_now = 3.1f; q.m_intent.combat = BotCombatIntent::Fire;
    q.ApplyLadderExecutor(aq);
    check(q.m_subQueueRef == 0 && !q.LadderQueued(), "combat cancels the queue");
    // 3b. path change
    q.m_now = 3.2f; q.m_intent = BotIntent(); q.m_intent.move = true;
    q.ApplyLadderExecutor(aq); check(q.LadderQueued(), "re-queued after combat");
    q.m_pathLinks.clear(); q.m_now = 3.3f;
    q.ApplyLadderExecutor(aq);
    check(q.m_subQueueRef == 0, "path without the ladder cancels the queue");
    // 3c. stale: queued, then the executor is not called for a while
    q.m_pathLinks = { ladder }; q.m_now = 4.0f; q.m_intent = BotIntent(); q.m_intent.move = true;
    q.ApplyLadderExecutor(aq); check(q.LadderQueued(), "re-queued after the path change");
    q.m_now = 4.6f;
    check(!q.LadderQueued(), "a queue not refreshed for > 0.5 s is not seen by spread/ahead");

    // 4a. timeout: 25 s in the queue -> cancel + replan + 10 s cooldown
    q.m_now = 4.7f; q.ApplyLadderExecutor(aq);
    q.m_now = q.m_subQueueSince + 25.1f; q.m_intent = BotIntent(); q.m_intent.move = true;
    check(!q.ApplyLadderExecutor(aq) && q.m_subQueueRef == 0 && q.m_subQueueCooldown > q.m_now, "timeout cancels and cools down");
    // 4b. cold OFF never queues
    WarsBots::BotConfig::Get().subExit = 0;
    BotBrain c; CPlayer acold; Reg(7, 7, c, acold, Vec3(2111.0f, 1880.6f, 90.2f), ladder); c.m_now = 1.0f;
    c.ApplyLadderExecutor(acold);
    check(c.m_subQueueRef == 0 && !c.LadderQueued(), "cold OFF: no queue");

    if (fails) { std::printf("SPEC-006a queue caller FAILED (%d)\n", fails); return 1; }
    std::printf("SPEC-006a queue caller PASS\n");
    return 0;
}
'''


def build(repo: Path, out: Path) -> int:
    rd = lambda p: (repo / p).read_text(encoding='utf-8-sig')
    brain, manager = rd('Code/CryGame/Bots/BotBrain.cpp'), rd('Code/CryGame/Bots/BotManager.cpp')
    m = base.method
    t = base.CPP_TEMPLATE
    t = t[:t.index('static void RegisterBot(')]   # the base fixture without its own scenarios
    # Vec3 / intent / brain mock additions used by the real queue code
    t = t.replace('    float GetLengthSquared2D() const { return x * x + y * y; }',
                  '    float GetLengthSquared2D() const { return x * x + y * y; }\n'
                  '    float GetLengthSquared() const { return x * x + y * y + z * z; }\n'
                  '    void Normalize() { const float l = len(); if (l > 0) { x /= l; y /= l; z /= l; } }')
    t = t.replace('    bool linkJump = false, meshPath = false;',
                  '    bool linkJump = false, meshPath = false, unstickStrength = false, unstickClimb = false;\n'
                  '    void MoveTo(const Vec3& d, const Vec3& l) { move = true; destination = d; aim = l; }')
    t = t.replace('#define WBI(field, value)', '#define WBI_MOVETO(...) (m_intent.MoveTo(__VA_ARGS__), m_intent.why.move = (short)__LINE__)\n#define WBI(field, value)')
    t = t.replace('struct BotBrain;\nstruct BotInstance {',
                  'struct MockSensor { int resets = 0; void Reset() { ++resets; } };\n'
                  'struct MockSteps { int step = 0, resets = 0; void Reset() { step = 0; ++resets; } };\n'
                  'struct BotBrain;\nstruct BotInstance {')
    t = t.replace('''    unsigned long long m_subQueueRef = 0; bool m_subQueueStanding = false;   // SPEC-006a (flag OFF in this test)
    bool UpdateLadderQueue(CPlayer&, const NavPathLink&, const Vec3&, const Vec3&, float) { return false; }
    void CancelLadderQueue(const char*) { m_subQueueRef = 0; }''',
                  '''    unsigned long long m_subQueueRef = 0; float m_subQueueSince = 0, m_subQueueCooldown = 0, m_subQueueAt = -1e9f;
    int m_subQueueSlot = -1; bool m_subQueueStanding = false;
    MockSensor m_unstickSensor; MockSteps m_unstickSteps; bool m_unstickStuck = false, m_unstickHasStall = false, m_unstickBackTried = false;
    bool UpdateLadderQueue(CPlayer& actor, const NavPathLink& l, const Vec3& a, const Vec3& feet, float dist2);
    bool LadderQueuedAhead(unsigned long long ref, const Vec3& a, float myDist2, int myNumber) const;
    bool LadderQueued() const;
    void CancelLadderQueue(const char* why);''')
    t = t.replace('    bool ShouldYieldLadder(unsigned long long ref, const Vec3& a, float myDist2, int myNumber) const;',
                  '    bool ShouldYieldLadder(unsigned long long ref, const Vec3& a, float myDist2, int myNumber) const;\n'
                  '    int LadderQueueAhead(unsigned long long ref, const Vec3& a, float myDist2, int myNumber) const;')
    assert 'bool LadderQueued() const;' in t and 'int LadderQueueAhead(' in t and 'MockSteps' in t, 'fixture anchors moved'
    real = [m(brain, 'bool BotBrain::UpdateLadderQueue('), m(brain, 'bool BotBrain::LadderQueued() const'),
            m(brain, 'void BotBrain::CancelLadderQueue('), m(brain, 'bool BotBrain::LadderQueuedAhead('),
            m(manager, 'int BotManager::LadderQueueAhead(')]
    t = t.replace('@YIELD_LADDER_TO@', m(brain, 'bool BotBrain::YieldLadderTo('))
    t = t.replace('@MANAGER_METHODS@', m(manager, 'bool BotManager::AIEnabled(') + '\n' + m(manager, 'bool BotManager::ShouldYieldLadder('))
    t = t.replace('@APPLY_LADDER_EXECUTOR@', m(brain, 'bool BotBrain::ApplyLadderExecutor(') + '\n' + '\n'.join(real))
    t = t.replace('@CANCEL_LADDER@', m(brain, 'void BotBrain::CancelLadder(').replace('void BotBrain::CancelLadder(', 'void BotBrain::CancelLadderBody('))
    t = t.replace('@UPDATE_LADDER@', m(brain, 'void BotBrain::UpdateLadder('))
    t = t.replace('@STEER_CORNERS@', m(brain, 'Vec3 BotBrain::SteerCorners('))
    t += MAIN
    out.mkdir(parents=True, exist_ok=True)
    inc = out / 'include' / 'CryGame' / 'Bots'; inc.mkdir(parents=True, exist_ok=True)
    (inc / 'BotLadder.h').write_text(rd('Code/CryGame/Bots/BotLadder.h'), encoding='utf-8')
    (inc / 'BotCorners.h').write_text(rd('Code/CryGame/Bots/BotCorners.h'), encoding='utf-8')
    unit, exe = out / 'sub_exit_queue_caller.cpp', out / 'sub_exit_queue_caller.exe'
    unit.write_text(t, encoding='utf-8')
    comp = subprocess.run(['cl', '/nologo', '/std:c++17', '/EHsc', '/I', str(out / 'include'), str(unit),
                           '/Fo:' + str(out) + '\\', '/Fe:' + str(exe)], capture_output=True)
    (out / 'compile.log').write_bytes(comp.stdout + comp.stderr)
    if comp.returncode:
        sys.stdout.buffer.write(comp.stdout + comp.stderr)
        raise RuntimeError('Caller fixture must compile; compiler failure is not semantic RED')
    run = subprocess.run([str(exe)], capture_output=True)
    sys.stdout.buffer.write(run.stdout + run.stderr)
    (out / 'run.log').write_bytes(run.stdout + run.stderr)
    return run.returncode


def main() -> int:
    ap = argparse.ArgumentParser(); root = Path(__file__).resolve().parents[1]
    ap.add_argument('--out', type=Path, default=root / 'units' / 'sub-exit-queue-caller')
    a = ap.parse_args()
    return build(root, a.out.resolve())


if __name__ == '__main__':
    raise SystemExit(main())
