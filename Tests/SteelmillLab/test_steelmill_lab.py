#!/usr/bin/env python3
"""Tests for Tools/SteelmillLab (stdlib unittest; no game, no network).

    python -m unittest discover -s Tests/SteelmillLab -v

Fixtures in Tests/SteelmillLab/fixtures are SYNTHETIC. Real-data tests run only when copies
exist in build/steelmill-lab/sources (or $STEELMILL_LAB_SOURCES) and are skipped otherwise.
"""
import io
import json
import math
import os
import struct
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools" / "SteelmillLab"))
import botlog       # noqa: E402
import level_data   # noqa: E402
import ml_validate  # noqa: E402
import navgraph     # noqa: E402
import wbmem        # noqa: E402

FIX = Path(__file__).resolve().parent / "fixtures"
SOURCES = Path(os.environ.get("STEELMILL_LAB_SOURCES", ROOT / "build" / "steelmill-lab" / "sources"))


def mem_bytes(nx=4, ny=3, cell=3.0, mx=10.0, my=20.0, records=((0, 0, 0),), count=None, magic=b"WBMEM01\0"):
    count = len(records) if count is None else count
    body = b"".join(struct.pack("<fff", *r) for r in records)
    return magic + struct.pack("<iifffi", nx, ny, cell, mx, my, count) + body


class WbMemTests(unittest.TestCase):
    def test_valid_roundtrip(self):
        p = wbmem.parse_bytes(mem_bytes(records=[(1, 2, 3), (0, 0, 0), (20, 5, 1)]))
        self.assertEqual(p["header"]["count"], 3)
        self.assertEqual(p["header"]["max_x"], 22.0)
        s = wbmem.summarize(p)
        self.assertTrue(s["valid"])
        self.assertEqual(s["stats"]["stuck"]["total"], 21)
        self.assertEqual(s["stats"]["stuck"]["at_cap"], 1)
        self.assertEqual(s["top_nodes"]["kills"][0], dict(node=2, value=5.0))

    def test_header_is_32_bytes(self):
        self.assertEqual(wbmem.HEADER.size, 32)
        self.assertEqual(len(mem_bytes(records=[])), 32)

    def test_rejects_bad_magic(self):
        with self.assertRaises(wbmem.MemError):
            wbmem.parse_bytes(mem_bytes(magic=b"WBMEM02\0"))

    def test_rejects_truncation(self):
        data = mem_bytes(records=[(1, 1, 1), (2, 2, 2)])
        for cut in (5, 31, len(data) - 1):
            with self.assertRaises(wbmem.MemError):
                wbmem.parse_bytes(data[:cut])

    def test_rejects_impossible_counts_and_grid(self):
        with self.assertRaises(wbmem.MemError):
            wbmem.parse_bytes(mem_bytes(nx=1, ny=1, records=[(0, 0, 0)] * 6))   # > 1*1*5 layers
        with self.assertRaises(wbmem.MemError):
            wbmem.parse_bytes(mem_bytes(nx=0))
        with self.assertRaises(wbmem.MemError):
            wbmem.parse_bytes(mem_bytes(cell=float("nan")))
        with self.assertRaises(wbmem.MemError):
            wbmem.parse_bytes(mem_bytes(records=[], count=-1))

    def test_flags_bad_values(self):
        p = wbmem.parse_bytes(mem_bytes(records=[(-1, 0, 0), (float("inf"), 0, 0), (25, 0.5, 0)]) + b"x")
        codes = {i["code"] for i in p["issues"]}
        self.assertTrue({"negative", "non_finite", "stuck_above_cap", "fractional", "trailing_bytes"} <= codes)
        self.assertFalse(wbmem.summarize(p)["valid"])

    def test_cli_never_writes_input(self):
        with tempfile.TemporaryDirectory() as d:
            m = Path(d) / "x.mem"
            m.write_bytes(mem_bytes(records=[(1, 0, 0)]))
            before = m.read_bytes(), m.stat().st_mtime_ns
            rc = wbmem.main([str(m), "--json", str(Path(d) / "r.json")])
            self.assertEqual(rc, 0)
            self.assertEqual((m.read_bytes(), m.stat().st_mtime_ns), before)

    def test_graph_link_requires_same_build(self):
        g = navgraph.fixture()
        b = g["build"]
        recs = [(0, 0, 0)] * len(g["nodes"])
        recs[3] = (2, 1, 0)
        p = wbmem.parse_bytes(mem_bytes(nx=b["nx"], ny=b["ny"], cell=b["cell"], mx=b["min_x"], my=b["min_y"], records=recs))
        link = wbmem.link_graph(p, g)
        self.assertTrue(link["linked"], link["problems"])
        self.assertEqual(link["nodes_with_memory"][0]["id"], 3)
        self.assertEqual(link["nodes_with_memory"][0]["pos"], g["nodes"][3][1:4])
        wrong = wbmem.parse_bytes(mem_bytes(nx=b["nx"], ny=b["ny"], cell=b["cell"], mx=b["min_x"] + 1, my=b["min_y"], records=recs))
        self.assertFalse(wbmem.link_graph(wrong, g)["linked"])
        short = wbmem.parse_bytes(mem_bytes(nx=b["nx"], ny=b["ny"], cell=b["cell"], mx=b["min_x"], my=b["min_y"], records=recs[:-1]))
        self.assertFalse(wbmem.link_graph(short, g)["linked"])

    @unittest.skipUnless((SOURCES / "Multiplayer_IA_Steelmill.mem").exists(), "real .mem copy not present")
    def test_real_steelmill_memory(self):
        p = wbmem.parse_bytes((SOURCES / "Multiplayer_IA_Steelmill.mem").read_bytes())
        h = p["header"]
        self.assertEqual((h["nx"], h["ny"], h["cell"]), (96, 63, 3.0))
        self.assertTrue(wbmem.summarize(p)["valid"])


