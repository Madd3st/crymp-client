"""Tests for detectors.py. Run: python -m unittest Tools/PlaytestReport/test_detectors.py"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(__file__))
import detectors as D  # noqa: E402


def log(*lines):
    return list(lines)


def move(t, actor=100, vx=2.0, vy=0.0, speed=2.0):
    return (f"{t:.2f} [WarsBots] MOVE TRACE actor={actor} kind=bot side=server local=0 t={t:.2f} "
            f"sampleDt=0.100 pos=(0,0,0) vel=({vx},{vy},0) speed={speed:.2f} vz=0.00 mode=3 energy=200.0")


def created(actor=100, n=1, skill="normal"):
    return (f"0.00 [WarsBots] #{n} created EntityId={actor} name=WarsBot_{n:02d} [Normal/Universal] "
            f"profile=universal/{skill} position=(0,0,0).")


def state(t, s, n=1):
    return f"{t:.2f} [WarsBots] #{n} AI state={s}."


def target(t, old, new, visible=1, actor=100):
    return (f"{t:.2f} [WarsBots] TARGET CHOICE actor={actor} old={old} target={new} skill=normal "
            f"style=universal score=1.0 visible={visible} health=100 ammo=30.")


def shot(t, target_id=5, actor=100):
    return (f"{t:.2f} [WarsBots] FIRE SHOT shooter={actor} target={target_id} projectile=1 weapon=1 "
            f"class=SCAR mode=Rapid rpm=700.00 clip=30 remote=0.")


def hit(t, victim=100, shooter=5, dist=10.0):
    return (f"{t:.2f} [WarsBots] FIRE HIT phase=before-Lua target={victim} shooter={shooter} projectile=1 "
            f"weapon=1 class=SCAR actor=1 human=0 health=100 distance={dist:.2f} material=4 head=0 part=23 damage=25.000.")


class DetectorTests(unittest.TestCase):
    def test_parse_maps_actor_and_skill(self):
        s = D.parse(log(created(100, 1, "veteran")))
        self.assertEqual(s.bot_skill[100], "veteran")
        self.assertEqual(s.bot_name[100], "WarsBot_01")

    def test_oscillation(self):
        lines = [created(), state(0.0, "CHASING_ENEMY")]
        for i, t in enumerate([0.0, 0.3, 0.6, 0.9, 1.2]):
            lines.append(move(t, vx=2.0 if i % 2 == 0 else -2.0, speed=2.0))
        s = D.parse(log(*lines))
        ev = D.detect_oscillation(s)
        self.assertTrue(ev and ev[0].type == "oscillation")

    def test_freeze(self):
        lines = [created(), state(0.0, "ATTACKING")]
        for t in [0.0, 0.5, 1.0, 1.5, 2.0]:
            lines.append(move(t, vx=0.0, speed=0.1))
        s = D.parse(log(*lines))
        ev = D.detect_freeze(s)
        self.assertTrue(ev and ev[0].type == "freeze")

    def test_wall(self):
        lines = [created(),
                 "0.50 [WarsBots] TRAP RECOVERY level=2 actor=100 age=1.00 pos=(0,0,0) blocked=1 speed=0.10 grounded=1 sink=0.",
                 "2.50 [WarsBots] TRAP RECOVERY level=2 actor=100 age=2.00 pos=(0,0,0) blocked=1 speed=0.10 grounded=1 sink=0."]
        s = D.parse(log(*lines))
        self.assertTrue(D.detect_wall(s))

    def test_flapping(self):
        lines = [created(), state(0.0, "CHASING_ENEMY"),
                 target(0.0, 0, 5), target(0.5, 5, 6), target(1.0, 6, 5)]
        s = D.parse(log(*lines))
        self.assertTrue(D.detect_flapping(s))

    def test_shoot_cover(self):
        lines = [created(), state(0.0, "ATTACKING"),
                 "0.10 [WarsBots] LOOK TRACE actor=100 reason=aim state=ATTACKING active=1 speed=0.0 interest=1 clear=0 rays=2 "
                 "eye=(0,0,0) corner=(0,0,0) aim=(0,0,0)",
                 shot(0.2), shot(0.4), shot(0.6)]
        s = D.parse(log(*lines))
        self.assertTrue(D.detect_shoot_cover(s))

    def test_open_under_fire(self):
        lines = [created(), state(0.0, "CHASING_ENEMY"),
                 move(0.0, vx=0.0, speed=0.1), move(0.5, vx=0.0, speed=0.1), move(1.0, vx=0.0, speed=0.1),
                 hit(0.2, victim=100, shooter=5), hit(0.8, victim=100, shooter=5)]
        s = D.parse(log(*lines))
        self.assertTrue(D.detect_open_under_fire(s))

    def test_ignore_visible(self):
        lines = [created(), state(0.0, "CHASING_ENEMY"), target(1.0, 0, 5, visible=1)]
        s = D.parse(log(*lines))
        # no shot for 1 s after the visible selection
        self.assertTrue(D.detect_ignore_visible(s))

    def test_stall(self):
        lines = [created(), state(0.0, "MOVING_TO_WEAPON"), state(61.0, "SEARCHING_ENEMY")]
        s = D.parse(log(*lines))
        self.assertTrue(D.detect_stall(s))

    def test_marks_join(self):
        lines = [created(), state(0.0, "CHASING_ENEMY")]
        for i, t in enumerate([0.0, 0.3, 0.6, 0.9, 1.2]):
            lines.append(move(t, vx=2.0 if i % 2 == 0 else -2.0, speed=2.0))
        lines.append('1.2 [WarsBots] OWNER MARK t=1.2 side=client pos=(0,0,0) yaw=0 pitch=0 spectating=100 text="twitch"')
        s = D.parse(log(*lines))
        ev = D.detect(s)
        self.assertTrue(ev and ev[0].mark_id == 1)


if __name__ == "__main__":
    unittest.main()
