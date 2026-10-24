from pathlib import Path
import sys,json,re,hashlib,collections,statistics,shutil
sys.path.insert(0,str(Path('Tools/SteelmillTriage').resolve()))
from wblab import demo
root=Path(sys.argv[1]);runs=json.loads((root/'runs.json').read_text(encoding='utf-8-sig'))
if isinstance(runs,dict):runs=[runs]
results=[]
for r in runs:
 trace=(root/r['log']).read_text(encoding='utf-8',errors='replace')
 p=Path(re.search(r'DEMO START path=(.+?) version=2 hz=20',trace).group(1))
 target=root/(r['tag']+'.wbdemo')
 if not target.exists():shutil.copy2(p,target)
 d=demo.read(target);ts=[t for t,_ in d['frames']];dt=sorted((b-a)*1000 for a,b in zip(ts,ts[1:]) if b>a)
 def pct(q):return round(dt[round((len(dt)-1)*q)],3)
 events=collections.Counter(e['kind'] for e in d['events'])
 z={'tag':r['tag'],'source_sha':r['source_sha'],'seconds':r['observed_seconds'],'wall_seconds':r['wall_seconds'],'demo':str(target.resolve()),'demo_original':str(p),'demo_sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'frames':len(ts),'events':dict(events),'reported_hit_damage':round(sum(e['value'] for e in d['events'] if e['kind']=='hit'),2),'demo_frame_delta_ms':{'min':round(min(dt),3),'p50':pct(.5),'p95':pct(.95),'p99':pct(.99),'max':round(max(dt),3)},'frame_delta_note':'Recorded20Hz game-time FRAME cadence, not every native engine frame','demo_metrics':demo.metrics(d),'memory_sha256':r['memory_sha256'],'roster':r['roster']}
 results.append(z)
(root/'demo-evidence.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
print(json.dumps([{'tag':z['tag'],'frames':z['frames'],'events':z['events'],'frame_ms':z['demo_frame_delta_ms']} for z in results]))
