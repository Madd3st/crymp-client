#!/usr/bin/env python3
"""T-6: write <map>.manifest.json next to the baked runtime data so the game can refuse mismatched files.
Usage: make_manifest.py <dir> <Map> [--mode IA] [--source "C019 crop a7553da7"] [--bake "cs 0.1 tile 256 r 0.3 --jumps"]
FNV-1a 64 is used because the game computes it without a crypto library; sha256 is recorded for humans."""
import hashlib, json, os, sys, argparse
def fnv64(b):
    h = 0xcbf29ce484222325
    for x in b: h = ((h ^ x) * 0x100000001b3) & 0xFFFFFFFFFFFFFFFF
    return '%016x' % h
ap = argparse.ArgumentParser(); ap.add_argument('dir'); ap.add_argument('map')
ap.add_argument('--mode', default='IA'); ap.add_argument('--source', default=''); ap.add_argument('--bake', default='')
a = ap.parse_args()
files = {}
for kind, suffix in (('navmesh', '.navmesh'), ('links', '.links.txt'), ('spots', '.spots.txt')):
    p = os.path.join(a.dir, a.map + suffix)
    if not os.path.exists(p): continue
    b = open(p, 'rb').read()
    files[kind] = {'name': a.map + suffix, 'bytes': len(b), 'fnv64': fnv64(b), 'sha256': hashlib.sha256(b).hexdigest()}
m = {'schema': 1, 'map': a.map, 'mode': a.mode, 'source': a.source, 'bake': a.bake, 'files': files}
out = os.path.join(a.dir, a.map + '.manifest.json')
json.dump(m, open(out, 'w'), indent=1); print(out, {k: v['fnv64'] for k, v in files.items()})
