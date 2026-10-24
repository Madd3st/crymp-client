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


if __name__ == '__main__':
    unittest.main()
