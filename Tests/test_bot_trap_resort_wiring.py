"""D-064 wiring guard (pure Python, reads the real sources; not a game test). TRAP4 (package adb16bb8: self-kills at
t990.03, t1097.40 right after LINK keep only_exit, t2969.20) runs a series of jumps and navmesh steps before the
native self-kill. Pins: level 4 starts the series instead of killing, the kill lives only behind TrapResortAction::Kill,
steps are checked on the navmesh, nothing teleports or pushes the bot, every action logs TRAP4 with a position, and
every watchdog reset also resets the series. The series logic itself: Tests/BotTrapWatchdogTests.cpp.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
BOTS = ROOT / 'Code/CryGame/Bots'
fails = []


def check(cond, what):
    print(('PASS ' if cond else 'FAIL ') + what)
    if not cond:
        fails.append(what)


def code(text):
    return re.sub(r'//[^\n]*', '', text)


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


brain = (BOTS / 'BotBrain.cpp').read_text(encoding='utf-8', errors='replace')
header = (BOTS / 'BotBrain.h').read_text(encoding='utf-8', errors='replace')
trap = code(body(brain, 'bool BotBrain::RecoverTrap('))
resort = code(body(brain, 'bool BotBrain::RunTrapResort('))

check('KillPlayer' not in trap, 'RecoverTrap itself never kills: level 4 starts the series')
check('m_trapResort.Begin(p.x,p.y,m_now);' in trap and 'return RunTrapResort(actor,p);' in trap, 'level 4 begins the series and runs it')
check(trap.index('if (m_trapResort.Active())') < trap.index('m_trap.Update('), 'a running series is not fed to the watchdog (a jump would reset it)')
check(resort.count('KillPlayer') == 1, 'one self-kill call')
kill_at = resort.index('KillPlayer')
check(resort.rfind('if (act==TrapResortAction::Kill)', 0, kill_at) != -1, 'the self-kill only on TrapResortAction::Kill')
check('nativeSelfKill=%d' in resort, 'the old nativeSelfKill line stays (audit metrics)')
for tag in ('TRAP4 jump', 'TRAP4 step', 'TRAP4 kill', 'TRAP4 freed'):
    line = next((l for l in resort.splitlines() if tag in l), '')
    check('pos=(%.2f,%.2f,%.2f)' in line, f'{tag} is logged with the position')
check('mesh->PolyAt(q)!=0 && mesh->RayClear(a,q)' in resort, 'a step goes only to a navmesh point with a clear straight walk')
for bad in ('SetPos', 'SetWorldTM', 'AddImpulse', 'pe_action_impulse', 'SetPosRotScale'):
    check(bad not in resort, f'no {bad} in the series (movement requests only)')
check('WBI(jump,BotJump::Normal)' in resort and 'm_blackboard.grounded' in resort, 'a series jump is a normal jump request, grounded only')
check(brain.count('m_trap.Reset(); m_trapStepUntil=0;') == 0, 'every watchdog reset also resets the series')
check(brain.count('m_trap.Reset(); m_trapResort.Reset();') >= 7, 'resets: init, death, level change, ladder queue, disable, freed, series abort')
check('TrapLastResort m_trapResort;' in header and 'bool RunTrapResort(CPlayer& actor, const Vec3& p);' in header, 'members declared')

sys.exit(1 if fails else 0)
