"""Offline SPEC-004 trace parser. Does not launch processes or mutate traces."""
import argparse
import json
import math
from pathlib import Path
import re

TIME=re.compile(r'PATHTIME (window|level|level-end) level=(\S+) n=(\d+) p50=(\S+) p95=(\S+) max=(\S+) ms dropped=(\d+) unstick=(\d+) filter=(\w+)')
EVENT=re.compile(r'\bUNSTICK (step=|exit\b|failed\b|abort\b)')
def parse_trace(text, mode, level):
    events=dict(step=0,exit=0,failed=0,abort=0)
    reports=[]
    for line in text.splitlines(keepends=True):
        if not line.endswith('\n'):continue  # ignore a live/truncated last row
        event=EVENT.search(line)
        if event:events[event[1].rstrip('=')]+=1
        m=TIME.search(line)
        if not m or int(m[8])!=mode or m[2]!=level:continue
        reports.append(dict(scope=m[1],level=m[2],n=int(m[3]),p50=float(m[4]),p95=float(m[5]),max=float(m[6]),dropped=int(m[7]),unstick=int(m[8]),filter=m[9]))
    # Select only this run's completed level: a later level report invalidates an older end.
    candidates=[r for r in reports if r['scope']!='window']
    selected=candidates[-1] if candidates else None
    gate='NOT_MEASURED'
    if selected and selected['scope']=='level-end':
        valid=(selected['n']>0 and selected['dropped']==0 and selected['filter']=='virtual'
               and all(math.isfinite(selected[k]) for k in ('p50','p95','max'))
               and 0<=selected['p50']<=selected['p95']<=selected['max'])
        gate=('PASS' if selected['p95']<=2 else 'FAIL') if valid else 'INVALID'
    return dict(gate=gate,selected=selected,reports=reports,events=events,
                limits='Single trace only; counts are events, not physical successes. Requires level-end; no fallback to level.')
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('trace',type=Path);ap.add_argument('--unstick',type=int,choices=[0,1],required=True)
    ap.add_argument('--level',required=True)
    a=ap.parse_args();result=parse_trace(a.trace.read_text(encoding='utf-8-sig',errors='replace'),a.unstick,a.level)
    print(json.dumps(result,indent=2));return 0 if result['gate']=='PASS' else 2
if __name__=='__main__':raise SystemExit(main())
