import sys, matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.collections import PolyCollection, LineCollection
import json
obj, js, links, out = sys.argv[1:5]
box = [float(x) for x in sys.argv[5].split(',')] if len(sys.argv) > 5 else None
V=[]; polys={'main':[], 'other':[]}; hz={'main':[], 'other':[]}; g='main'
for line in open(obj):
    if line[0]=='v': V.append(tuple(map(float,line.split()[1:])))
    elif line[0]=='g': g=line.split()[1]
    elif line[0]=='f':
        idx=[int(t)-1 for t in line.split()[1:]]
        pts=[(V[i][0],V[i][1]) for i in idx]
        if box and not any(box[0]<=x<=box[2] and box[1]<=y<=box[3] for x,y in pts): continue
        polys[g].append(pts); hz[g].append(sum(V[i][2] for i in idx)/len(idx))
fig,ax=plt.subplots(figsize=(14,11),dpi=110); ax.set_facecolor('#111')
pc=PolyCollection(polys['other'],array=hz['other'],cmap='Greys_r',edgecolors='none',alpha=0.9); ax.add_collection(pc)
pm=PolyCollection(polys['main'],array=hz['main'],cmap='summer',edgecolors='none'); ax.add_collection(pm)
fig.colorbar(pm,ax=ax,shrink=0.6,label='z main (m)')
cols={'drop':'#ff9f1c','jump':'#2ec4ff','strength':'#ff3df5'}
L=[l.split() for l in open(links)]
for k,c in cols.items():
    seg=[((float(a[0]),float(a[1])),(float(a[3]),float(a[4]))) for a in L if a[7]==k]
    if box: seg=[s for s in seg if box[0]<=s[0][0]<=box[2] and box[1]<=s[0][1]<=box[3]]
    ax.add_collection(LineCollection(seg,colors=c,linewidths=0.7,label=f'{k} ({len(seg)})'))
d=json.load(open(js))
for p in d['points']:
    x,y=p['pos'][:2]
    if box and not (box[0]<=x<=box[2] and box[1]<=y<=box[3]): continue
    if p['kind']=='spawn': ax.plot(x,y,'o',mfc='none',mec='white' if p['main'] else 'red',ms=9,mew=1.6)
    else: ax.plot(x,y,'^',ms=4,color='#7fdfff' if p['two_way'] else ('#ffd166' if p['reach'] else 'red'))
ax.set_aspect('equal'); ax.autoscale()
if box: ax.set_xlim(box[0],box[2]); ax.set_ylim(box[1],box[3])
ax.legend(loc='lower right',facecolor='#222',labelcolor='w')
s=d['summary']; ax.set_title(f"{d['input']['obj'].split('/')[-1]}  Recast v1.6.0  spawns main {s['spawns_in_main']}/{s['spawns']}  items two-way {s['items_two_way']}/{s['items']}  (red = outside)")
plt.savefig(out,bbox_inches='tight'); print(out)
