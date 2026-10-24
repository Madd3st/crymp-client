"""Streaming validation of an offline wb_geom_export bundle. No engine/ML dependencies."""
import argparse
import json
import math
from pathlib import Path


def validate(path):
    meta = json.loads(path.with_suffix('.geometry.json').read_text(encoding='utf-8'))
    assert meta['schema'] == 'wbgeometry/1' and meta['up_axis'] == 'Z'
    vertices = faces = 0
    pending = []
    with path.open(encoding='ascii') as stream:
        for line in stream:
            if line.startswith('#'):
                continue
            fields = line.split()
            if fields[0] == 'v':
                assert len(fields) == 4
                point = tuple(map(float, fields[1:]))
                assert all(math.isfinite(v) for v in point)
                pending.append(point)
                vertices += 1
            elif fields[0] == 'f':
                assert len(pending) == 3 and len(fields) == 4
                assert list(map(int, fields[1:])) == [vertices-2, vertices-1, vertices]
                a, b, c = pending
                u = [b[i]-a[i] for i in range(3)]
                v = [c[i]-a[i] for i in range(3)]
                cross = (u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
                assert sum(x*x for x in cross) > 0
                pending.clear()
                faces += 1
            else:
                raise ValueError('Unexpected OBJ record')
    assert not pending and vertices == meta['vertices'] and faces == meta['triangles']
    assert faces > 0 and meta['terrain_cells'] > 0
    for suffix, schema, key in [('.spawns.json','wbspawns/1','spawns'),('.items.json','wbitems/1','items')]:
        data = json.loads(path.with_suffix(suffix).read_text(encoding='utf-8'))
        assert data['schema'] == schema and data['map'] == meta['map']
        for item in data[key]:
            assert len(item['position']) == 3 and all(math.isfinite(v) for v in item['position'])
    return dict(path=str(path),triangles=faces,vertices=vertices,bytes=path.stat().st_size,
                terrain_cells=meta['terrain_cells'],unsupported_parts=meta['unsupported_parts'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('obj',type=Path)
    print(json.dumps(validate(parser.parse_args().obj),indent=2))
