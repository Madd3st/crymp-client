import gzip
import importlib.util
import tempfile
import unittest
from pathlib import Path
spec=importlib.util.spec_from_file_location('crop',Path(__file__).parents[1]/'Tools/Crop-WarsBotsGeometry.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)

class CropTests(unittest.TestCase):
    def test_crossing_touching_outside_and_renumber(self):
        source=b'# map=test sha=abc\n'
        triangles=[((-2,0,0),(2,0,0),(0,2,0)),((1,1,0),(2,1,0),(1,2,0)),((3,3,0),(4,3,0),(3,4,0))]
        for i,t in enumerate(triangles):
            source+=b''.join(('v %s %s %s\n'%p).encode() for p in t)
            source+=f'f {i*3+1} {i*3+2} {i*3+3}\n'.encode()
        with tempfile.TemporaryDirectory() as d:
            src=Path(d)/'map.obj';dest=Path(d)/'map.crop.obj.gz';src.write_bytes(source)
            result=module.crop(src,dest,(-1,-1,1,1),3)
            self.assertEqual(result['triangles'],2)
            data=gzip.decompress(dest.read_bytes())
            self.assertTrue(data.startswith(b'# map=test sha=abc\n'))
            self.assertIn(b'f 4 5 6\n',data);self.assertNotIn(b'v 3 3 0\n',data)
    def test_bounds_and_invalid_source_not_published(self):
        self.assertEqual(module.marker_bounds({'spawns':[{'position':[1,2,3]}]},{'items':[{'position':[5,6,7]}]},120),(-119,-118,125,126))
        with tempfile.TemporaryDirectory() as d:
            src=Path(d)/'bad.obj';dest=Path(d)/'bad.obj.gz';src.write_bytes(b'v 0 0 0\nf 1 2 3\n')
            with self.assertRaises(ValueError):module.crop(src,dest,(0,0,1,1))
            self.assertFalse(dest.exists());self.assertFalse(Path(str(dest)+'.tmp').exists())

if __name__=='__main__':unittest.main()
