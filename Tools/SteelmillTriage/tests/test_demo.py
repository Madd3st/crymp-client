import os, sys, tempfile, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from wblab import demo  # noqa: E402


class DemoTests(unittest.TestCase):
    def test_roundtrip_and_metrics(self):
        fd, p = tempfile.mkstemp(suffix='.wbdemo'); os.close(fd)
        w = demo.Writer(p, {'map': 'test', 'hz': 20})
        w.actor(0, 1, 'human', 'Nomad'); w.actor(0, 2, 'bot', 'WarsBot_01', 'universal/normal')
        w.event(0.0, 'revive', 2, value=176)
        for i in range(100):                                   # 5 s at 20 Hz
            t = i * 0.05
            human = dict(id=1, x=0, y=0, z=0, yaw=10.0, pitch=0, weapon='SCAR')
            bot = dict(id=2, x=10, y=0, z=0, yaw=(30.0 if i % 2 else -30.0), pitch=0,  # wanders ±30°
                       weapon='SOCOM' if t < 2 else 'SCAR')
            w.frame(t, [human, bot])
            vis = t >= 3.0
            w.brain(t, 2, 'ATTACKING' if vis else ('SEEKING' if (i // 10) % 2 else 'CHASING'), target=1 if vis else 0,
                    visible=vis, aim=(10, 10, 0), reason='test')
        w.event(2.0, 'pickup', 2, other=99, value=w.sid('SCAR'))
        w.event(3.4, 'shot', 2, other=1)
        w.event(4.0, 'kill', 1, other=2)
        w.close()
        d = demo.read(p)
        self.assertEqual(len(d['frames']), 100)
        self.assertEqual(d['actors'][2]['name'], 'WarsBot_01')
        m = demo.metrics(d)
        self.assertEqual(m['aim_wander_deg_s']['human'], 0.0)
        self.assertAlmostEqual(m['aim_wander_deg_s']['bot'], 1200.0, delta=1)    # 60° per 0.05 s
        self.assertAlmostEqual(m['reaction_s'], 0.4, delta=0.01)
        self.assertAlmostEqual(m['time_to_primary_s'], 2.0, delta=0.01)
        self.assertEqual(m['kills_by_weapon'], {'SCAR': 1})
        self.assertGreater(m['decision_flips_per_bot_min'], 0)


class DemoYawTests(unittest.TestCase):
    def test_yaw_350_and_minus_10_do_not_overflow(self):
        fd, p = tempfile.mkstemp(suffix='.wbdemo'); os.close(fd)
        w = demo.Writer(p, {'map': 't'}); w.actor(0, 1, 'bot', 'b')
        w.frame(0.0, [dict(id=1, x=0, y=0, z=0, yaw=350.0)]); w.frame(0.05, [dict(id=1, x=0, y=0, z=0, yaw=-10.0)])
        w.close()
        f = demo.read(p)['frames']
        self.assertEqual(f[0][1][0][4], -1000); self.assertEqual(f[1][1][0][4], -1000)


class DemoV2Tests(unittest.TestCase):
    def test_intent_inv_and_client_merge(self):
        def mk(path, client=False, off=0.0):
            w = demo.Writer(path, {'map': 't'}); aid = 7 if client else 1
            w.actor(0, aid, 'human', 'Nomad')
            if not client:
                w.actor(0, 2, 'bot', 'b'); w.intent(0.0, 2, move=(0, 1), gait=2, jump=2, reason='ledge')
                w.inv(0.0, 2, [('SCAR', 30, 90), ('SOCOM', 12, 24)])
            for i in range(200):
                t = i * 0.05
                me = dict(id=aid, x=t * 3, y=(t * 2) % 7, z=0, yaw=(90.0 if client else 0.0),
                          flags=(7 if client else 5), peak=(12.5 if client else 0))
                w.frame(t + (off if client else 0), [me] if client else [me, dict(id=2, x=50, y=0, z=0)])
            w.close()
        fd, s = tempfile.mkstemp(suffix='.wbdemo'); os.close(fd)
        fd, c = tempfile.mkstemp(suffix='.wbdemo.client'); os.close(fd)
        mk(s); mk(c, client=True, off=1.5)
        d = demo.read(s); self.assertEqual(d['version'], 2)
        self.assertEqual(d['intents'][0]['jump'], 2); self.assertEqual(d['inv'][0]['items'][0], ('SCAR', 30, 90))
        m, info = demo.merge_client(d, demo.read(c))
        self.assertAlmostEqual(info['offset_s'], -1.5, delta=0.06); self.assertEqual(info['actor'], 1)
        row = [r for r in m['frames'][100][1] if r[0] == 1][0]
        self.assertEqual(row[4], 9000); self.assertTrue(row[7] & demo.F_SPRINT); self.assertEqual(row[13], 1250)


if __name__ == '__main__':
    unittest.main()
