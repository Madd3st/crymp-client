# Off-mesh links for elevators and ladders from the editor scene (level.editor_xml via movers.json) + the baked OBJ.
import sys, json, numpy as np
movers, objp, mapname, out = sys.argv[1:5]
V=[]; P=[]
for l in open(objp):
    if l[0]=='v': V.append(tuple(map(float,l.split()[1:])))
    elif l[0]=='f': P.append([int(t)-1 for t in l.split()[1:]])
V=np.array(V); C=np.array([V[p].mean(0) for p in P])
def levels(x,y,r,zlo,zhi):
    d=np.hypot(C[:,0]-x,C[:,1]-y); m=(d<r)&(C[:,2]>zlo)&(C[:,2]<zhi)
    pts=C[m]; 
    if not len(pts): return []
    lv=[]
    for p in pts[np.argsort(pts[:,2])]:
        if not lv or p[2]-lv[-1][2]>1.5: lv.append(p)
        elif np.hypot(p[0]-x,p[1]-y)<np.hypot(lv[-1][0]-x,lv[-1][1]-y): lv[-1]=p
    return lv
L=[]
for r in json.load(open(movers))[mapname]:
    x,y,z=r['pos']
    if r['kind']=='Elevator':
        h=float(r['fFloorHeight']); n=int(r['nFloorCount'])
        lv=levels(x,y,4.0,z-1,z+h*(n-1)+3)
        kind='lift'
    elif r['kind']=='Ladder':
        lv=levels(x,y,2.0,z-1.5,z+12); kind='ladder'
    else: continue
    for a,b in zip(lv,lv[1:]):
        L.append((a,b,kind,r['name']))
with open(out,'w') as f:
    for a,b,k,n in L: f.write('%.2f %.2f %.2f %.2f %.2f %.2f 1 %s\n'%(*a,*b,k))
for a,b,k,n in L: print(k,n,a.round(1),'->',b.round(1))
