# C142 native PS purchase result: PASS on 532e4497

Exact **532e4497d018cb7f6fea61b6ad92e4d4390849d7**, CI40574773650 GREEN, DS058 independent review OK before runtime. DS055 one-file bool-return correction only, wb_ps_buy default0 unchanged. One Shore6Normalx300, C139 recipe/events1/doors0/vehicles0, actorless/roster44 effective checks PASS.

All **12 initial/reset actors** (6 bots each spawn set) primaryok1/laserok1, each has one native primary purchase event and one `item=lam` event; selected SMG/Shotgun is recorded for each actor. Six shotgun-ammo successes; **30 successful operations, 24 item purchase events** (ammo uses its own stock path). Parent C139 reportedok0/LAM0 oracle is now GREEN. No PP/item grants or stock-buy bypass.

Laser purchase is corroborated by native event+boolean success, **not** by demo accessory inventory or beam rendering. The recorder includes IWeapon only (`BotDemoRecorder.cpp:138`), so LAM absence from its inventory snapshots is a coverage limit. Attachment/beam visuals UNKNOWN. Incoming PP100 and item prices75/50+25 observed; exact post-purchase PP not sampled, no measured after-delta claim.

Stability: crash/Lua/RMI/channel0; own-base groups12/12 PASS. Stalls84, traps1 (<=1 PASS). Kills/deaths/wave0/0/0: no death-revive coverage. Demo6019frames/300.958s readable. Purchase event parser exercised24; other PS event types unexercised. Single native RNG run; do not treat stall changes as causal.

Original memory actualSHA256 and absent-file state restored. No game/client active, no local game binary build, default promotion, sniper repeat or soak. Raw `build/c142-Shore-532e4497/`; [JSON](C142_PS_RESULT.json). C140 door RMI fix is separate: DS057 maps spawn-room threshold and finds0/6 NK actors exited by20s, so its exit gate FAIL despite stable opening-call path. Whole-base perimeter/physical door pass remains unproved.
