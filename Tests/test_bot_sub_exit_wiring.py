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
loco, player, intent_h = rd('Code/CryGame/Bots/BotLocomotion.cpp'), rd('Code/CryGame/Actors/Player/Player.cpp'), rd('Code/CryGame/Bots/BotIntent.h')
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

# SPEC-006a p. 4 (D-058): swim-out under the hull
st = body(brain, 'Vec3 BotBrain::Steer(')
check('swim starts only with the flag, swimming, a Detour start failure, solid above and open water found',
      "BotConfig::Get().subExit == 1 && !m_subSwim" in st and 'm_blackboard.swimming' in st and '!std::strncmp(detourReason, "start_", 6)' in st
      and 'mesh->SolidAbove(p) && mesh->NearestOpenWater(p, 40.f, w)' in st)
check('swim success is the path found again (physical), not a timer', 'if (found && m_subSwim)' in st and 'SUB SWIM done' in st)
sw = body(brain, 'Vec3 BotBrain::SubSwimStep(')
check('swim ends on flag OFF / leaving water / 30 s with a replan and cooldown', 'BotConfig::Get().subExit != 1' in sw and '!m_blackboard.swimming' in sw and 'm_now - m_subSwimSince > 30.f' in sw and 'm_subSwimCooldown = m_now + 30.f' in sw)
check('no SetPos/impulse in the swim', not re.search(r'SetPos|AddImpulse|SetWorldTM', sw))
check('swim target comes from the navmesh, no coordinates', not re.search(r'21[01]\d\.|18[89]\d\.', sw + st[st.find('SUB SWIM start')-400:st.find('SUB SWIM start')]))
check('swim descent is the native ACTION_CROUCH, not a pitched aim/target',
      'm_subSwimDown = mesh->SolidAbove(p);' in sw and 'return Vec3(m_subSwimTarget.x, m_subSwimTarget.y, feet.z);' in sw and '-2.3f' not in sw)
check('descent requested only while swimming out, flag ON',
      'm_intent.swimDown = m_subSwim && m_subSwimDown && m_blackboard.swimming && BotConfig::Get().subExit == 1;' in brain
      and 'subExit == 1;\n\tif (BotConfig::Get().botTurnCap==1)' in brain)
check('descent default off in the intent', 'bool swimDown = false;' in intent_h)
check('locomotion sets/clears the descent every tick, only in water, before the canMove refusal',
      'actor.SetWarsBotSwimDown(intent.swimDown && state.waterMovement);' in loco
      and loco.index('SetWarsBotSwimDown(intent.swimDown') < loco.index('if (!state.canMove)')
      and 'actor.SetWarsBotSwimDown(false);' in body(loco, 'bool BotLocomotion::ExecuteLadder('))
sd = body(player, 'void CPlayer::SetWarsBotSwimDown(')
check('SetWarsBotSwimDown is server-side and owned-bot only, touches only ACTION_CROUCH',
      "if (!gEnv->bServer || !g_pGame->GetBotManager().Owns(GetEntityId())) return;" in sd
      and 'm_actions|=ACTION_CROUCH' in sd and 'm_actions&=~ACTION_CROUCH' in sd and not re.search(r'SetPos|AddImpulse|Physics|m_stance', sd))
check('descent cleared on swim end and reset', brain.count('m_subSwimDown = false') >= 2 and 'm_subSwim=false; m_subSwimDown=false;' in brain)
for name, ok in checks: print(('PASS ' if ok else 'FAIL ') + name)
print('%d/%d' % (sum(ok for _, ok in checks), len(checks)))
raise SystemExit(0 if all(ok for _, ok in checks) else 1)
