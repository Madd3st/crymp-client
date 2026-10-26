import sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'Tools/SteelmillTriage'))
from wblab.logparse import parse_lines
class ProfileNameTests(unittest.TestCase):
 def test_plain_and_profile_names_keep_slot_entity_skill(self):
  for name in ['WarsBot_01','WarsBot_01 [Beginner/Universal]']:
   with self.subTest(name=name):
    runs=parse_lines([f'0.06 [WarsBots] #1 created EntityId=65501 name={name} profile=universal/beginner position=(1, 2, 3).'])
    self.assertEqual(runs[0].slot_entity,{1:65501})
    self.assertEqual(runs[0].bots[65501].skill,'beginner')
 def test_missing_profile_is_not_attributed(self):
  runs=parse_lines(['0.06 [WarsBots] #1 created EntityId=65501 name=WarsBot_01 [Beginner] position=(1, 2, 3).'])
  self.assertEqual(runs[0].bots,{})
if __name__=='__main__':unittest.main()
