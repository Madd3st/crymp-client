# PS-2 revive buying (DS-049 #3)

Behind `wb_ps_buy 0` (default OFF). On death in PowerStruggle, while the bot is still dead (2 s after death, before
the ~17 s wave), it buys one primary through a **server-side** Lua entry and the stock revive queue applies it when
the wave revives it.

## Chooser (`Code/CryGame/Bots/BotPSBuy.h:32`, engine-free)

`ChoosePsBuy({pp, style, skill, preferredRange, hasPrimary, alive})` -> `{primary, price, ammo, ammoPrice}`:

- marksman (style 2) or preferred range > 30 m: DSG1 200 if PP >= 200, else FY71 125, else shotgun 50;
- range < 12 m or PP < 125: shotgun 50;
- otherwise SCAR (buyList id `macs`) 150 if PP >= 150, else FY71 125;
- one primary only (`hasPrimary`/`alive` -> no buy); ammo only after the primary and only if >= 40 PP remain
  (bullet/fybullet/shotgunshell 5, sniperbullet 10).
- Prices/ids from the stock buyList: `PowerStruggleBuying.lua:38` shotgun 50, `:40` FY71 125, `:41` SCAR (`macs`)
  150, `:43` DSG1 200, `:44` Gauss 600; ammo `:108-116`. Gauss is not chosen: 600 PP is worse than DSG1 + ammo on
  these maps. PS has **no armor item**; `armorMode` is the free Armor suit already used on revive.

## Server-side path (no per-client RMI)

`PowerStruggleBuying.lua:490` adds `PowerStruggle:WarsBotsBuy(playerId, itemName)`:
`WarsBotsIsBot` guard -> team/frozen/`ItemExists` -> `IsVehicle` reject -> dead **or** `IsInBuyZone` ->
`EnoughPP` -> `BuyItem`. It never calls `SvBuy` (`:455`) and never calls `ClBuyOk/ClBuyError` (`:481/483`), which is
the channel-0 RMI that DS-045 had to suppress. Stock functions used: `GetPlayerPP` `:222`, `ItemExists` `:376`,
`IsVehicle` `:382`, `EnoughPP` `:388`, `BuyItem` `:868`, `IsInBuyZone` `:1196`; the dead purchase is appended to
`revive.items` (`:997`) and committed by `PowerStruggle:CommitRevivePurchases` (`PowerStruggle.lua:890`).

`BotBrain.cpp:502 PSTryBuy` reads PP with `GetPlayerPP`, runs the chooser, then calls `WarsBotsBuy` for the primary
and (if planned) ammo; `BotBrain.cpp:477` triggers it once per life while dead. Log:
`PS BUY pp=... primary=... price=... ok=... ammo=... ok=...`.

## Tests

`Tests/BotPSBuyTests.cpp` (CI `BotPSBuy`): PP thresholds 0/49/50/125/160/200/300/400, SCAR vs FY71 vs shotgun vs
DSG1, sniper/close range, ammo reserve (50 left buys, 35 does not), hasPrimary/alive skip. MSVC
`/std:c++17 /permissive-`: pass.

## Limits

- Not run in the engine; the Lua syntax is not executed by the unit test.
- Ammo while dead depends on the stock `HasItem` check on the live inventory (the primary is still in `revive.items`
  at that moment), so `ok=` may be 0; the log makes it visible and the primary is the important part.
- Captures/PP grants and the wave revive remain stock; nothing here writes HP or kills.
