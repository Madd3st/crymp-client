"""Synthetic tests for Steelmill Triage. Run: python -m unittest discover -s Tools/SteelmillTriage/tests"""
import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from wblab import analysis, logparse, mem, navgraph, render  # noqa: E402


def make_mem(n=6):
    m = mem.MapMemory(2, 2, 3.0, 100.0, 200.0, [0.0] * n, [0.0] * n, [0.0] * n)
    return m


class MemTests(unittest.TestCase):
    def test_roundtrip_matches_botnav_layout(self):
        m = make_mem(); m.stuck[1] = 20; m.kills[3] = 4
        raw = mem.write(m)
        self.assertEqual(len(raw), 32 + 6 * 12)
        self.assertEqual(raw[:8], b"WBMEM01\x00")
        self.assertEqual(struct.unpack_from("<iifffi", raw, 8), (2, 2, 3.0, 100.0, 200.0, 6))
        back = mem.parse(raw)
        self.assertEqual(back.stuck, m.stuck); self.assertEqual(back.kills, m.kills)

    def test_rejects_truncated_and_bad_magic(self):
        raw = mem.write(make_mem())
        with self.assertRaises(mem.MemError): mem.parse(raw[:-4])
        with self.assertRaises(mem.MemError): mem.parse(b"XXXXXXX\x00" + raw[8:])

    def test_hotspot_audit_counts_odd_nodes_as_ignored(self):
        m = make_mem(); m.kills[1] = 10; m.kills[2] = 10  # one odd, one even hotspot
        a = m.summary()["hotspot_audit"]["fight"]
        self.assertEqual(a["eligible_nodes"], 2); self.assertEqual(a["visible_to_PickHotspot"], 1)
        self.assertAlmostEqual(a["weight_ignored_percent"], 50.0)

    def test_diff_requires_same_graph(self):
        a = make_mem(); b = make_mem(); b.stuck[0] = 2
        self.assertEqual(mem.diff(a, b)["new_stuck_nodes"], [0])
        with self.assertRaises(mem.MemError): mem.diff(a, make_mem(7))


LOG = """0.00 [WarsBots] BotManager initialized (build vTEST-1-gabc): x
0.10 [WarsBots] #1 created EntityId=11 name=WarsBot_01 profile=assault/elite position=(10.000, 10.000, 5.000).
0.10 [WarsBots] #2 created EntityId=22 name=WarsBot_02 profile=hunter/hard position=(50.000, 50.000, 5.000).
{tele}
20.00 [WarsBots] #1 NAV fallback start=(10.00,10.00,5.00) goal=(12.00,10.00,9.00) retry=2.0.
20.10 [WarsBots] MOTION INTENT: gait=1 strafe=0 requestedBase=1.00 actual=0.00 lateral=0.00 facingDot=1.00 blocked=1.
20.20 [WarsBots] TACTIC SAMPLE actor=11 skill=elite style=assault weapon=SCAR suit=0 distance=0.00 action=SEARCHING_ENEMY target=0 visible=0.
1.00 [WarsBots] #1 created EntityId=33 name=WarsBot_01 profile=assault/elite position=(0.000, 0.000, 0.000).
"""


def telemetry_lines():
    out = []
    for i in range(10):  # bot 11 never moves, bot 22 walks 2 m per sample
        t = 1 + i * 2
        out.append(f"{t:.2f} [WarsBots] AI telemetry EntityId=11 state=SEARCHING_ENEMY pos=(10.00, 10.00, 5.00) health=100 weapon=1 ammo=5 target=0.")
        out.append(f"{t:.2f} [WarsBots] AI telemetry EntityId=22 state=SEARCHING_ENEMY pos=({50 + 2 * i:.2f}, 50.00, 5.00) health=100 weapon=1 ammo=5 target=0.")
    return "\n".join(out)


