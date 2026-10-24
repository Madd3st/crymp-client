import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from wblab import jumps  # noqa: E402


def s(t, x, y, z, j, l, e=200, peak=None):
    return dict(t=t, x=x, y=y, z=z, jump=j, land=l, peak=peak if peak is not None else z, energy=e, mode=3, health=100)


class JumpTests(unittest.TestCase):
    def test_strength_kept_teleport_rejected(self):
        ss = [s(0, 0, 0, 0, 0, 0), s(0.25, 0, 0, 0, 1, 0, 130), s(0.5, 1, 0, 2, 1, 0, 130, 4.0), s(0.75, 2, 0, 3, 1, 1, 130, 4.0),
              s(1.0, 2, 0, 3, 1, 1), s(1.25, 2, 0, 3, 2, 1), s(1.5, 60, 0, 3, 2, 2)]   # second "jump" = 58 m in 0.25 s
        js = jumps.extract(ss)
        self.assertEqual(len(js), 2)
        self.assertTrue(js[0]['keep']); self.assertEqual(js[0]['kind'], 'strength')
        self.assertFalse(js[1]['keep']); self.assertTrue(any('teleport' in r for r in js[1]['reasons']))


if __name__ == '__main__':
    unittest.main()
