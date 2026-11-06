"""SPEC-004 wiring guard (pure Python, reads the real sources). Not a runtime caller: it pins the four fixes from the
Codex review of 96a09959 so a later edit cannot silently undo them.
  1. the sensor counts progress only along one path (no ApplyPathSignature inside UnstickSensor);
  2. the Strength step never sets linkJump (which skips JumpSpace) and both jump steps are offered only after the
     executor's own clearance probe (CanJump -> JumpSpace);
  3. Detour penalties decay per query by the memory clock, and the clock is saved without new records;
  4. allies are the same non-zero team only (Instant Action has none).
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
BOTS = ROOT / 'Code/CryGame/Bots'


def code(text):
    return re.sub(r'//[^\n]*', '', text)   # comments may name what the code must not do


def body(text, signature):
    start = text.index(signature)
    left = text.index('{', start)
    depth = 0
    for i in range(left, len(text)):
        depth += text[i] == '{'
        depth -= text[i] == '}'
        if not depth:
            return text[start:i + 1]
    raise ValueError(signature)


def main():
    fails = []
    def check(ok, what):
        print(('PASS ' if ok else 'FAIL ') + what)
        if not ok:
            fails.append(what)

    unstick = (BOTS / 'BotUnstick.h').read_text(encoding='utf-8-sig')
    sensor = code(body(unstick, 'class UnstickSensor'))
    check('ApplyPathSignature' not in sensor, 'sensor: no legacy restart rule (alternating replans give no progress)')
    check(re.search(r'm_sig\s*=\s*sig;\s*m_best\s*=\s*rem;\s*\}', sensor) is not None, 'sensor: new path = new baseline, same clock')

    brain = (BOTS / 'BotBrain.cpp').read_text(encoding='utf-8-sig')
    upd = code(body(brain, 'void BotBrain::UpdateUnstick('))
    check('linkJump' not in upd, 'steps: Strength never sets linkJump (JumpSpace stays on)')
    check(re.search(r'UnstickJumpInput\(needJump,needJump && m_locomotion\.CanJump\(actor,forward,false\),\s*needJump && m_locomotion\.CanJump\(actor,forward,true\)', upd) is not None, 'steps: each jump step gets its own real probe')
    check('in.canJump=ability.canJump; in.canStrength=ability.canStrength;' in upd, 'steps: abilities come from UnstickJumpInput')
    begin = code(body(unstick, 'void Begin('))
    check('!in.canStrength || StrengthUsed' in begin and '!in.canJump || !in.canStrength' not in begin, 'steps: a failed normal probe does not veto Strength')
    check('m_blackboard.mode==NANOMODE_STRENGTH' in upd, 'steps: Strength jump only once the suit is in Strength')
    loco_h = (BOTS / 'BotLocomotion.h').read_text(encoding='utf-8-sig')
    loco = (BOTS / 'BotLocomotion.cpp').read_text(encoding='utf-8-sig')
    check('!intent.linkJump && !JumpSpace(' in loco and 'unstickStrength' not in loco, 'executor: only baked links skip JumpSpace')

    core = (BOTS / 'BotNavMeshCore.cpp').read_text(encoding='utf-8-sig')
    check('c->memoryClock' in body(core, 'float NavMeshCore::SegmentCostMultiplier('), 'Detour: penalty decays per query')
    check('cost.memoryClock = UnstickClock(' in brain, 'path query passes the memory clock')
    nav = (BOTS / 'BotNavMesh.cpp').read_text(encoding='utf-8-sig')
    check('if (s.clock.Due(now)) SaveUnstickMemory(' in body(nav, 'float UnstickClock('), 'clock saved every 60 s without records')

    mgr = (BOTS / 'BotManager.cpp').read_text(encoding='utf-8-sig')
    allies = body(mgr, 'int BotManager::UnstickAllies(')
    check('if (myTeam <= 0) return 0;' in allies and 'UnstickAlly(myTeam' in allies, 'allies: same non-zero team, none in IA')
    check(re.search(r'GetEntityId\(\)\s*==\s*me', allies) is not None and 'bot->GetActor()' in allies, 'allies: own actor skipped, owned actors only')
    check(re.search(r'queryStart = std::chrono::steady_clock::now\(\);\s*found = mesh->FindPathLinks\(.*?\);\s*PathQueryTimed\(', brain, re.S) is not None,
          'D-042: every FindPathLinks of the brain is timed')
    check(brain.count('mesh->FindPathLinks(') == 1, 'D-042: no untimed FindPathLinks in the brain')
    timed = code(body(nav, 'void PathQueryTimed('))
    check('BotConfig::Get().unstick' not in timed, 'D-042: timing does not depend on wb_bot_unstick (control run measured too)')
    check('LogPathTimes(s, "level-end"' in nav, 'D-042: level-end report before the reset')
    # Server run 3ba5119c (Codex 06.11): stalls 16/17/10 s, crowd of 4
    check('in.progressed' not in upd and 'in.escaped=chain && UnstickEscaped(m_unstickFrom.x,m_unstickFrom.y,m_unstickTo.x,m_unstickTo.y,feet.x,feet.y);' in upd, 'exit = physical escape toward the corner, not remaining-path jitter')
    check('m_unstickSensor.Update(rem,PathSignature(m_path),m_now,muted,xy);' in upd, 'sensor gets the position (place rule)')
    steps = code(body(unstick, 'class UnstickSteps'))
    check('trying && in.escaped' in steps, 'no exit inside back-off or wait')
    check('UnstickPickBackoff(' in upd and 'WBI_MOVETO(m_unstickBack' in upd, 'back-off target picked away from allies')
    check('JumpSpace(actor,direction,intent.move,strength,intent.unstickClimb)' in loco and 'm_intent.unstickClimb=true' in upd, 'exit jumps use the slope-aware probe in the executor too')
    check('return JumpSpace(actor, direction, true, strength, true);' in loco_h, 'CanJump asks the same climb probe')

    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
