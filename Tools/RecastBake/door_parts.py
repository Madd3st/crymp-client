# Heuristic door finder for a triangle soup: group triangles into parts by shared (1 mm) vertices, keep parts whose
# bbox looks like a door leaf (thin vertical slab ~0.7-2.2 m wide, 1.8-3.2 m tall). Writes filtered OBJ + list.
import sys, gzip, json, numpy as np
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import connected_components
src, out_obj, out_json = sys.argv[1:4]
V=[];F=[]
for l in gzip.open(src,'rt'):
    if l[:2]=='v ': V.append(tuple(map(float,l.split()[1:4])))
    elif l[:2]=='f ':
        i=[int(t.split('/')[0])-1 for t in l.split()[1:]]
        for k in range(1,len(i)-1): F.append((i[0],i[k],i[k+1]))
V=np.array(V); F=np.array(F)
key=np.round(V*1000).astype(np.int64)
_,vid=np.unique(key,axis=0,return_inverse=True); vid=vid.ravel()
Fu=vid[F]; nt=len(F); nv=vid.max()+1
# triangle-vertex bipartite graph -> components of triangles
rows=np.repeat(np.arange(nt),3); cols=Fu.ravel()+nt
g=coo_matrix((np.ones(len(rows)),(rows,cols)),shape=(nt+nv,nt+nv))
nc,lab=connected_components(g,directed=False); tl=lab[:nt]
T=V[F]; lo=np.full((nc,3),1e9); hi=np.full((nc,3),-1e9)
np.minimum.at(lo,tl,T.min(1)); np.maximum.at(hi,tl,T.max(1))
cnt=np.bincount(tl,minlength=nc)
ext=hi-lo; h=ext[:,2]; w=np.maximum(ext[:,0],ext[:,1]); th=np.minimum(ext[:,0],ext[:,1])
door=(h>1.8)&(h<3.2)&(w>0.7)&(w<2.4)&(th<0.45)&(cnt<=400)
print('parts',nc,'door candidates',door.sum())
drop=door[tl]
with open(out_obj,'w') as f:
    f.write('# door-filtered\n')
    for t in T[~drop]:
        for p in t: f.write('v %.3f %.3f %.3f\n'%tuple(p))
    for i in range(int((~drop).sum())): f.write('f %d %d %d\n'%(3*i+1,3*i+2,3*i+3))
json.dump([dict(lo=lo[c].round(2).tolist(),hi=hi[c].round(2).tolist(),tris=int(cnt[c])) for c in np.flatnonzero(door)],open(out_json,'w'))
