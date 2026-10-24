"""Ported from warsbots/research/steelmill-lab (ad34d283) Tests/SteelmillLab: level_data and ml_validate."""
import json, sys, tempfile, unittest, zipfile
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from wblab import level_data, ml_validate  # noqa: E402
FIX = Path(__file__).resolve().parent / "fixtures"


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
