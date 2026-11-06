import importlib.util
from pathlib import Path
import unittest
p=Path(__file__).resolve().parents[1]/'Tools/Spec004/trace.py'
s=importlib.util.spec_from_file_location('spec004trace',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
def row(scope='level-end',p95='2.000',n=100,dropped=0,mode=1):
 return f'60.0 [WarsBots] PATHTIME {scope} level=Multiplayer/PS/Shore n={n} p50=0.2 p95={p95} max=4.0 ms dropped={dropped} unstick={mode} filter=virtual.\n'
class TraceTests(unittest.TestCase):
 def test_exact_boundary_max_is_not_gate(self):self.assertEqual(m.parse_trace(row(),1,'Multiplayer/PS/Shore')['gate'],'PASS')
 def test_above_boundary(self):self.assertEqual(m.parse_trace(row(p95='2.001'),1,'Multiplayer/PS/Shore')['gate'],'FAIL')
 def test_level_is_not_level_end(self):self.assertEqual(m.parse_trace(row('level'),1,'Multiplayer/PS/Shore')['gate'],'NOT_MEASURED')
 def test_window_cannot_override_end(self):self.assertEqual(m.parse_trace(row()+row('window',p95='3'),1,'Multiplayer/PS/Shore')['gate'],'PASS')
 def test_dropped_or_empty_invalid(self):
  for r in [row(n=0),row(dropped=1),row(p95='nan')]:self.assertEqual(m.parse_trace(r,1,'Multiplayer/PS/Shore')['gate'],'INVALID')
 def test_selector(self):self.assertEqual(m.parse_trace(row(mode=0),1,'Multiplayer/PS/Shore')['gate'],'NOT_MEASURED')
 def test_new_level_requires_new_end(self):self.assertEqual(m.parse_trace(row()+row('level'),1,'Multiplayer/PS/Shore')['gate'],'NOT_MEASURED')
 def test_truncated_end_ignored(self):self.assertEqual(m.parse_trace(row().rstrip(),1,'Multiplayer/PS/Shore')['gate'],'NOT_MEASURED')
 def test_events_do_not_count_memory(self):
  txt='1 [WarsBots] #1 UNSTICK step=sprint actor=4.\n2 [WarsBots] #1 UNSTICK exit strength=0 actor=4.\n3 [WarsBots] #1 UNSTICK failed next=none actor=4.\n4 [WarsBots] UNSTICK MEMORY record transitions=1.\n'
  self.assertEqual(m.parse_trace(txt,1,'Multiplayer/PS/Shore')['events'],dict(step=1,exit=1,failed=1,abort=0))
if __name__=='__main__':unittest.main()
