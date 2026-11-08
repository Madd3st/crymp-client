"""SPEC-006a wiring guard (engine-free source checks): the ladder queue and the no-penalty rule run only under
wb_bot_sub_exit 1, the memory filter only reads (no file rewrite), the queue mutes the SPEC-004 sensor and never moves
the bot by SetPos/impulse. BotLadderTests.cpp covers the helpers. Not a game test.
"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
rd = lambda p: (root / p).read_text(encoding='utf-8-sig')
cfg_h, cfg_c, brain, nav, core = (rd('Code/CryGame/Bots/BotConfig.h'), rd('Code/CryGame/Bots/BotConfig.cpp'),
    rd('Code/CryGame/Bots/BotBrain.cpp'), rd('Code/CryGame/Bots/BotNavMesh.cpp'), rd('Code/CryGame/Bots/BotNavMeshCore.cpp'))
mgr = rd('Code/CryGame/Bots/BotManager.cpp')
checks = []
def check(name, ok): checks.append((name, bool(ok)))
def body(src, sig):
    s = src[src.index(sig):]; return s[:s.index('\n}\n') if '\n}\n' in s else len(s)]

check('flag default OFF', 'int subExit=0;' in cfg_h)
check('cvar wb_bot_sub_exit bound', '{"wb_bot_sub_exit",&BotConfig::subExit,' in cfg_c)
ex = body(brain, 'bool BotBrain::ApplyLadderExecutor(')
check('queue only under the flag, before the 2 m gate',
      ex.find('BotConfig::Get().subExit==1 && UpdateLadderQueue(') >= 0 and ex.find('UpdateLadderQueue(') < ex.find('if (dist2>4.0f'))
q = body(brain, 'bool BotBrain::UpdateLadderQueue(')
check('queue only for slow ladders', 'SlowLadderLink(true,l.a,l.b)' in q)
check('queue mutes the SPEC-004 sensor', 'm_unstickSensor.Reset();' in q)
check('queue timeout replans and cools down', 'LadderQueueTimedOut(m_subQueueSince,m_now)' in q and 'm_nextRepath=m_minRepath=0' in q and 'm_subQueueCooldown=m_now+10.0f' in q)
check('no SetPos/impulse in the queue', not re.search(r'SetPos|AddImpulse|SetWorldTM', q))
check('SUB QUEUE log fields', 'SUB QUEUE slot=%.1f ladder=%llu ahead=%d waited=%.1f' in q and 'SUB QUEUE timeout ladder=%llu waited=%.1f' in q)
at = body(brain, 'bool BotBrain::UnstickAtSlowLadder(')
check('no-penalty rule only under the flag', 'BotConfig::Get().subExit!=1' in at)
check('back-off ban skipped at a slow ladder start', 'if (UnstickAtSlowLadder(mesh)) {' in brain and brain.index('if (UnstickAtSlowLadder(mesh)) {') < brain.index('NavCostBlock b;'))
check('memory not recorded at a slow ladder start (both calls)', brain.count('&& !UnstickAtSlowLadder(mesh))') == 2)
check('memory filter is read-only and follows the flag', nav.count('SetLadderMemoryFilter(BotConfig::Get().subExit == 1)') == 2 and 'SaveUnstickMemory' not in body(core, 'void NavMeshCore::SetTransitionMemory('))
check('filter applied when reading memory', 'm_ladderMemoryFilter && NearSlowLadderStart(to)' in core)
check('spread ignores allies in a ladder queue', 'if (brain.LadderQueued()) continue;' in mgr)

for name, ok in checks: print(('PASS ' if ok else 'FAIL ') + name)
print('%d/%d' % (sum(ok for _, ok in checks), len(checks)))
raise SystemExit(0 if all(ok for _, ok in checks) else 1)
