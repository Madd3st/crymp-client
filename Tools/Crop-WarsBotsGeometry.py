"""C028: crop streamed C019 triangle-soup OBJ by spawn/item XY bounds; no game calls."""
import argparse
import gzip
import hashlib
import json
import math
import os
from pathlib import Path


def marker_bounds(spawns, items, padding):
    points = [p['position'] for p in spawns['spawns'] + items['items']]
    if not points or not math.isfinite(padding) or padding < 0:
        raise ValueError('Markers and nonnegative finite padding required')
    if any(len(p) != 3 or not all(math.isfinite(v) for v in p) for p in points):
        raise ValueError('Invalid marker position')
    return (min(p[0] for p in points)-padding, min(p[1] for p in points)-padding,
            max(p[0] for p in points)+padding, max(p[1] for p in points)+padding)


def intersects(points, bounds):
    return (max(p[0] for p in points) >= bounds[0] and min(p[0] for p in points) <= bounds[2]
            and max(p[1] for p in points) >= bounds[1] and min(p[1] for p in points) <= bounds[3])


def crop(source, dest, bounds, expected_faces=None, expected_sha=None, max_bytes=150*1024*1024, max_z=None):
    if len(bounds) != 4 or not all(map(math.isfinite,bounds)) or bounds[0]>=bounds[2] or bounds[1]>=bounds[3]:
        raise ValueError("Invalid explicit XY bounds")
    if max_z is not None and not math.isfinite(max_z):
        raise ValueError("Invalid maximum Z")
    temp = dest.with_name(dest.name+'.tmp')
    if dest.exists() or temp.exists():
        raise FileExistsError('Existing crop retained; choose another destination')
    vertices = faces = kept = 0
    pending = []
    header = False
    fingerprint = hashlib.sha256()
    try:
        with source.open('rb', buffering=1024*1024) as src, temp.open('wb') as raw:
            with gzip.GzipFile(filename='',mode='wb',fileobj=raw,mtime=0,compresslevel=6) as output:
                for line in src:
                    fingerprint.update(line)
                    if line.startswith(b'#'):
                        output.write(line)
                        continue
                    if not line.strip():
                        continue
                    if not header:
                        output.write(('# crop_xy='+','.join(map(str,bounds))+'; bbox intersects; Z-up\n').encode('ascii'))
                        header = True
                    fields = line.split()
                    if fields[0] == b'v':
                        point = tuple(map(float,fields[1:]))
                        if len(point) != 3 or not all(map(math.isfinite,point)):
                            raise ValueError('Invalid vertex')
                        pending.append((line,point))
                        if len(pending) > 3:
                            raise ValueError('Expected C019 triangle-soup OBJ, not shared vertices')
                        vertices += 1
                    elif fields[0] == b'f':
                        if len(pending) != 3 or list(map(int,fields[1:])) != [vertices-2,vertices-1,vertices]:
                            raise ValueError('Invalid C019 face ordering')
                        faces += 1
                        if intersects([p[1] for p in pending],bounds) and (max_z is None or all(p[1][2] <= max_z for p in pending)):
                            for original,_ in pending:
                                output.write(original)
                            first = kept*3+1
                            output.write(f'f {first} {first+1} {first+2}\n'.encode('ascii'))
                            kept += 1
                        pending.clear()
                    else:
                        raise ValueError('Unsupported OBJ record')
        if pending or not kept or (expected_faces is not None and faces != expected_faces):
            raise ValueError('Incomplete/empty crop or source triangle count mismatch')
        digest = fingerprint.hexdigest()
        if expected_sha and digest != expected_sha.lower():
            raise ValueError('Source SHA256 mismatch')
        if temp.stat().st_size >= max_bytes:
            raise ValueError('Compressed crop exceeds target size')
        os.replace(temp,dest)
        return dict(source=str(source),path=str(dest),source_sha256=digest,bounds_xy=list(bounds),
                    source_triangles=faces,triangles=kept,vertices=kept*3,bytes=dest.stat().st_size,
                    sha256=hashlib.sha256(dest.read_bytes()).hexdigest())
    except BaseException:
        temp.unlink(missing_ok=True)
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--padding',type=float,default=120)
    parser.add_argument('--maps',nargs='+',default=['Steelmill','Quarry','Armada','Outpost'])
    parser.add_argument('--bounds-xy',nargs=4,type=float,metavar=('XMIN','YMIN','XMAX','YMAX'))
    parser.add_argument('--max-z',type=float,help='Keep whole triangles only when all vertices are at or below this Z')
    args = parser.parse_args()
    root = args.directory
    out = root/'crop'
    out.mkdir(exist_ok=True)
    exports = {e['map']:e for e in json.loads((root/'exports.json').read_text(encoding='utf-8-sig'))}
    results = []
    for name in args.maps:
        if name not in exports:
            raise ValueError('Unknown map')
        load = lambda suffix: json.loads((root/(name+suffix)).read_text(encoding='utf-8'))
        bounds = tuple(args.bounds_xy) if args.bounds_xy else marker_bounds(load('.spawns.json'),load('.items.json'),args.padding)
        original = exports[name]
        result = crop(root/(name+'.obj'),out/(name+'.crop.obj.gz'),bounds,
                      original['triangles'],original['obj_sha256'],max_z=args.max_z)
        result.update(map=name,padding=None if args.bounds_xy else args.padding,max_z=args.max_z,bounds_source='explicit' if args.bounds_xy else 'markers+padding',z_policy='all_vertices_at_or_below' if args.max_z is not None else 'unrestricted')
        (out/(name+'.crop.json')).write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
        results.append(result)
        (out/'crops.json').write_text(json.dumps(results,indent=2)+'\n',encoding='utf-8')
        print(json.dumps(result),flush=True)


if __name__ == '__main__':
    main()
