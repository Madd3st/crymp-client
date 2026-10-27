# C-139 PS buy: `ok=0` is a C++ return-type bug, not Lua (DS-055)

Base `ba4d5f4e` (`C-139`). Inputs: `Docs/Coordination/C139_PS_REPORT.md` (commit `1cbcf135`), raw
`build/c139-Shore-ba4d5f4e/` (`metrics.json`, `*-run.log`, `ps-evidence.log`, demo). No build, no game run in this
analysis (DS-055 rule); the patch is proposed, not integrated.

## What C139 observed

- 12 attempts (6 initial, 6 after the 31.7 s native reset), each `PS BUY ... primary=... ok=0 laser=lam ok=0`.
- Yet 12 `PS EVENT purchase` lines, all `alive=true`: `smg price=75` (bots 1/3/5) and `shotgun price=50` (bots 2/4/6),
  in both spawn sets.
- Demo inventory per actor: `SOCOM + SMG` or `SOCOM + Shotgun`, selected primary matches; **no LAMRifle**.
- PP 100 -> 25 (SMG) / 50 (shotgun), exactly `100 - price`; `PS SUMMARY buys=0` because it counts the C++ `ok` values.

The purchase event is emitted inside `BuyItem` only after the inventory check, the PP award and the item grant
(`PowerStruggleBuying.lua:1005`, `:1007`, `:1012`), immediately before `return true` (`:1033`). A false `BuyItem`
cannot produce that line or the PP drop, so **Lua returned `true`**; only the C++ read is wrong.

## Root cause

Lua:
- `PowerStruggle:WarsBotsBuy(playerId, itemName)` (`PowerStruggleBuying.lua:490-507`) returns booleans:
  `BuyItem(...)` (`:503`), `DoBuyAmmo(...)` for ammo (`:498`), `false` on every guard (`:491-495`, `:500`, `:506`).
- `PowerStruggle:BuyItem` returns `true` on success (`:1033`) and `false` on rejection (`:876`, `:884`, `:896`, `:920`,
  `:953`, `:958`, `:969`, `:1022`, `:1026`).
- `PowerStruggle:DoBuyAmmo` returns `true` only when rounds were actually added (`:799` vehicle, `:836` player) and
  `false` otherwise (`:722`, `:727`, `:841`).

C++ (`BotBrain::PSTryBuy`, `Code/CryGame/Bots/BotBrain.cpp:523-528` on `ba4d5f4e`):
```cpp
int ok=0; ss->EndCall(ok); return ok!=0;
```
`IScriptSystem::EndCall(T&)` (`Code/CryCommon/CryScriptSystem/IScriptSystem.h:301-306`) fills a `ScriptAnyValue` and
calls `CopyTo(T&)`. `CopyTo(int&)` accepts only `ANY_TNUMBER` (`:117`); a Lua boolean is `ANY_TBOOLEAN`, so the copy
fails and `ok` stays `0`. `EndCall(bool&)` uses `CopyTo(bool&)` (`:116`), which accepts `ANY_TBOOLEAN`.

So `primary=false` for every delivered primary. `laser=primary&&plan.buyLaser?buy(plan.laser):false`
(`BotBrain.cpp:530`) short-circuits: `buy("lam")` is never called, and the summary line still prints
`laser=lam ok=0`, which reads like a failed laser purchase but is an unattempted one. The same applies to the shotgun
ammo branch (`:531`).

Why not change Lua to `return ... and 1 or 0`: the boolean is the correct Lua contract for all three functions, and a
number would codify the wrong C++ type. The narrow fix is the C++ type.

## Shipped narrow patch (`patches/ds055_buy_return.patch`, delivered in `c040bdfb`)

One line in `BotBrain::PSTryBuy` (`BotBrain.cpp:527` on `ba4d5f4e`), BotBrain.cpp only:

```cpp
bool ok=false; ss->EndCall(ok); return ok;    // was int ok=0; ss->EndCall(ok); return ok!=0;
```

No other behaviour change, no new flag, `wb_ps_buy` stays 0. Applies clean on `ba4d5f4e`.

## Oracle catching it (rerun gate, no code oracle required)

The DS-055a addendum already defines the live oracle; the C139 evidence gives the exact expected values. An optional
in-code check would be a PP read around every alive buy:

