# Optional server demo recorder

`wb_demo_record 1` arms server recording; default0. `wb_demo_hz` default20,10..60;
changing rate starts a new file so header rate stays correct. `wb_demo_start [label]`,
`wb_demo_stop`, `wb_demo_mark <text>` require the existing local IA test gate. Labels
use<=32ASCII letters/digits/_/-. No automatic bot spawn or actor mutation.

Format: frozen **wbdemo/2**, canonical spec owned by research/steelmill-triage
Docs/Research/SteelmillTriage/DEMO_FORMAT.md at414edb92. STRING before first use,
ACTOR join/leave,33-byte FRAME states, BRAIN+INTENT per decision tick, inventory
on change, stock gameplay shot/human-pickup/manual-drop/suit; successful direct bot pickup hook;
hit/kill/revive from existing
GameRules/instance hooks, bot jump/land from locomotion latches, human jump transition
and all-actor landing sampled every engine frame. Land latches deduplicated250ms.
Actual view quaternion, yaw[-18000,18000), pitch+up, peak inter-frame yaw preserved.
No new LOS rays, no information fed into AI. Perception cone/cloak bits describe cached
visible targets only; damage bit follows existing damage memory. LinkID0/navmesh none.

Server human view is replicated/smoothed; sprint bit0 for humans by contract.
Exact owner sidecar is deferred (C034 allows second commit), not claimed implemented.
World position = native entity feet; dead/spectating actors retained. INV max4 usable
weapon classes (skip Fists/OffHand); current weapon always included first.

Files LOCALAPPDATA/WarsBots/Demos/<map>-<localtime>[-label]-<sequence>.wbdemo.
Buffered,flush~1game second, close on manual stop/level/reset/shutdown. Force-killed
process may lose the final buffered second; parser intentionally tolerates partial tail.
512MB safety cap,64actors/string table limits; recording error disables retry until
flag toggled or map changes. DEMO PROFILE reports observer overhead including events.

**Exit for new wb_demo_record/hz: diagnostic-only opt-in, permanent tool; never
promote to default recording.** No runtime gameplay-policy flag added.

C025 runtime complete: two660s datasets (initial pickup gap preserved; fixed dataset
contains all mandatory events), one60s C040 diagnostic probe.20Hz, average cost
0.0339ms/frame for fixed baseline, exact peak disclosed in experiment report.
[Data/result](Experiments/c025-demo-2026-10-24.md). Codec sizes/yaw boundaries and
cross-Python reader fixture PASS; C040target-only metric test PASS.
BRAIN aim now raw cached target point when target_id exists. Parser target error uses
alive target FRAME XY, labelled yaw-only for preserved baselines; native3D reference
from eye is logged separately (no layout change). Client sidecar/playback pending.
