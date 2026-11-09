"""SPEC-005b wiring guard (engine-free source checks): the defender position runs only under wb_bot_ps_hold 1 inside the
SPEC-005 squad path, the prototype-until-TAC rule reaches the planner only under the flag and before TAC, logs carry the
fields the spec PASS table reads, no SetPos/impulse and no level coordinates. BotPsHoldTests.cpp and
BotTeamOrdersTests.cpp cover the logic. Not a game test.
"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
rd = lambda p: (root / p).read_text(encoding='utf-8-sig')
cfg_h, cfg_c = rd('Code/CryGame/Bots/BotConfig.h'), rd('Code/CryGame/Bots/BotConfig.cpp')
cap, brain, brain_h = rd('Code/CryGame/Bots/BotPSCapture.cpp'), rd('Code/CryGame/Bots/BotBrain.cpp'), rd('Code/CryGame/Bots/BotBrain.h')
hold, orders, flags = rd('Code/CryGame/Bots/BotPsHold.h'), rd('Code/CryGame/Bots/BotTeamOrders.h'), rd('Docs/FLAGS.md')
checks = []
def check(name, ok): checks.append((name, bool(ok)))
def body(src, sig):
    s = src[src.index(sig):]
    return s[:s.index('\nbool BotBrain::', 10) if '\nbool BotBrain::' in s[10:] else len(s)]

check('flag default OFF', re.search(r'int psHold=0;', cfg_h))
check('cvar wb_bot_ps_hold bound', '{"wb_bot_ps_hold",&BotConfig::psHold,' in cfg_c)
check('flag in FLAGS.md as OFF', '| `wb_bot_ps_hold` | OFF |' in flags)
check('BotPsHold.h is engine-free', not re.search(r'#include\s+"', hold) and 'gEnv' not in hold)
check('planner rule off by default', 'int prototypeHoldFrom = 0;' in orders)
check('planner rule only with the flag and before TAC', "if (BotConfig::Get().psHold == 1 && !tr.tac) cfg.prototypeHoldFrom = 4;" in cap)
check('planner gets the cfg', 'PlanTeamOrders(targets, bots, team, now, tr.pending, tr.orders, cfg);' in cap)
check('hold scan only with the flag', 'if (BotConfig::Get().psHold == 1 && now >= c.nextHoldScan) HoldScan(c, rules, now);' in cap)
check('TAC from the stock inventory class', 'GetCountOfClass("TACGun")' in cap)
check('claims dropped for bots off the roster', 'tr.holdPos.erase(h)' in cap)
sq = body(brain, 'bool BotBrain::UpdatePSSquad(')
i_hold = sq.find('const bool holdPath=BotConfig::Get().psHold==1&&reachable&&order.hold&&point->ownerTeam==team;')
check('hold path only with the flag, a hold order on an own reachable point', i_hold > 0)
check('hold path before the stage-1 stand-inside branch', 0 < i_hold < sq.find('if(!reachable||inside)'))
check('flag OFF / other task resets the hold state', 'if(!holdPath&&m_psHoldTarget)ResetPSHold(team);' in sq)
hp = body(brain, 'bool BotBrain::UpdatePSHold(')
check('enemy in the zone falls back to stage 1 (go in, fight)', 'if(info.contested)' in hp and 'return false;' in hp)
check('no candidate falls back to stage 1', 'if(!m_psHoldHas)return false;' in hp)
check('candidates on the navmesh, path from the centre', 'mesh->Project(q,s,' in hp and 'mesh->FindPath(cs,s,xyz)' in hp)
check('only sector directions are cast', 'SectorDirections(ap,cfg.sectorDeg)' in hp)
check('cover = low ray blocked, high ray free', 'h.cover=lowBlocked&&highFree;' in hp)
check('energy turns on the higher-first rule', 'point.kind==int(PsKind::Energy)' in hp)
check('timer re-arms every pick (no ping-pong)', 'm_psHoldTimer.Assign(m_now,HoldPeriod(Rand01(),cfg));' in hp)
check('no SetPos/impulse in the hold path', not re.search(r'SetPos|AddImpulse|SetWorldTM', hp + sq))
check('no level coordinates in the hold code', not re.search(r'\b(19|20|21|22)\d\d\.\d', hp + hold))
check('PS HOLD fields', re.search(r'PS HOLD bot=%u target=%d pos=\(%\.1f,%\.1f,%\.1f\) score=%d cover=%d up=%\.1f sector=%d reason=%s', hp))
check('PS HOLD reasons capture/back', 'reason=capture' in hp and 'reason=back' in hp)
check('PS HOLD tac log', 'PS HOLD tac team=%d by=%u' in cap)
check('back after combat over', 'm_psHoldBack=true;' in sq)
check('declared in BotBrain.h', 'bool UpdatePSHold(CPlayer& actor,const PsCapturePoint& point,int team,const Vec3& feet);' in brain_h)

# Codex #44 BLOCK: arrival on the hold position includes the height (upper floor / platform)
check('hold arrival is 3D (HoldArrived), XY-only arrival gone', 'if(!HoldArrived(off.x,off.y,off.z))' in brain and 'const Vec3 off(m_psHoldPos.x-feet.x,m_psHoldPos.y-feet.y,m_psHoldPos.z-feet.z);' in brain and 'm_psHoldPos.y-feet.y,0);' not in brain)

for name, ok in checks: print(('PASS ' if ok else 'FAIL ') + name)
print('%d/%d' % (sum(ok for _, ok in checks), len(checks)))
raise SystemExit(0 if all(ok for _, ok in checks) else 1)
