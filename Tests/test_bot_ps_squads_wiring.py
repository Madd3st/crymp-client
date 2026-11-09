"""SPEC-005 stage 1 wiring guard (engine-free source checks): the commander only runs under wb_bot_ps_squads 1, bunkers
join the target snapshot only under the flag, OFF keeps the old planner path, and the logs carry the fields the spec
metrics read. BotTeamOrdersTests.cpp covers the planning logic itself. Not a game test.
"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
rd = lambda p: (root / p).read_text(encoding='utf-8-sig')
cfg_h, cfg_c = rd('Code/CryGame/Bots/BotConfig.h'), rd('Code/CryGame/Bots/BotConfig.cpp')
cap, brain, brain_h = rd('Code/CryGame/Bots/BotPSCapture.cpp'), rd('Code/CryGame/Bots/BotBrain.cpp'), rd('Code/CryGame/Bots/BotBrain.h')
orders = rd('Code/CryGame/Bots/BotTeamOrders.h')
checks = []
def check(name, ok): checks.append((name, bool(ok)))

check('flag default OFF', re.search(r'int psSquads=0;', cfg_h))
check('cvar wb_bot_ps_squads bound', '{"wb_bot_ps_squads",&BotConfig::psSquads,' in cfg_c)
check('BotTeamOrders.h has no engine includes', not re.search(r'#include\s+"(?!CryGame/Bots/BotTeamOrders)', orders) and 'gEnv' not in orders)
# snapshot: SpawnGroup class looked up only when the flag is on, and the cache key includes the flag
check('bunker class only under the flag', 'IEntityClass* spawnGroup = squads ? ' in cap)
check('snapshot cache keyed by the flag', 'squads==cachedSquads' in cap)
check('bunker needs bCapturable like every target', cap.index('const bool isBunker') < cap.index('if (!capturable) continue;'))
# brain: the squad path is taken only with the flag, before the old planner
up = brain[brain.index('bool BotBrain::UpdatePSCapture('):]
up = up[:up.index('\nbool BotBrain::', 10)]
i_sq = up.find('if (BotConfig::Get().psSquads==1) return UpdatePSSquad(')
check('squad branch only with the flag', i_sq > 0)
check('squad branch after the per-bot snapshot, before the old planner', up.find('PSCaptureSnapshot(m_psPoints') < i_sq < up.find('m_psCapture.Update('))
check('UpdatePSSquad declared', 'bool UpdatePSSquad(CPlayer& actor,float decisionTime,CGameRules& rules,int team,const Vec3& feet);' in brain_h)
sq = brain[brain.index('bool BotBrain::UpdatePSSquad('):]
sq = sq[:sq.index('\nbool BotBrain::', 10)]
check('order copied (commander vector may change)', 'const TeamOrder order=*found;' in sq)
check('stage 1 stands inside the native area', 'IsPointWithin(feet)' in sq and 'WBI(move,false);' in sq)
check('no SetPos/impulse in the squad path', not re.search(r'SetPos|AddImpulse|SetWorldTM', sq))
# playtest 09.11: a bot without its own path must not stand for the whole match (US sub lower deck, KNDR pit)
i_fb = sq.find('if(order.kind!=PsKind::Spawn&&!reachable)return false;')
check('no own path: back to the usual chain (unstick/roam/swim), not a stand',
      i_fb > 0 and 'reason=bot_unreachable fallback=roam.' in sq
      and i_fb < sq.find('bool inside=false;') < sq.find('WBI(move,false);'))
check('stand only for the team spawn hold or inside the zone', 'if(!reachable||inside)' in sq)
# logs
check('PS ORDER fields', re.search(r'PS ORDER team=%d bot=%u target=%d kind=%s role=%s pair=%d reason=%s', cap))
check('PS ORDER unreachable once per state change', 'reason=unreachable.' in cap and 'r->second != t.reachable' in cap)
check('PS ORDER cache with time', re.search(r'PS ORDER cache targets=%d .* ms=%\.1f', cap))
check('PS OWN fields', re.search(r'PS OWN team=%d target=%d kind=%s owned=%\.0f lost=%d recaptured=%d', cap))
check('PS FIGHT over (combat-over definition)', 'PS FIGHT over' in sq and 'CombatOver(' in sq)
check('owner 08.11: no energy gate for the prototype', 'TeamEnergy' not in cap and 'prototypeFrom = 4' in orders)
check('path cache built once, not per plan', cap.count('BuildCache(c, *mesh, points, rules);') == 1)

# D-059 (owner 09.11): defenders only when no neutral point is left, at most 2, released after 45 s without an enemy
check('D-059 config: at most 2 defenders, release after 45 s, TAC not detected here',
      'int maxDefenders = 2;' in orders and 'float releaseAfter = 45.f;' in orders and 'bool teamHasTac = false;' in orders)
check('D-059: neutral point left -> no defender except the prototype', 'n >= cfg.defendFrom && !neutralLeft' in orders and 'n >= cfg.prototypeFrom && !cfg.teamHasTac' in orders)
check('D-059: a quiet defender leaves at once with reason=quiet', 'o.reason = "quiet";' in orders and '!freed &&' in orders)
check('D-059 runtime: enemy within 40 m of an owned point, capture counts as contact',
      '40.f * 40.f' in cap and 'c.teams[team].lastEnemy[p.id] = now;' in cap and 't.quiet = now - e->second;' in cap)

for name, ok in checks: print(('PASS ' if ok else 'FAIL ') + name)
print('%d/%d' % (sum(ok for _, ok in checks), len(checks)))
raise SystemExit(0 if all(ok for _, ok in checks) else 1)