class NavGraphTests(unittest.TestCase):
    def setUp(self):
        self.g = json.loads((FIX / "fixture_graph.json").read_text())

    def test_fixture_is_flagged_and_valid(self):
        self.assertTrue(self.g["fixture"])
        err, warn = navgraph.validate(self.g)
        self.assertEqual(err, [])
        self.assertEqual(self.g, navgraph.fixture(), "fixture file must be regenerated by navgraph.py fixture")

    def test_validator_catches_corruption(self):
        bad = json.loads(json.dumps(self.g))
        bad["nodes"][5][0] = 99
        bad["edges"].append([0, len(bad["nodes"]) - 1, 1.0])
        bad["edges"].append([1, 1, 1.0])
        err, _ = navgraph.validate(bad)
        text = " ".join(err)
        self.assertIn("id 99", text)
        self.assertIn("non-adjacent", text)
        self.assertIn("self loop", text)
        tampered = json.loads(json.dumps(self.g))
        tampered["nodes"][0][3] += 0.5
        self.assertIn("fingerprint does not match nodes/edges", navgraph.validate(tampered)[0])
        bad2 = dict(self.g, schema="other")
        self.assertTrue(navgraph.validate(bad2)[0])

    def test_components_and_queries(self):
        G = navgraph.Graph(self.g)
        comp, scc = G.components()
        self.assertEqual(len(set(comp)), 2)            # main building + roof island
        roof = next(n[0] for n in self.g["nodes"] if n[3] == 9.0)
        ground = G.nearest([1.5, 1.5, 0.0])
        upper = G.nearest([7.5, 7.5, 4.0])
        self.assertGreaterEqual(ground, 0)
        self.assertEqual(self.g["nodes"][upper][3], 4.0)
        self.assertTrue(G.astar(ground, upper)[0])     # via the stairs
        self.assertFalse(G.astar(ground, roof)[0])
        self.assertEqual(G.nearest([1.5, 1.5, 30.0]), -1)  # no floor within 2.5 m: off graph
        self.assertFalse(G.astar(ground, upper, cap=2)[0])  # expansion cap like BotNav (6000)

    def test_failed_query_replay(self):
        mv = dict(events=[dict(type="astar_failed", t=1, pos=[1.5, 1.5, 0.05], goal=[22.5, 16.5, 9.05]),
                          dict(type="astar_failed", t=2, pos=[1.5, 1.5, 0.05], goal=[1.5, 1.5, 40.0])])
        rep = navgraph.analyze(self.g, mv)
        self.assertEqual(rep["failed_query_replay"]["reasons"], {"different_component": 1, "goal_off_graph": 1})


class BotLogTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r = botlog.analyze(FIX / "fixture.bots.log", {501: {"class": "SCAR", "pos": [21.0, 4.0, 0.9]}})

    def test_blocks_are_attached_to_their_actor(self):
        tracks = {t["entity"]: t for t in self.r["tracks"]}
        self.assertEqual(set(tracks), {1001, 1002})
        self.assertEqual(tracks[1001]["style"], "assault")
        s = tracks[1001]["samples"][0]
        self.assertEqual(s[7:10], [2.5, 2.5, 0])        # requested, actual, blocked from the same block
        self.assertGreaterEqual(self.r["meta"]["unparsed_blocks"], 1)   # the orphan Motion line

    def test_confirmed_events(self):
        c = self.r["summary"]["counts"]
        self.assertEqual(c["astar_failed"], 1)
        self.assertEqual(c["weapon_unreachable"], 1)
        self.assertEqual(c["stuck_reported"], 1)
        ev = next(e for e in self.r["events"] if e["type"] == "weapon_unreachable")
        self.assertEqual(ev["confidence"], "confirmed")
        self.assertEqual(ev["item_pos"], [21.0, 4.0, 0.9])
        kill = next(e for e in self.r["events"] if e["type"] == "kill")
        self.assertEqual(kill["victim"], "0:1001")
        self.assertNotIn("pos", kill)                 # killer's last sample is 3.5 s old: not attached
        self.assertEqual(kill["victim_pos_age"], 0.0)

    def test_inferred_windows(self):
        c = self.r["summary"]["counts"]
        self.assertEqual(c.get("stall_unblocked"), 1)
        self.assertEqual(c.get("loop_suspect"), 1)
        for e in self.r["events"]:
            if e["type"] in ("stall_unblocked", "loop_suspect"):
                self.assertEqual(e["confidence"], "inferred")
        self.assertGreater(self.r["summary"]["share_unblocked_no_motion"], 0)

    def test_clock_restart_starts_new_segment(self):
        a = botlog.Analysis()
        for line in ["5.00 [WarsBots] #1 created EntityId=7 name=b profile=assault/normal position=(0.0, 0.0, 0.0).",
                     "0.10 [WarsBots] #1 created EntityId=7 name=b profile=assault/normal position=(0.0, 0.0, 0.0)."]:
            a.feed(line)
        self.assertEqual(a.segment, 1)
        self.assertIn("1:7", a.profiles)

    @unittest.skipUnless(any(SOURCES.glob("*.bots.log")) if SOURCES.exists() else False, "real logs not present")
    def test_real_log_parses(self):
        log = sorted(SOURCES.glob("*.bots.log"), key=lambda p: p.stat().st_size)[0]
        r = botlog.analyze(log)
        self.assertGreater(r["summary"]["samples"], 0)
        self.assertLess(r["meta"]["unparsed_blocks"], max(5, r["summary"]["samples"] * 0.01))


