# FPS AI research: bounded goals before more tactics

Reviewed 2026-10-23. Source review is selective, not a claim to have audited every subsystem.
No third-party implementation was copied or linked into WarsBots.

| Primary source | Useful implementation | WarsBots decision / cost |
| --- | --- | --- |
| [YaPB navigate.cpp](https://github.com/yapb/yapb/blob/master/src/navigate.cpp), [tasks.cpp](https://github.com/yapb/yapb/blob/master/src/tasks.cpp), [license](https://github.com/yapb/yapb/blob/master/UNLICENSE) | Route validity tied to current goal; accumulated stuck detection and recovery. Repository is Unlicense. | Retain our FSM and bounded A*. Revalidate item goals while travelling (small change); progress-aware recovery next. Weapon and difficulty subsystems not reviewed in depth. |
| [Xonotic Havocbot](https://github.com/xonotic/xonotic-data.pk3dir/blob/master/qcsrc/server/bot/default/havocbot/havocbot.qc), [server AI cvars](https://github.com/xonotic/xonotic-data.pk3dir/blob/master/xonotic-server.cfg) | Failed goal temporarily ignored; clear and rebuild routes; independent perception/strategy/weapon intervals and distance-specific priorities. | Keep our existing 90-second bad-item cache; evaluate failed-route cancellation; first QA shows the sparse graph makes hard rejection too aggressive. Preserve local fallback and throttle failed queries instead. No code reuse: per-file/project licensing still needs review before any copying. |
| [RCBot2 schedules](https://github.com/APGRoboCop/rcbot2/blob/master/utils/RCBot2_meta/bot_schedule.cpp), [repository license](https://github.com/APGRoboCop/rcbot2/blob/master/LICENSE) | Tasks carry failure interrupts and target-lifetime conditions. Repository says AGPL-3.0; inspected schedule source carries an older GPL header. | Adopt the idea of cancelling a task when its item becomes unavailable; do not import code with mixed license notices. Small, native implementation. Full class tactics are deferred. |
| [Detour off-mesh links](https://github.com/recastnavigation/recastnavigation/blob/main/Detour/Include/DetourNavMesh.h), [license](https://github.com/recastnavigation/recastnavigation/blob/main/License.txt) | Explicit typed links with endpoints and flags; zlib license. | Future Strength transitions must have validated takeoff/landing and executor feedback. Do not add Recast now: extracting geometry and replacing a working graph is substantial work. |
| [Quake III BotLib movement](https://github.com/id-Software/Quake-III-Arena/blob/master/code/botlib/be_ai_move.c) | Reachability links, travel types, timed avoidance of failed links. File carries GPL-2.0-or-later notice. | Next: directional failed-edge memory and completion feedback, independently implemented. AAS import not justified. |

Chosen now: target-lifetime cancellation, failed-path retry caching, machine-readable before/after metrics.
Hard rejection of every missing graph route is retained only as a documented failed experiment.
Next useful change: record actual route progress, not just requested movement being blocked.
These are engineering improvements; none constitutes machine learning.
