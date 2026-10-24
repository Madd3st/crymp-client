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
    # warsbots/botnav-graph/1: 3 ground nodes on a line (columns 0..2) plus an isolated roof
    # node above column 0 (layer 0 = topmost, so the roof is layer 0 and the ground layer 1).
    nodes = [[0, 101.5, 201.5, 8.0, 0, 0, 0], [1, 101.5, 201.5, 0.0, 0, 0, 1],
             [2, 104.5, 201.5, 0.0, 1, 0, 0], [3, 107.5, 201.5, 0.0, 2, 0, 0]]
    edges = [[1, 2, 3.0], [2, 1, 3.0], [2, 3, 3.0], [3, 2, 3.0]]
    d = dict(schema=navgraph.SCHEMA, fixture=True, level="Test", source="test",
             build=dict(nx=3, ny=1, cell=3.0, min_x=100.0, min_y=200.0, min_z=-1.0, max_z=10.0, max_layers=5),
             counts=dict(nodes=4, edges=4, truncated_edge_lists=0),
             node_columns=list(navgraph.NODE_COLUMNS), nodes=nodes,
             edge_columns=list(navgraph.EDGE_COLUMNS), edges=edges,
             memory_compat=dict(nx=3, ny=1, cell=3.0, min_x=100.0, min_y=200.0, count=4))
    d["fingerprint"] = navgraph.fingerprint(d)
    return d


