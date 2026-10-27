# Review 5f9d5096 vs ds048.patch (DS-049 #1, 10 lines)

1. Gate placement is right: `BotJumpLink.h:105` (Approach) and `:147` (Act) only wrap the fail; `:49/:64` add `LinkParams::blockedFailFast` and the setter.
2. `BotBrain.cpp:1191` calls `SetBlockedFailFast` inside `ExecuteIntent` **before** `ApplyLinkExecutor`, so a link started in the same tick sees the flag.
3. The goal-age change is gated consistently at `BotBrain.cpp:652` (`linkFailFast==1 && m_link.Active()`); with the flag off the 20 s timer behaves as before.
4. Data is not lost: `Steelmill.links.txt`/manifest are byte-identical to ds048 (2 `removed` lines, FNV `523caf82c65722fc`), and `BotNavMeshCoreTests.cpp:98-99` subtracts exactly those lines.
5. `BotJumpLinkTests.cpp:63` constructs `LinkParams.blockedFailFast=true`, so case 7 actually tests the gate, not the default.
6. PS-1 is intact: `BotPSCapture.*` and its test are byte-identical, hook at `BotBrain.cpp:715`, `BotPSCapture` in CI and `BotPSCapture.cpp` in CMake.
7. Nothing dropped or merged wrong in the diff; only line endings/BOM differences.
8. Residual (harmless): the blocked timer still runs when the flag is off (`BotJumpLink.h:101-102/143-144`), so it maintains `m_blockedSince` without failing.
9. Residual (QA-only): `SetBlockedFailFast` mutates the params after construction; toggling `wb_bot_link_failfast` mid-link takes effect on the next `ExecuteIntent`.
10. Residual: the `removed` counter is a `" removed"` substring test; correct for this file, but a future comment/id containing that text would over-count.
