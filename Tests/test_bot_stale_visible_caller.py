"""Audit 14.11 p. 5 caller guard (pure Python, reads the real BotBrain.cpp): the brain clears the blackboard sight at
the start of every decision tick, before any branch that can own the tick and return (ladder, test, weapon seek,
first primary, upgrade), and both readers go through the tested helpers. BotAuditFixesTests.cpp covers the logic.
Not a game test.
"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
brain = (root / 'Code/CryGame/Bots/BotBrain.cpp').read_text(encoding='utf-8-sig')
code = re.sub(r'//[^\n]*', '', brain)
checks = []
def check(name, ok): checks.append((name, bool(ok)))

reset = 'BeginTickSight(m_blackboard.visible,m_blackboard.distance);'
check('one reset per tick', code.count(reset) == 1)
i = code.find(reset)
tick = code.rfind('m_intent=BotIntent();', 0, i)
check('reset right after the intent is cleared (10 Hz decision tick)', 0 < tick < i and i - tick < 40)
for owner in ('UpdateLadder(actor,decisionTime); return;', 'if(UpdateTest(actor,decisionTime))return;',
              'if(UpdateWeaponSeek(actor,decisionTime,socomQA))return;', 'UpdateUpgrade(bot,actor,enemy,decisionTime,socomQA,true)) return;',
              'if(UpdateUpgrade(bot,actor,enemy,decisionTime,socomQA))return;', 'UpdateCombat(bot,actor,decisionTime);'):
    j = code.find(owner, tick)
    check('reset before: ' + owner, i < j)
check('the fight still writes the sight', 'm_blackboard.visible=visible;' in code and 'm_blackboard.distance=distance;' in code)
check('unstick reads through the helper', 'UnstickCombatMute(m_blackboard.visible,m_state==State::ATTACKING,m_intent.combat!=BotCombatIntent::None)' in code)
check('pro_fire reads through the helper',
      'ProFireBlocksSprint(BotConfig::Get().proFire==1,m_blackboard.visible,m_blackboard.distance,m_profile.holdRange,m_intent.jump!=BotJump::None,m_intent.gait==BotGait::Sprint)' in code)
readers = [m.start() for m in re.finditer(r'm_blackboard\.visible(?!=)', code)]
check('no other raw reader of m_blackboard.visible', len(readers) == 3)   # the reset and the two helpers

for name, ok in checks: print(('PASS ' if ok else 'FAIL ') + name)
print('%d/%d' % (sum(ok for _, ok in checks), len(checks)))
raise SystemExit(0 if all(ok for _, ok in checks) else 1)