def broken(mutate):
    d = tiny_graph(); mutate(d); return navgraph.check(d)[0]


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
        d = tiny_graph(); r = navgraph.validate(d)
        self.assertTrue(r["ok"]); self.assertTrue(r["fixture"])
        g = navgraph.NavGraph(d)
        self.assertEqual(g.find_path((101, 201, 0), (107, 201, 0))[0], "ok")
        self.assertEqual(g.find_path((101, 201, 0), (107, 201, 0))[1], [1, 2, 3])
        self.assertEqual(g.find_path((101, 201, 0), (101, 201, 8))[0], "unreachable")
        self.assertEqual(g.find_path((101, 201, 30), (107, 201, 0))[0], "start_unprojected")
        self.assertEqual(g.find_path((101, 201, 0), (107, 201, 0), budget=1)[0], "budget")
        self.assertEqual(g.find_path((101, 201, 0), (101.4, 201, 0))[0], "same_node")
        self.assertEqual(len(g.components(undirected=True)), 2)
        self.assertEqual(g.one_way_edges(), 0)
        self.assertEqual(g.summary()["isolated_nodes"], 1)

    def test_nearest_truncates_like_cpp(self):
        g = navgraph.NavGraph(tiny_graph())
        # x = 99.0 is 1 m west of min_x: C++ int(-0.33) = 0 -> column 0, floor() would give -1.
        self.assertEqual(g.col(99.0, 201.0), (0, 0))

    def test_rejects_contract_violations(self):
        self.assertTrue(broken(lambda d: d.update(schema="wbnav/9")))
        self.assertTrue(broken(lambda d: d["nodes"][2].__setitem__(0, 7)))            # id != index
        self.assertTrue(broken(lambda d: d["nodes"][3].__setitem__(6, 5)))            # layer >= max
        self.assertTrue(broken(lambda d: d["nodes"][3].__setitem__(4, 1)))            # duplicate slot
        self.assertTrue(broken(lambda d: d["edges"].append([1, 3, 6.0])))             # Chebyshev 2
        self.assertTrue(broken(lambda d: d["edges"].insert(0, [3, 2, 3.0])))          # not m_edges order
        self.assertTrue(broken(lambda d: d["edges"].__setitem__(0, [1, 1, 1.0])))     # self loop
        self.assertTrue(broken(lambda d: d["counts"].update(edges=9)))
        self.assertTrue(broken(lambda d: d.update(fingerprint="0" * 64)))
        self.assertTrue(broken(lambda d: d["memory_compat"].update(count=5)))

    def test_warnings_are_not_errors(self):
        d = tiny_graph(); d["edges"][0][2] = 3.5; d["fingerprint"] = navgraph.fingerprint(d)
        err, warn = navgraph.check(d)
        self.assertEqual(err, []); self.assertTrue(any("3D distance" in w for w in warn))

    def test_memory_link(self):
        with self.assertRaises(navgraph.GraphError): navgraph.validate(tiny_graph(), make_mem(4))
        ok = mem.MapMemory(3, 1, 3.0, 100.0, 200.0, [0.0] * 4, [0.0] * 4, [0.0] * 4)
        self.assertTrue(navgraph.validate(tiny_graph(), ok)["ok"])

    def test_stuck_penalty_changes_route(self):
        # Diamond: 0 -> {1, 2} -> 3; penalising node 1 must route via 2.
        d = tiny_graph()
        d["build"].update(nx=3, ny=3)
        d["nodes"] = [[0, 101.5, 201.5, 0.0, 0, 0, 0], [1, 104.5, 201.5, 0.0, 1, 0, 0],
                      [2, 104.5, 204.5, 0.0, 1, 1, 0], [3, 107.5, 204.5, 0.0, 2, 1, 0]]
        s2 = 18 ** 0.5
        d["edges"] = [[0, 1, 3.0], [0, 2, s2], [1, 3, s2], [2, 3, 3.0]]
        g = navgraph.NavGraph(d)
        self.assertEqual(g.find_path((101.5, 201.5, 0), (107.5, 204.5, 0), stuck=[0, 6, 0, 0])[1], [0, 2, 3])

    def test_lab_fixture_validates_with_same_fingerprint(self):
        f = Path(__file__).parent / "fixtures" / "fixture_graph.json"
        d = navgraph.read_json(f)
        self.assertEqual(navgraph.check(d)[0], [])
        self.assertEqual(navgraph.fingerprint(d), d["fingerprint"])
        s = navgraph.NavGraph(d).summary()
        self.assertEqual((s["nodes"], s["edges"]), (51, 276))

    def test_wbnav1_converts_losslessly(self):
        d = tiny_graph()
        legacy = dict(schema="wbnav/1", level="Test", build=dict(describe="x"),
                      grid=dict(nx=3, ny=1, cell=3.0, min_x=100.0, min_y=200.0, min_z=-1.0, max_z=10.0),
                      params=dict(layers=5), counts=dict(nodes=4, edges=4),
                      nodes=[[n[1], n[2], n[3], n[5]] for n in d["nodes"]],
                      edges=dict(first=[0, 0, 1, 3], count=[0, 1, 2, 1], to=[2, 1, 3, 2], cost=[3.0] * 4))
        raw = json.loads(json.dumps(legacy))
        self.assertEqual(navgraph.check(raw)[0], [])          # raw wbnav/1 dict is upgraded in place
        self.assertEqual(raw["schema"], navgraph.SCHEMA)
        up = navgraph.from_wbnav1(legacy)
        self.assertEqual(up["nodes"], d["nodes"]); self.assertEqual(up["edges"], d["edges"])
        self.assertEqual(navgraph.check(up)[0], [])
        self.assertEqual(up["fingerprint"], d["fingerprint"])

    def test_scc_profile_finds_sink_and_classifies_routes(self):
        d = tiny_graph(); d["edges"] = [[1, 2, 3.0], [2, 1, 3.0], [2, 3, 3.0]]  # node 3: enter only
        d["counts"]["edges"] = 3; d["fingerprint"] = navgraph.fingerprint(d)
        d["spawns"] = [[195, 107.5, 201.5, 0.0]]
        g = navgraph.NavGraph(d); p = g.scc_profile()
        self.assertEqual(p["sink_sccs"], 1); self.assertEqual(p["spawns"], {"sink": 1})
        r = g.classify_native_routes(["1.0 [WarsBots] #1 NAV fallback start=(0,0,0) goal=(1,1,1) retry=2.0. actor=1 reason=no_route from=3 to=1 expanded=1",
                                      "2.0 [WarsBots] #1 NAV fallback start=(0,0,0) goal=(1,1,1) retry=2.0. actor=1 reason=goal_unprojected from=3 to=-1 expanded=0"])
        self.assertEqual(r["native_reasons"], {"no_route": 1, "goal_unprojected": 1})
        self.assertEqual(r["no_route_start"], {"sink": 1})

    def test_gzip_and_size_cap(self):
        import gzip
        with tempfile.TemporaryDirectory() as t:
            p = Path(t) / "g.graph.json.gz"
            p.write_bytes(gzip.compress(json.dumps(tiny_graph()).encode()))
            self.assertEqual(len(navgraph.load(p).nodes), 4)


