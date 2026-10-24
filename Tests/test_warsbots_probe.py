import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('warsbots_probe',ROOT/'Tools/warsbots_probe.py')
p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)

class ProbeTests(unittest.TestCase):
 def test_manifest_frozen_counts_and_invalid_input(self):
  d=json.loads((ROOT/'Tools/Probes/c031-v1.json').read_text())
  p.validate_manifest(d)
  self.assertEqual({m:sum(c['map']==m for c in d['cases']) for m in p.MAPS},dict(Steelmill=8,Quarry=4,Armada=4,Outpost=4))
  for mutate in [lambda x:x['cases'].append(x['cases'][0]),lambda x:x['cases'][0]['bots'][0]['position'].__setitem__(0,float('nan')),lambda x:x['cases'][0].__setitem__('seconds',5)]:
   bad=copy.deepcopy(d);mutate(bad)
   with self.assertRaises(ValueError):p.validate_manifest(bad)
 def test_flips_ignore_duplicates_and_death_boundary(self):
  def row(t,state):return dict(t=t,id=1,state=state)
  self.assertEqual(p.flips([row(0,'A'),row(.5,'A'),row(1,'B'),row(2,'A')]),1)
  self.assertEqual(p.flips([row(0,'A'),row(1,'B'),row(2,'DEAD'),row(2.5,'A')]),0)
  self.assertEqual(p.flips([row(0,'A'),row(1,'B'),row(4,'A')]),0)
 def test_stationary_excludes_dead_and_target_and_motion(self):
  def data(flags=1,target=0,moving=False):
   return dict(actors={1:dict(kind='bot')},brain=[dict(id=1,t=0,target=target)],frames=[(i*.1,[(1,i if moving else 0,0,0,0,0,0,flags)]) for i in range(21)])
  self.assertAlmostEqual(p.stationary(data())['seconds'],2)
  self.assertEqual(p.stationary(data())['episodes_ge1s'],1)
  for d in [data(flags=0),data(target=2),data(moving=True)]:self.assertEqual(p.stationary(d)['seconds'],0)
 def test_specific_pickup_wire_and_map_validation(self):
  with tempfile.TemporaryDirectory() as td:
   file=Path(td)/'run.wbdemo';w=p.demo.Writer(file,dict(map='Multiplayer/IA/Steelmill',build='a'*40))
   w.actor(0,1,'bot','Bot');w.brain(0,1,'SEARCHING_ENEMY')
   for i in range(301):w.frame(i*.1,[dict(id=1,x=0,y=0,z=0)])
   cls=w.sid('SCAR');w.event(1,'pickup',1,other=42,value=cls);w.close()
   case=dict(id='p',map='Steelmill',seconds=30,kind='pickup',bots=[{}],expected_item=dict(id=42,**{'class':'SCAR'}))
   self.assertEqual(p.analyze(file,case)['outcome'],'reached')
   case['expected_item']['id']=43
   self.assertEqual(p.analyze(file,case)['outcome'],'not_reached')
   case['map']='Quarry'
   with self.assertRaises(ValueError):p.analyze(file,case)
   file.write_bytes(file.read_bytes()+b'\x03\x07\x00x')
   case['map']='Steelmill'
   with self.assertRaises(ValueError):p.analyze(file,case)
 def test_jump_rejects_late_travel_jump_and_wrong_mode(self):
  with tempfile.TemporaryDirectory() as td:
   case=dict(id='j',map='Steelmill',seconds=30,kind='jump',jump_mode='strength',bots=[{}])
   for n,(t,mode,expected) in enumerate([(2.5,1,'landed'),(10,1,'not_completed'),(2.5,0,'not_completed')]):
    f=Path(td)/('j%d.wbdemo'%n);w=p.demo.Writer(f,dict(map='Multiplayer/IA/Steelmill',build='a'*40))
    w.actor(0,1,'bot','Bot');w.brain(0,1,'SEARCHING_ENEMY')
    for i in range(301):w.frame(i*.1,[dict(id=1,x=0,y=0,z=0)])
    w.event(t,'jump',1,value=mode);w.event(t+1,'land',1);w.close()
    self.assertEqual(p.analyze(f,case)['outcome'],expected)
if __name__=='__main__':unittest.main()