class LogTests(unittest.TestCase):
    def setUp(self):
        self.runs = logparse.parse_lines(LOG.replace("{tele}", telemetry_lines()).splitlines())

    def test_backwards_time_starts_new_run(self):
        self.assertEqual(len(self.runs), 2)
        self.assertEqual(self.runs[0].build, "vTEST-1-gabc")

    def test_slot_lines_attributed_to_current_entity_only(self):
        fb = [d for k, t, d in self.runs[0].events if k == "nav_fallback"]
        self.assertEqual(fb[0]["entity"], 11)
        # This MOTION INTENT is not inside a telemetry block, so it must not be attributed.
        self.assertEqual(self.runs[0].unattributed["block line outside telemetry block"], 1)
        self.assertFalse(any(k == "motion" for k, t, d in self.runs[0].events))

    def test_block_lines_join_preceding_telemetry(self):
        text = ("5.00 [WarsBots] #1 created EntityId=7 name=W profile=assault/elite position=(0.000, 0.000, 0.000).\n"
                "6.00 [WarsBots] AI telemetry EntityId=7 state=SEARCHING_ENEMY pos=(1.00, 2.00, 3.00) health=100 weapon=1 ammo=5 target=0.\n"
                "6.00 [WarsBots] TACTIC SAMPLE actor=7 skill=elite style=assault weapon=SCAR suit=0 distance=0.00 action=SEARCHING_ENEMY target=0 visible=0.\n"
                "6.00 [WarsBots] AI decision target=0 visible=0 sample=0.00 age=0.10 (cached decision; muzzle checked separately).\n"
                "6.00 [WarsBots] Motion heading=(0.00,1.00) physicalSpeed=0.00.\n"
                "6.00 [WarsBots] ABILITY PHYSICS: speed=0.00 vz=0.00 grounded=1 sprint=0 mode=3 energy=200.0 active=1 clients=0.\n"
                "6.00 [WarsBots] MOTION INTENT: gait=1 strafe=0 requestedBase=2.50 actual=0.00 lateral=0.00 facingDot=1.00 blocked=0.\n")
        run = logparse.parse_lines(text.splitlines())[0]
        motion = [d for k, t, d in run.events if k == "motion"]
        self.assertEqual(len(motion), 1)
        self.assertEqual(motion[0]["entity"], 7); self.assertFalse(motion[0]["blocked"])
        self.assertEqual(sum(run.unattributed.values()), 0)

    def test_pinned_and_stall_detection(self):
        r = self.runs[0]
        pins = analysis.pinned(r)
        self.assertEqual([p["entity"] for p in pins], [11])
        st = analysis.stalls(r)
        self.assertTrue(any(s["entity"] == 11 and s["seconds"] >= 4 for s in st))
        self.assertFalse(any(s["entity"] == 22 for s in st))

    def test_fallback_floor_classification(self):
        c = analysis.fallback_profile(self.runs[0])["counts"]
        self.assertEqual(c["total"], 1); self.assertEqual(c["dz_over_2.5"], 1); self.assertEqual(c["under_3m"], 1)

    def test_dataset_flags_state_as_action(self):
        q = analysis.dataset_quality(self.runs[0])
        self.assertEqual(q["action_equals_state_percent"], 100.0)
        self.assertTrue(any("FSM state" in i for i in q["issues"]))


def tiny_graph():
    # 4 nodes on a line in separate columns (3 m), plus an isolated roof node 1.
    nodes = [[101.5, 201.5, 0.0, 0], [101.5, 201.5, 8.0, 0], [104.5, 201.5, 0.0, 1], [107.5, 201.5, 0.0, 2]]
    adj = {0: [2], 1: [], 2: [0, 3], 3: [2]}
    first, count, to, cost = [], [], [], []
    for i in range(4):
        first.append(len(to)); count.append(len(adj[i]))
        for j in adj[i]: to.append(j); cost.append(3.0)
    return dict(schema="wbnav/1", level="Test", build=dict(sha="x"),
                grid=dict(nx=3, ny=1, cell=3.0, min_x=100.0, min_y=200.0),
                counts=dict(nodes=4, edges=len(to)), nodes=nodes,
                edges=dict(first=first, count=count, to=to, cost=cost))


