from pathlib import Path
import json,sys,math,bisect,collections
sys.path.insert(0,str(Path('Tools/SteelmillTriage').resolve()))
from wblab import demo
root=Path(sys.argv[1]);result={};caps={'beginner':400,'normal':720,'veteran':800,'elite':900}
for label in ['baseline','candidate']:
 d=demo.read(root/(label+'-r1-n6.wbdemo'));prev={};first={};rates=[];by=collections.defaultdict(list);revives=collections.defaultdict(list)
 for e in d['events']:
  if e['kind']=='revive':revives[e['actor']].append(e['t'])
 for t,rows in d['frames']:
  for row in rows:
   a=row[0];info=d['actors'][a]
   if info['kind']!='bot':continue
   first.setdefault(a,t);old=prev.get(a);prev[a]=(t,row)
   if not old or not (row[7]&1 and old[1][7]&1) or not .01<t-old[0]<.2 or t-first[a]<1:continue
   if any(old[0]-1<=v<=t+.1 for v in revives[a]):continue
   dt=t-old[0];yaw=((row[4]-old[1][4]+18000)%36000-18000)/100/dt
   def direction(r):
    y,p=math.radians(r[4]/100),math.radians(r[5]/100);return (math.sin(y)*math.cos(p),math.cos(y)*math.cos(p),math.sin(p))
   v,w=direction(row),direction(old[1]);angle=math.degrees(math.acos(max(-1,min(1,sum(x*y for x,y in zip(v,w))))))/dt
   skill=info['profile'].split('/')[0].lower();assert skill in caps,skill
   by[skill].append(angle);rates.append(abs(yaw))
 result[label]={'samples':len(rates),'yaw_gt720_percent':round(100*sum(x>720 for x in rates)/len(rates),3),'yaw_p95':round(sorted(rates)[int(.95*(len(rates)-1))],2),'by_skill':{s:{'samples':len(xs),'cap_deg_s':caps[s],'angular_over_cap_5pct_percent':round(100*sum(x>caps[s]*1.05 for x in xs)/len(xs),3),'max_angular_deg_s':round(max(xs),2)} for s,xs in by.items()},'exclusion':'alive consecutive frames, first1s excluded, recorded revive proximity excluded; sampled20Hz, not every native tick'}
(root/'turn-rate-evidence.json').write_text(json.dumps(result,indent=2),encoding='utf-8');print(json.dumps(result))