```cpp
int ppNow=ppBefore; /* GetPlayerPP */ ...;
if (ok != (ppNow < ppBefore)) WB_LOG(0, "PS BUY RETURN MISMATCH item=%s lua=%d pp=%d->%d", ...);
```

not shipped: the rerun gate below fails loudly without it, and keeping the fix one line avoids touching the log format
the C139 metrics parser reads. If Claude wants the check in code, it is a separate diagnostic patch, no new flag.

Every PS-2 plan item has a price > 0 (primary 75/50, `lam` 25, `shotgunshell` 5), so an alive delivery always spends
PP: `lua=0` with a PP drop is the C-139 signature; `lua=1` with no drop is a free/failed buy. Dead revive buys are
excluded because `BuyItem` queues the item without charging until revive (`:1018-1021`).

## Ammo-return semantics and 100 PP arithmetic

`WarsBotsBuy` returns `DoBuyAmmo`'s boolean for `shotgunshell`/`smgbullet` (`:498`), so the same `bool` fix covers
ammo. `DoBuyAmmo` returns `true` only when it added rounds and charged (or queued) them; a full ammo pool returns
`false` with no PP change, which is a legitimate `ok=0`.

At exactly 100 PP (`BotPSBuy.h:36-50`):

| style | plan | cost | PP left | expected after fix |
| --- | --- | --- | ---: | --- |
| close (assault/flanker/hunter or range < 12) | shotgun + lam + shotgunshell | 50+25+5 | 20 | 3 purchases, 3 events |
| other | smg + lam | 75+25 | 0 | 2 purchases, 2 events |
| other, all three | smg + lam + smgbullet | 75+25+5 = 105 | -5 | impossible at 100 PP |

The SMG path cannot buy all three at 100 PP; the plan correctly skips the 5 PP ammo. If the owner wants ammo too, it
needs 105 PP (or ammo-first at 80, no laser) - a plan decision, not this bug.

## Oracle / acceptance (unchanged strictness)

1. Unit: `Tests/BotPSBuyTests.cpp` already locks the 100 PP plans (SMG+laser, shotgun+laser+shells); it passes on
   `ba4d5f4e`, which proves the plan is not the failing part.
2. Correction run: same C137 preset as C139 (capture/buy/events 1, doors/vehicles 0, 6 Normal, 300 s, demo + raw logs),
   after the patch:
   - `PS BUY` for each spawn set: `ok=1` for primary and `lam`; shotgun also `ammo=shotgunshell ok=1`;
   - 12 primary + 12 `lam` purchase events (plus 12 `shotgunshell`), all `alive=true`;
   - PP 100->0 for SMG, 100->20 for shotgun with ammo; inventory contains `LAMRifle` for all 6 in both sets;
   - `PS BUY RETURN MISMATCH` absent;
   - `metrics.json`: `successful_items=24` (36 with shells), `successful_laser_bots` 6, `purchase_gate_pass=true`,
     Lua/RMI/channel errors 0.
3. Do not accept primary-only delivery, `ok=1` without a purchase event, or a laser event without `LAMRifle` in the
   inventory. Any `MISMATCH` line is a BLOCK.

## Residual risks (oracle distinguishes them)

- `lam` is `@mp_catAddons`, `equip=true` (`PowerStruggleBuying.lua:145`), flag `BUY_EQUIPMENT=4` (`:19`,
  `GetItemFlag` `:352-362`). `BuyZone_US13` has `bEquipment=1`; the runtime zone that allowed the NK/US-spawn primary
  buys is not logged, so its equipment flag is unproven. If the correction run gets `lam ok=0` **with no PP drop**, the
  cause is the zone/flags or `CheckInventoryRestrictions("LAMRifle")`, not the return type; the mismatch oracle stays
  silent in that case. Do not widen the patch for it until the run shows it.
- Dead-buy coverage is still absent in C139 (0 deaths); a revive-buy run needs inventory-after-revive, not PP.

## Review duty

Ready for the `review <sha>: OK/BLOCK` item (encoding/BOM, flags all default OFF and no flips, no client RMI or
channel 0, compile_check + run_units + the run evidence above). No code was run for DS-055 and no claude-sprint SHA was
touched.