class LevelDataTests(unittest.TestCase):
    def test_extract_from_synthetic_pak(self):
        mission = ('<Mission><Objects>'
                   '<Entity Name="SP1" Pos="1,2,3" EntityClass="SpawnPoint" EntityId="10"/>'
                   '<Entity Name="SCAR1" Pos="4,5,6" EntityClass="SCAR" EntityId="11" Layer="Weapons"/>'
                   '<Entity Name="Box" Pos="0,0,0" EntityClass="AreaBox" EntityId="12"><Properties BoxSizeX="5"/></Entity>'
                   '</Objects></Mission>')
        with tempfile.TemporaryDirectory() as d:
            pak = Path(d) / "level.pak"
            with zipfile.ZipFile(pak, "w") as z:
                z.writestr("mission_mission0.xml", mission)
                z.writestr("netmission0.bai", b"\0" * 96)
            xml = Path(d) / "L.xml"
            xml.write_text('<MetaData><MiniMap Filename="L.dds" startX="0" startY="10" endX="100" endY="110"/></MetaData>')
            m = level_data.extract(pak, xml)
            self.assertEqual(m["spawns"][0]["id"], 10)
            self.assertEqual(m["items"][0]["class"], "SCAR")
            self.assertEqual(m["areas"][0]["properties"], {"BoxSizeX": "5"})
            self.assertTrue(m["cryai_navigation"].startswith("absent"))
            self.assertEqual(m["minimap"]["start_y"], 10.0)
            log = Path(d) / "x.bots.log"
            log.write_text("1.0 [WarsBots] #1 revive requested EntityId=5 spawnId=10 previousSpawn=0 position=(1,2,3).\n"
                           "2.0 [WarsBots] REAL PICKUP actor=5 item=11 acquired=1.\n")
            c = level_data.check_ids(m, log)
            self.assertEqual((c["spawn_ids_in_mission"], c["item_ids_in_mission"]), (1, 1))


class MlValidateTests(unittest.TestCase):
    def rows(self):
        return [json.loads(l) for l in (FIX / "fixture_episodes.jsonl").read_text().splitlines() if l.strip()]

    def test_fixture_rows(self):
        issues, examples, flags = ml_validate.validate(self.rows())
        self.assertEqual(issues["E_end_before_start"], 1)
        self.assertEqual(issues["E_overlapping_episodes_same_actor"], 1)
        self.assertEqual(issues["E_context_complete_inconsistent"], 1)
        self.assertEqual(issues["W_suit_changed_during_episode"], 1)
        self.assertEqual(issues["W_censored_outcome"], 1)

    def test_log_cross_checks(self):
        with tempfile.TemporaryDirectory() as d:
            log = Path(d) / "a.bots.log"
            log.write_text("5.00 [WarsBots] #1 death observed EntityId=1 health=0 lastHitShooter=2 KillPlayerObserved=1.\n"
                           "1.50 [WarsBots] Hit after-Lua target=2 shooter=1 name=x weapon=3 damage=10 health=90.\n")
            rows = [dict(actor=1, bot=1, skill="normal", style="assault", target=2, start_seconds=1.0, end_seconds=8.0,
                         outcome="kill", damage_dealt_hp=0, damage_taken_hp=0, weapons=["SCAR"], suit_modes=[3],
                         actions={}, context_complete=True, schema=2, run="s:a", source_sha="abcdef1",
                         first_sampled_distance=10.0, first_sample_seconds=2.0, first_sample_visible=True,
                         distance_source="visible_target")]
            issues, _, _ = ml_validate.validate(rows, {"s:a": log})
            self.assertEqual(issues["E_crosses_life_boundary"], 1)
            self.assertEqual(issues["E_kill_outcome_without_kill_line"], 1)
            self.assertEqual(issues["W_context_sampled_after_first_hit"], 1)
            rows[0].update(outcome="death", end_seconds=5.0, target=9)
            issues, _, _ = ml_validate.validate(rows, {"s:a": log})
            self.assertEqual(issues["W_death_caused_by_other_than_target"], 1)


if __name__ == "__main__":
    unittest.main()
