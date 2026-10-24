"""Summarize native C-006 edge explanations, read-only input; no graph repair."""
import argparse,collections,json,re
from pathlib import Path

def main():
 p=argparse.ArgumentParser();p.add_argument('log',type=Path);p.add_argument('--pairs',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
 pairs=json.loads(a.pairs.read_text(encoding='utf-8-sig'));expected={(int(x['frm']),int(x['to'])) for x in pairs};expected|={(b,a) for a,b in tuple(expected)}
 lines=[s for s in a.log.read_text(encoding='utf-8',errors='replace').splitlines() if 'NAV explain from=' in s]
 pattern=re.compile(r'from=(\d+) to=(\d+) candidate=(\d+) storedEdge=(\d+) allowed=(\d+) test=(\S+) sample=([\d.-]+) value=([\d.-]+) hit=\(([^)]+)\) entity=(\d+) name=(.*?) class=(.*?) surface=(-?\d+) part=(-?\d+)')
 records=[]
 for line in lines:
  m=pattern.search(line)
  if not m:raise ValueError('Unparsed diagnostic: '+line)
  v=m.groups();records.append(dict(frm=int(v[0]),to=int(v[1]),candidate=int(v[2]),stored_edge=int(v[3]),allowed=int(v[4]),test=v[5],sample=float(v[6]),value=float(v[7]),hit=[float(x) for x in v[8].split(',')],entity=int(v[9]),name=v[10],entity_class=v[11],surface=int(v[12]),part=int(v[13])))
 seen={(r['frm'],r['to']) for r in records}
 if len(lines)!=2*len(pairs) or seen!=expected:raise ValueError('Incomplete/unexpected directed pair evidence')
 failed=[r for r in records if not r['allowed']]
 criteria=collections.Counter(r['test']+(' h='+str(r['sample']) if r['test']=='clearance' else '') for r in failed)
 surfaces=collections.Counter(str(r['surface']) for r in failed)
 entities=collections.Counter(str(r['entity'])+'|'+r['name']+'|'+r['entity_class'] for r in failed)
 out=dict(pairs=len(pairs),directions=len(records),allowed=len(records)-len(failed),failed_by_criterion=dict(criteria),failed_by_surface=dict(surfaces),failed_by_entity=dict(entities),records=records)
 a.out.mkdir(parents=True,exist_ok=True);(a.out/'raw.log').write_text('\n'.join(lines)+'\n',encoding='utf-8');(a.out/'report.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8');print(json.dumps({k:v for k,v in out.items() if k!='records'}))
if __name__=='__main__':main()