class MovementTests(unittest.TestCase):
    def test_states_and_like_for_like(self):
        from wblab import movement
        lines = []
        for i in range(4):
            t = f"{10 + i:.2f}"
            lines += [f"{t} [WarsBots] AI telemetry EntityId=7 state=CHASING_ENEMY pos=(0.00, 0.00, 0.00) health=100 weapon=1 ammo=5 target=9.",
                      f"{t} [WarsBots] ABILITY PHYSICS: speed=4.00 vz=0.00 grounded=1 sprint={i % 2} mode=0 energy=150.0 active=1 clients=1.",
                      f"{t} [WarsBots] MOTION INTENT: gait=2 strafe=0 requestedBase=2.50 actual=4.00 lateral=0.00 facingDot=1.00 blocked=0."]
        lines += ["1.00 [WarsBots] Bot 7 acquired enemy id=5 name=Nomad kind=human.",
                  "2.00 [WarsBots] LOS blocked target=5 obstruction=0 eyes=(0,0,0) chest=(0.00,0.00,1.00) sample=2.00.",
                  "2.50 [WarsBots] LOS blocked target=5 obstruction=0 eyes=(0,0,0) chest=(2.50,0.00,1.00) sample=2.50.",
                  "2.00 [WarsBots] LOS blocked target=7 obstruction=0 eyes=(0,0,0) chest=(0.00,0.00,1.00) sample=2.00.",
                  "3.00 [WarsBots] LOS blocked target=7 obstruction=0 eyes=(0,0,0) chest=(1.00,0.00,1.00) sample=3.00."]
        states, _ = movement.bot_states(lines)
        self.assertEqual(states["CHASING_ENEMY"]["native_sprint_percent"], 50.0)
        self.assertEqual(states["CHASING_ENEMY"]["suit_mode_percent"], {"Speed": 100})
        lfl = movement.los_speeds(lines)
        self.assertEqual(lfl["humans"]["Nomad"]["median"], 5.0)
        self.assertEqual(lfl["bots"]["median"], 1.0)


class GraphTests(unittest.TestCase):
    def test_validate_and_paths(self):
        d = tiny_graph(); navgraph.validate(d)
        g = navgraph.NavGraph(d)
        self.assertEqual(g.find_path((101, 201, 0), (107, 201, 0))[0], "ok")
        self.assertEqual(g.find_path((101, 201, 0), (101, 201, 8))[0], "unreachable")
        self.assertEqual(g.find_path((101, 201, 30), (107, 201, 0))[0], "start_unprojected")
        self.assertEqual(g.find_path((101, 201, 0), (107, 201, 0), budget=1)[0], "budget")
        self.assertEqual(len(g.components(undirected=True)), 2)
        self.assertEqual(g.one_way_edges(), 0)

    def test_validate_rejects_broken_csr_and_foreign_memory(self):
        d = tiny_graph(); d["edges"]["first"][2] += 1
        with self.assertRaises(navgraph.GraphError): navgraph.validate(d)
        with self.assertRaises(navgraph.GraphError): navgraph.validate(tiny_graph(), make_mem(4))
        ok = mem.MapMemory(3, 1, 3.0, 100.0, 200.0, [0.0] * 4, [0.0] * 4, [0.0] * 4)
        self.assertTrue(navgraph.validate(tiny_graph(), ok))


class RenderTests(unittest.TestCase):
    def test_html_is_self_contained(self):
        run = logparse.parse_lines(LOG.replace("{tele}", telemetry_lines()).splitlines())[0]
        s, st, _ = analysis.summarize(run)
        g = navgraph.NavGraph(tiny_graph())
        p = render.build_payload("t", run, s, st, analysis.pinned(run), None, g, None)
        with tempfile.TemporaryDirectory() as d:
            out = render.write_html(Path(d) / "x.html", [p])
            html = out.read_text(encoding="utf-8")
        self.assertIn("<title>", html); self.assertNotIn("<html", html.lower())
        data = html.split("const RUNS = ", 1)[1].split(";\nconst css", 1)[0]
        self.assertEqual(json.loads(data)[0]["label"], "t")


if __name__ == "__main__":
    unittest.main()
