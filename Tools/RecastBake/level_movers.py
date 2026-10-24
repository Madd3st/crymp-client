import sys, re, zipfile, json
out = {}
for path in sys.argv[1:]:
    name = path.split('/')[-1][:-4]
    s = zipfile.ZipFile(path).read('level.editor_xml').decode('utf-8', 'replace')
    rec = []
    for m in re.finditer(r'<Object [^>]*>', s):
        t = m.group(0)
        g = lambda k: (re.search(' ' + k + r'="([^"]*)"', t) or [None, None])[1]
        cls, geo = g('EntityClass') or '', (g('Prefab') or g('Geometry') or '')
        low = (geo + ' ' + (g('Name') or '')).lower()
        kind = None
        if cls in ('Elevator', 'ElevatorSwitch', 'Door', 'AnimDoor'): kind = cls
        elif 'ladder' in low: kind = 'Ladder'
        elif 'stair' in low: kind = 'Stairs'
        if kind:
            p = [float(v) for v in (g('Pos') or '0,0,0').split(',')]
            r = {'kind': kind, 'name': g('Name'), 'pos': p, 'rotate': g('Rotate'), 'geometry': geo}
            if cls == 'Elevator':
                i = s.find('>', m.end() - 1); j = s.find('</Properties>', i)
                pr = s[i:j]
                for k in ('nFloorCount', 'fFloorHeight', 'nInitialFloor', 'bAutomatic', 'objModel'):
                    mm = re.search(k + r'="([^"]*)"', pr); r[k] = mm.group(1) if mm else None
            rec.append(r)
    out[name] = rec
    from collections import Counter
    print(name, Counter(r['kind'] for r in rec))
json.dump(out, open('movers.json', 'w'), indent=1)
