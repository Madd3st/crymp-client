import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'Tools'))
from wb_tactics_report import parse

LOG="""0.00 [WarsBots] #1 created EntityId=11 name=WarsBot_01 profile=assault/elite.
0.00 [WarsBots] #2 created EntityId=22 name=WarsBot_02 profile=defender/beginner.
0.10 [WarsBots] Bot 11 acquired enemy id=22 name=WarsBot_02 kind=bot.
0.20 [WarsBots] Hit before-Lua target=22 shooter=11 name=x weapon=55 damage=500 health=100 botTarget=2 botShooter=1.
0.21 [WarsBots] #1 combat kill shooter=11 target=22 via stock GameRules::KillPlayer.
0.22 [WarsBots] Hit after-Lua target=22 shooter=11 name=x weapon=55 damage=500 health=-1067 botTarget=2 botShooter=1.
0.23 [WarsBots] Hit before-Lua target=33 shooter=11 name=x weapon=55 damage=20 health=100 botTarget=0 botShooter=1.
0.24 [WarsBots] Hit after-Lua target=33 shooter=11 name=x weapon=55 damage=20 health=80 botTarget=0 botShooter=1.
0.30 [WarsBots] #2 death observed EntityId=22 health=0 lastHitShooter=11.
0.40 [WarsBots] TACTIC SAMPLE actor=11 skill=elite style=assault weapon=SCAR suit=3 distance=15.0 action=ATTACKING target=22 visible=1.
1.00 [WarsBots] #1 ENTITY_EVENT_DONE EntityId=11.
"""

class ReportTests(unittest.TestCase):
    def test_effective_hp_and_incidental_target(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'run.log'; path.write_text(LOG)
            episodes=[]; skills=parse(path,episodes.append)
            self.assertEqual(skills['elite']['damage_dealt_hp'],120)
            self.assertEqual(skills['beginner']['damage_taken_hp'],100)
            self.assertEqual(skills['elite']['kills'],1)
            self.assertEqual(skills['beginner']['deaths'],1)
            self.assertEqual(len(episodes),1)
            self.assertEqual(episodes[0]['target'],22)
            self.assertEqual(episodes[0]['damage_dealt_hp'],100)
            self.assertEqual(episodes[0]['outcome'],'kill')
            self.assertEqual(episodes[0]['weapons'],['SCAR'])

    def test_actual_motion_despite_unblocked_requests(self):
        for points,key in (([(0,0)]*4,'stationary_windows'), ([(0,0),(3,0),(3,3),(0,0)],'loop_suspect_windows')):
            text="0.0 [WarsBots] #1 created EntityId=11 name=WarsBot_01 profile=assault/elite.\n"
            for i,(x,y) in enumerate(points):
                t=1+2*i
                text+=f"{t}.0 [WarsBots] AI telemetry EntityId=11 state=CHASING_ENEMY pos=({x}.0, {y}.0, 1.0) health=100 weapon=55 ammo=20 target=22.\n"
                text+=f"{t}.0 [WarsBots] MOTION INTENT: requestedBase=2.50 actual=0.0 blocked=0.\n"
            with tempfile.TemporaryDirectory() as tmp:
                path=Path(tmp)/'run.log'; path.write_text(text)
                result=parse(path,lambda ep:None)['elite']
                self.assertEqual(result['blocked_samples'],0)
                self.assertEqual(result['progress_windows'],1)
                self.assertEqual(result[key],1)

    def test_dataset_limit(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp); (root/'run.log').write_text(LOG+LOG.replace('11','44').replace('22','55'))
            (root/'runs.json').write_text(json.dumps([dict(tag='test',source_sha='test',log='run.log')]))
            subprocess.run([sys.executable,str(ROOT/'Tools/wb_tactics_report.py'),str(root/'runs.json'),
                '--output',str(root/'out.json'),'--episodes',str(root/'out.jsonl'),
                '--max-episodes','1','--max-bytes','1024'],check=True,capture_output=True)
            result=json.loads((root/'out.json').read_text())
            self.assertEqual(result['episode_rows'],1)
            self.assertTrue(result['episode_limit_reached'])
            self.assertLessEqual((root/'out.jsonl').stat().st_size,1024)

if __name__=='__main__': unittest.main()