class SummaryTests(unittest.TestCase):
    def test_frozen_revive_and_watchdog(self):
        from wblab import summary
        L = ["10.00 [WarsBots] #1 revive requested EntityId=7 spawnId=195 previousSpawn=0 position=(1247.500,1319.750,136.450).",
             "10.00 [WarsBots] #2 revive requested EntityId=8 spawnId=218 previousSpawn=0 position=(1247.500,1338.125,136.450)."]
        for i in range(8):
            t = 10.5 + 2 * i
            L.append(f"{t:.2f} [WarsBots] AI telemetry EntityId=7 state=ATTACKING pos=(1247.50, 1319.75, 136.51) health=100 weapon=1 ammo=5 target=0.")
            L.append(f"{t:.2f} [WarsBots] AI telemetry EntityId=8 state=MOVING pos=({1247.5 + i:.2f}, 1338.13, 136.05) health=100 weapon=1 ammo=5 target=0.")
        self.assertEqual(summary.revives(L), {195: dict(revives=1, frozen=1), 218: dict(revives=1, frozen=0)})
        W = ["18.1 [WarsBots] TRAP RECOVERY level=1 actor=7 age=8.01 pos=(1247.50,1319.75,136.51) blocked=0 speed=0.00 grounded=0.",
             "26.1 [WarsBots] TRAP RECOVERY level=2 actor=7 age=16.01 pos=(1247.50,1319.75,136.51) blocked=0 speed=0.00 grounded=0.",
             "55.1 [WarsBots] TRAP RECOVERY level=4 actor=7 age=45.01 pos=(1247.50,1319.75,136.51) blocked=0 speed=0.00 grounded=0.",
             "55.1 [WarsBots] TRAP RECOVERY level=4 actor=7 nativeSelfKill=1 health=0.",
             "90.0 [WarsBots] TRAP RECOVERY level=1 actor=9 age=8.00 pos=(1300.00,1300.00,140.00) blocked=1 speed=0.00 grounded=1."]
        w = summary.watchdog(W)
        self.assertEqual((w["episodes"], w["at_spawn195"], w["self_kills"]), (2, 1, 1))
        self.assertEqual(w["reached_level"], {"1": 2, "2": 1, "4": 1})
        P = L[:2] + ["20.0 [WarsBots] REAL PICKUP actor=8 item=5 acquired=1",
                     "30.0 [WarsBots] #1 revive requested EntityId=8 spawnId=195 previousSpawn=0 position=(1,1,1)."]
        pk = summary.pickups(P, {195: "sink", 218: "main1"})
        self.assertEqual((pk["lives"], pk["lives_with_pickup"]), (3, 1))
        self.assertEqual(pk["by_spawn_kind"]["main"], dict(lives=1, with_pickup=1, percent=100.0))


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


class PaceTests(unittest.TestCase):
    def test_pace_profile_and_accept(self):
        from wblab import pace
        mk = lambda a, k, sp, sprint, mode: (f"1.0 [WarsBots] MOVE TRACE actor={a} kind={k} side=server t=1 speed={sp} "
                                             f"mode={mode} energy=150.0 sprint={sprint} takeoffs=0 health=100")
        lines = [mk(1, 'bot', 2.5, 0, 3)] * 8 + [mk(1, 'bot', 0.0, 0, 3)] * 2 + [mk(2, 'human', 6.0, 1, 3)] * 4
        lines.append(mk(3, 'bot', 9.9, 1, 3).replace('side=server', 'side=client'))
        p = pace.profile(pace.parse(lines))
        assert p['bot']['actors'] == 1 and p['bot']['moving_pct'] == 80.0 and p['bot']['moving_median'] == 2.5
        assert p['human']['over4_pct'] == 100.0 and p['human']['sprint_pct'] == 100.0
        ok, _ = pace.accept(p['bot']); assert not ok


if __name__ == "__main__":
    unittest.main()
