# C140 DS054 door/probe integration runtime verdict

Exact **545a5022d06ce1edf219df3d57a9470e4f8295a5**, parent ba4d5f4e; CI40571455339 GREEN and independent DS056 review OK aa9a6546 before runtime. One Shore6Normalx300 with C139 combat recipe + doors1/events1/vehicles0, actorless/roster effective checks PASS. Native demo6026frames/301.305s readable; no game binary built locally.

**PARTIAL, not full acceptance.** Crash/Lua/RMI/channel0, one `PS DOOR open` at31.50s (bot1/entity104/pre-action2); old OnUsed/use path0. Native server opening request exercised with no prior C138 client-RMI errors. This log records the call and pre-state, not a visual/physical post-state proof. Reviewed code writes only Open(user,1), no bot close request.

Stalls81->96 (+18.52%, <=25% gate PASS), traps1->0 PASS; kills/deaths/wave0/0/0 so death-revive unexercised. Twelve own-base initial/reset spawns PASS. Purchase events12; inherited C139 return/laser bug remains separate (not fixed by DS054). Other event types unexercised.

NK exit<=20s remains **UNKNOWN**, no full gate PASS: first20s world positions saved per initial/reset actor in `nk-first20.json`; maximum displacement8.4..19.24m, samples within0.017s of20s. A task-approved authoritative base boundary is absent, so displacement does not prove leaving base. Only opening request at31.50s is later than20s. DS057 requests map-grounded classification before any repeat.

Initial memory differs from C139 snapshot; single native RNG run, no causal claim. Original memory actualSHA256 and absent-file state restored; own server stopped, no client/GUI or soak. Nav files not modified by harness, no pre-nav hash claim. Raw `build/c140-Shore-545a5022/` contains demo/logs/effective config/arguments/harness/summary/PS events and hashes. [JSON](C140_PS_RESULT.json).
