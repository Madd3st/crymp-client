"""Archive-only C153 verification; never launches a game or changes live data."""
from pathlib import Path
import argparse, hashlib, importlib.util, json, os, re, struct
ap=argparse.ArgumentParser();ap.add_argument('archive',type=Path);ap.add_argument('--source',required=True);a=ap.parse_args()
root=a.archive.resolve();repo=Path(__file__).resolve().parent.parent
def text(p):
    b=p.read_bytes();return b.decode('utf-16' if b.startswith((b'\xff\xfe',b'\xfe\xff')) else 'utf-8-sig')
def read(p):return json.loads(text(p))
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
state=read(root/'session.json');runtime=read(root/'runtime-result.json')
assert state['source']==runtime['source']==a.source
assert state['phase']=='archived_restored','cleanup did not complete'
trace=root/Path(state['server_log']).with_suffix('.bots.log').name
log=text(trace)
created={int(n):int(actor) for n,actor in re.findall(r'#(\d+) created EntityId=(\d+)',log)}
assert len(created)==8
first={};pickups=[]
for line in log.splitlines():
    m=re.search(r'^([\d.]+).*REAL PICKUP actor=(\d+) item=(\d+) acquired=1 class=(\w+)',line)
    if m and m[4]!='SOCOM':
        row={'t':float(m[1]),'actor':int(m[2]),'item':int(m[3]),'class':m[4]};pickups.append(row);first.setdefault(row['actor'],row)
assert set(first)==set(created.values()),'not all eight actors made real primary pickups'
assert 'TEST GRANT SCAR' not in log
goals=[];replacements=[];active={};repeated_pursuit=[];pursuing={}
for line in log.splitlines():
    dead=re.search(r'#(\d+) AI state=(DEAD|RESPAWNING)',line)
    if dead:active.pop(int(dead[1]),None);pursuing.pop(int(dead[1]),None)
    m=re.search(r'^([\d.]+).*#(\d+) WEAPON GOAL previous=(\d+) target=(\d+) event=(acquire|clear)',line)
    if m:
        t,n,old,new=float(m[1]),int(m[2]),int(m[3]),int(m[4]);goals.append({'t':t,'bot':n,'previous':old,'target':new,'event':m[5]})
        if old and new and old!=new:replacements.append(goals[-1])
        if new:active[n]=new
        else:active.pop(n,None)
        pursuing.pop(n,None)
    m=re.search(r'^([\d.]+).*#(\d+) WEAPON GOAL target=(\d+) event=pursue since=([\d.]+)',line)
    if m:
        n,target=int(m[2]),int(m[3]);row={'t':float(m[1]),'bot':n,'target':target,'event':'pursue','since':float(m[4])};goals.append(row)
        if pursuing.get(n)==target:repeated_pursuit.append(row)
        pursuing[n]=target
assert goals and not replacements and not repeated_pursuit,'goal replacement or timer restart observed'
restore={}
for kind,folder in [('memory','Memory'),('nav','Navmesh')]:
    before=read(root/f'{kind}-before/manifest.json');receipt=read(root/f'{kind}-restored.json')
    expected={r['name']:r['sha256'].lower() for r in before['files']}
    assert receipt['exact_names_and_hashes']
    assert expected=={r['name']:r['sha256'].lower() for r in receipt['files']}
    assert all(digest(root/f'{kind}-before'/name)==h for name,h in expected.items())
    live=Path(os.environ['LOCALAPPDATA'])/'WarsBots'/folder
    actual={p.name:digest(p) for p in live.iterdir() if p.is_file()} if live.exists() else {}
    assert actual==expected,'live files no longer match snapshot; do not overwrite later owner changes'
    assert live.exists()==bool(before['directory_existed'])
    restore[kind]={'snapshot_and_receipt':'PASS','current_names_and_hashes':'PASS','files':len(expected)}
spec=importlib.util.spec_from_file_location('c153_demo',repo/'build/ds076-triage/Tools/SteelmillTriage/wblab/demo.py');demo=importlib.util.module_from_spec(spec);spec.loader.exec_module(demo)
demos=[]
for record in read(root/'demo-archive.json'):
    p=root/record['name'];assert digest(p)==record['sha256'].lower();b=p.read_bytes();assert b[:4]==b'WBDM'
    version,hlen=struct.unpack_from('<HI',b,4);o=10+hlen
    while o<len(b):
        assert o+3<=len(b),'demo record header truncated'
        _,length=struct.unpack_from('<BH',b,o);o+=3+length;assert o<=len(b),'demo payload truncated'
    d=demo.read(p);build=str(d['header']['build']);assert version==2 and d['frames'] and 8<=len(build)<=40 and a.source.startswith(build)
    ids={actor['id'] for actor in d['actors'].values() if actor['kind']=='bot'};assert ids==set(created.values())
    primary={'SCAR','FY71','SMG','Shotgun','DSG1','GaussRifle','Hurricane'}
    held={row['id'] for row in d['inv'] if any(cls in primary and clip+reserve>0 for cls,clip,reserve in row['items'])}
    assert held==ids,'demo inventory does not independently show armed primary for every bot'
    demos.append({'name':p.name,'sha256':digest(p),'frames':len(d['frames']),'tail_complete':True,'header':d['header'],'bots':len(ids),'event_counts':{kind:sum(e['kind']==kind for e in d['events']) for kind in ('pickup','kill','revive','shot','hit')}})
assert demos
assert runtime['runtime_criteria_pass']
summary={'schema':'warsbots-run-summary/1','task':'C153','source':a.source,'verdict':'PASS','client_visual':'NOT TESTED','archive':str(root),'created':created,'first_real_primary':list(first.values()),'real_primary_pickups':len(pickups),'weapon_goal_events':len(goals),'mid_goal_replacements':replacements,'repeated_pursuit_anchors':repeated_pursuit,'timeout_events':len(re.findall('unreachable; skipping it',log)),'unavailable_cancels':len(re.findall('reason=unavailable',log)),'runtime':runtime,'restore':restore,'demos':demos,'trace_sha256':digest(trace),'limits':'Headless shared batch only. Timeout/stolen/urgent controls have offline evidence; absent runtime events are not claimed exercised. No cross-build performance or full-Shore/client claim.'}
(root/'summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
(root/'weapon-goals.json').write_text(json.dumps(goals,indent=2)+'\n',encoding='utf-8')
print(json.dumps({k:v for k,v in summary.items() if k in ('source','verdict','real_primary_pickups','weapon_goal_events','timeout_events','unavailable_cancels','client_visual')},ensure_ascii=False))
