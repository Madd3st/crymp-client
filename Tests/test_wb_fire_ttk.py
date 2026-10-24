import sys,unittest,tempfile
from pathlib import Path
sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'Tools'))
from wb_tactics_report import parse_fire_ttk

def hit(t,phase,shooter,hp,energy=200,head=0):
 return f"{t:.2f} [WarsBots] FIRE HIT phase={phase} target=20 shooter={shooter} projectile=99 weapon=10 class=SCAR actor=1 human=1 health={hp} suit=3 energy={energy:.2f} distance=5.00 material=1 head={head} part=2 damage=26.000.\n"
class FireTTK(unittest.TestCase):
 def parse(self,text,limit=1000):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'x.log';p.write_text(text,encoding='utf-8');return parse_fire_ttk(p,limit)
 def test_multishooter_armor_overkill(self):
  text='0.50 [WarsBots] FIRE SHOT shooter=1 target=20 projectile=99 weapon=10 class=SCAR mode=Rapid remote=0.\n'
  text+=hit(1,'before-Lua',1,100)+hit(1,'after-Lua',1,100,161)
  text+=hit(1.3,'before-Lua',2,100,0)+hit(1.3,'after-Lua',2,60,0)
  text+=hit(1.6,'before-Lua',1,60,0)+hit(1.6,'after-Lua',1,-1067,0,1)
  e=self.parse(text)['episodes'][0]
  self.assertEqual(e['hp_lost'],100);self.assertEqual(e['armor_energy_spent'],39)
  self.assertEqual(e['head_hits'],1);self.assertEqual(e['observed_contributors'],[1,2])
  self.assertEqual(e['contributors_last_second'],[1,2]);self.assertEqual(e['impact_to_death_seconds'],.6)
  self.assertEqual(e['first_impact_projectile_launch_seconds'],.5)
  self.assertEqual(e['observed_targeted_fire_to_death_seconds'],1.1)
 def test_revive_censor_and_missing_before(self):
  text=hit(1,'before-Lua',1,100)+hit(1,'after-Lua',1,60)
  text+=hit(1.5,'before-Lua',1,60)
  text+='2.00 [WarsBots] revive result EntityId=20 sameEntity=1 alive=1\n'
  text+=hit(3,'after-Lua',1,0)
  r=self.parse(text);self.assertEqual(r['episodes'][0]['outcome'],'lifecycle_boundary')
  self.assertIsNone(r['episodes'][0]['impact_to_death_seconds']);self.assertEqual(r['counters']['unmatched_after'],1)
 def test_invulnerability_and_replica_shots(self):
  text='0.50 [WarsBots] FIRE SHOT shooter=1 target=20 projectile=99 weapon=10 class=SOCOM remote=1.\n'
  text+=hit(1,'before-Lua',1,100).replace('actor=1','invulnerable=1 actor=1')+hit(1,'after-Lua',1,100)
  text+=hit(2,'before-Lua',1,100)+hit(2,'after-Lua',1,0)
  r=self.parse(text);self.assertEqual(r['counters']['replica_shot_events'],1)
  self.assertEqual(r['counters']['invulnerable_impacts_excluded'],1)
  self.assertEqual(r['episodes'][0]['first_impact_seconds'],2)
 def test_authoritative_cadence_excludes_replica_and_unknown(self):
  lines=[]
  for t,clip,remote in [(1,20,0),(1,19,1),(1.25,19,0),(1.5,18,-1)]:
   lines.append(f"{t:.2f} [WarsBots] FIRE SHOT shooter=1 target=20 projectile=99 weapon=10 class=SOCOM rpm=450 clip={clip} remote={remote}.\n")
  r=self.parse(''.join(lines));c=r['cadence']['SOCOM']
  self.assertEqual(c['shots'],2);self.assertEqual(c['active_interval_min'],.25)
  self.assertEqual(c['clip_decrement_values'],[1])
  self.assertEqual(r['counters']['shot_callbacks_unknown_replica_state'],1)
 def test_shot_only_context_does_not_survive_revive(self):
  text='1.00 [WarsBots] FIRE SHOT shooter=1 target=20 projectile=99 weapon=10 class=SOCOM remote=0.\n'
  text+='1.50 [WarsBots] revive result EntityId=20 sameEntity=1 alive=1\n'
  text+='2.00 [WarsBots] FIRE SHOT shooter=1 target=20 projectile=99 weapon=10 class=SOCOM remote=0.\n'
  text+=hit(2.1,'before-Lua',1,100)+hit(2.1,'after-Lua',1,0)
  e=self.parse(text)['episodes'][0]
  self.assertEqual(e['first_observed_targeted_shot_seconds'],2)
  self.assertEqual(e['observed_targeted_fire_to_death_seconds'],.1)
 def test_clip_context_breaks_on_lifecycle_gap_and_unknown(self):
  def shot(t,clip,remote=0):
   return f"{t:.2f} [WarsBots] FIRE SHOT shooter=1 target=20 projectile=99 weapon=10 class=FY71 rpm=640 clip={clip} remote={remote}.\n"
  text=shot(1,17)+'1.05 [WarsBots] revive result EntityId=1 sameEntity=1 alive=1\n'+shot(1.1,7)
  text+=shot(5,3)+shot(5.1,2,-1)+shot(5.2,1)
  r=self.parse(text)
  self.assertEqual(r['cadence']['FY71']['clip_decrement_values'],[])
  self.assertIsNone(r['cadence']['FY71']['active_interval_min'])
 def test_old_logs_are_unavailable_and_cap(self):
  self.assertFalse(self.parse('0.10 [WarsBots] Hit after-Lua target=311 health=-1')['available'])
  text=hit(1,'before-Lua',1,100)+hit(1,'after-Lua',1,-5)
  self.assertEqual(len(self.parse(text,0)['episodes']),0)
  self.assertEqual(self.parse(text,0)['counters']['episode_cap_drops'],1)
if __name__=='__main__':unittest.main()
