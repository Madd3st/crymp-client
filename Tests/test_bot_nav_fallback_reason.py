"""SPEC-006a step 1 (Codex b2a0a02 p. 5): NAV fallback keeps the original Detour cause next to the sparse-graph one.
Source wiring guard plus the SteelmillTriage parsers on the new line format. Engine-free; not a game test.
"""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[1]
src = (root / 'Code/CryGame/Bots/BotBrain.cpp').read_text(encoding='utf-8-sig')
start = src.index('Vec3 BotBrain::Steer(')
body = src[start:src.index('if (m_path.empty())', start)]

checks = []
def check(name, ok):
    checks.append((name, bool(ok)))

check('defaults declared once', body.count('const char* detourReason = "none";') == 1 and body.count('const char* fallbackReason = "not_run";') == 1)
i_why, i_det, i_fb = body.find('trace.reason = why;'), body.find('detourReason = why;'), body.find('nav.FindPath(feet, goal, m_path, &trace)')
check('detour reason saved before the fallback resets trace', 0 <= i_why < i_det < i_fb)
check('fallback reason taken from the fallback call', re.search(r'found = nav\.FindPath\(feet, goal, m_path, &trace\); fallbackReason = trace\.reason;', body))
fmt = re.search(r'NAV fallback[^"]*"', body)
check('log keeps reason= from= to= expanded= then the two new fields',
      fmt and fmt.group(0).endswith('reason=%s from=%d to=%d expanded=%d detour_reason=%s fallback_reason=%s"'))
check('arguments in format order', 'trace.reason,trace.from,trace.to,trace.expanded,detourReason,fallbackReason);' in body)
check('no behaviour change: retry deadlines untouched', 'm_nextRepath = m_now + (found ? 5.0f : 2.0f);' in body)

sys.path.insert(0, str(root / 'Tools/SteelmillTriage'))
line = ('719.0 [WarsBots] #3 NAV fallback start=(2119.37,1882.61,87.57) goal=(2107.62,1882.50,103.25) retry=2.0. '
        'actor=130973 reason=start_unprojected from=-1 to=12 expanded=0 detour_reason=other_component fallback_reason=start_unprojected')
nav_re = re.compile(re.search(r'r"(NAV fallback [^"]*)"', (root / 'Tools/SteelmillTriage/wblab/navgraph.py').read_text(encoding='utf-8')).group(1))
m = nav_re.search(line)
check('navgraph regex: reason/from/to of the plain field', m and m.groups() == ('start_unprojected', '-1', '12'))
sum_re = re.compile(re.search(r'_REASON = re\.compile\(r"([^"]*)"\)', (root / 'Tools/SteelmillTriage/wblab/summary.py').read_text(encoding='utf-8')).group(1))
check('summary regex: plain reason, not fallback_reason', sum_re.search(line).group(1) == 'start_unprojected')
old = line.split(' detour_reason=')[0]
check('old lines still parse', nav_re.search(old).groups() == ('start_unprojected', '-1', '12'))

for name, ok in checks:
    print(('PASS ' if ok else 'FAIL ') + name)
print('%d/%d' % (sum(ok for _, ok in checks), len(checks)))
raise SystemExit(0 if all(ok for _, ok in checks) else 1)
