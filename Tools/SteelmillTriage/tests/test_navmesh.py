import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from wblab import navmesh  # noqa: E402


class Mesh:
    def __init__(self):
        self.v, self.f = [], []

    def quad(self, a, b, c, d):
        i = len(self.v); self.v += [a, b, c, d]; self.f += [(i, i + 1, i + 2), (i, i + 2, i + 3)]

    def floor(self, x0, y0, x1, y1, z):           # upward-facing
        self.quad((x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z))

    def box(self, x0, y0, x1, y1, z0, z1):        # closed solid
        self.floor(x0, y0, x1, y1, z1)
        self.quad((x0, y0, z0), (x0, y1, z0), (x1, y1, z0), (x1, y0, z0))
        for (ax, ay), (bx, by) in (((x0, y0), (x1, y0)), ((x1, y0), (x1, y1)), ((x1, y1), (x0, y1)), ((x0, y1), (x0, y0))):
            self.quad((ax, ay, z0), (bx, by, z0), (bx, by, z1), (ax, ay, z1))

    def obj(self):
        fd, p = tempfile.mkstemp(suffix='.obj'); os.close(fd)
        with open(p, 'w') as f:
            f.write('# map=test up=z\n')
            for x, y, z in self.v: f.write(f'v {x} {y} {z}\n')
            for a, b, c in self.f: f.write(f'f {a + 1} {b + 1} {c + 1}\n')
        return p


def world():
    m = Mesh()
    # main floor 30x20 with a 4x4 hole (pit) at x 20..24, y 8..12
    m.floor(0, 0, 20, 20, 0); m.floor(24, 0, 30, 20, 0); m.floor(20, 0, 24, 8, 0); m.floor(20, 12, 24, 20, 0)
    m.floor(20, 8, 24, 12, -2.0)                                  # pit bottom: drop in, no way out
    for (ax, ay), (bx, by) in (((20, 8), (24, 8)), ((24, 8), (24, 12)), ((24, 12), (20, 12)), ((20, 12), (20, 8))):
        m.quad((ax, ay, -2.0), (bx, by, -2.0), (bx, by, 0), (ax, ay, 0))
    m.box(4, 4, 8, 8, 0, 1.0)                                      # 1 m crate: drop down only
    # ramp up to a 2 m deck (x 10..14 rises, deck x 14..18) – two-way
    m.quad((10, 14, 0), (14, 14, 2.0), (14, 18, 2.0), (10, 18, 0))
    m.box(14, 14, 18, 18, 0, 2.0)
    m.box(0, 19.5, 30, 20, 0, 3.0)                                 # wall along the far edge
    return m


class NavmeshTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        v, f = navmesh.load_obj(world().obj())
        cls.m = navmesh.WalkModel(v, f, bounds=((-1, -1), (31, 21)), cs=0.25)

    def test_regions(self):
        spawns = [(1, 2, 2, 0), (2, 22, 10, -2.0), (3, 6, 6, 1.0), (4, 16, 16, 2.0), (5, 26, 2, 0)]
        p = navmesh.profile(self.m, spawns=spawns)
        sp = p['spawns']
        self.assertEqual(sp['on_mesh'], 5, p)
        self.assertEqual(sp['main'], 3, p)            # floor, deck via ramp, far floor
        self.assertEqual(sp['from_main_only'], 1, p)  # pit: reachable, no way back
        self.assertEqual(sp['reach_main_only'], 1, p) # crate top: can only drop off
        self.assertGreaterEqual(p['sinks'], 1)

    def test_route_reproducibility(self):
        ok = [(i, 2 + i, 2, 0) for i in range(10)]
        climb_crate = [(0, 3.5, 6, 0), (1, 6, 6, 1.0)]       # owner strength-jumps onto the crate
        p = navmesh.profile(self.m, route=ok + climb_crate)
        self.assertEqual(p['route']['on_mesh_pct'], 100.0)
        self.assertLess(p['route']['reproducible_pct'], 100.0)

    def test_jumps(self):
        v, f = navmesh.load_obj(world().obj())
        m = navmesh.WalkModel(v, f, bounds=((-1, -1), (31, 21)), cs=0.25)
        spawns = [(1, 2, 2, 0), (5, 26, 2, 0), (2, 22, 10, -2.0), (3, 6, 6, 1.0)]
        m.add_jumps('jump', v0=4.43, vh=4.5)                   # 1 m apex: neither the 1 m crate nor the 2 m pit
        self.assertEqual(navmesh.profile(m, spawns=spawns)['spawns']['main'], 2)
        n = m.add_jumps('strength', v0=9.60, vh=4.5)           # 4.7 m apex: both
        self.assertGreater(n, 0)
        self.assertEqual(navmesh.profile(m, spawns=spawns)['spawns']['main'], 4)

    def test_strength_jump_clears_3m_wall_not_6m(self):
        for wall, expect in ((3.0, 2), (6.0, 1)):
            m = Mesh(); m.floor(0, 0, 10, 10, 0); m.floor(10.5, 0, 20, 10, 0); m.box(10, 0, 10.5, 10, 0, wall)
            v, f = navmesh.load_obj(m.obj())
            w = navmesh.WalkModel(v, f, bounds=((-1, -1), (21, 11)), cs=0.25)
            w.add_jumps('strength', v0=9.60, vh=4.5)
            self.assertEqual(navmesh.profile(w, spawns=[(1, 5, 5, 0), (2, 15, 5, 0)])['spawns']['main'], expect, wall)

    def test_wall_column_not_walkable_under_clearance(self):
        self.assertEqual(self.m.project(15, 19.8, 0), -1)    # inside the 3 m wall


if __name__ == '__main__':
    unittest.main()
