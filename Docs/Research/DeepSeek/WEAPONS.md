# Weapon table from the game XML (DS-017 item 3)

Source: `E:\Electronic Arts\Crytek\Crysis Wars\Game\GameData.pak` (GOG install). The eight player weapons were
extracted to `%TEMP%\wb-weapons` with `tar -xf`; **no game file is committed**. Values are the XML `Scripts/Entities/
Items/XML/Weapons/<name>.xml` values at the listed lines. `rate` is rounds/minute; `damage` is per bullet unless `pellets`
is shown. The machine-readable copy is `weapons.json` next to this file.

## Summary

| Weapon | Mode | RPM | Damage/shot | DPS | Clip / reserve | Reload | Hip spread (min-max) | Recoil max/attack | XML |
| --- | --- | ---: | --- | ---: | --- | ---: | --- | --- | --- |
| SCAR | default | 700 | 21 | 245.0 | 40 / 80 | 1.5 s | 1.00-2.25 | 6 / 2.25 | `SCAR.xml:220-263` |
| FY71 | default | 570 | 22 | 209.0 | 30 / 60 | 1.45 s | 1.15-2.25 | 8 / 2.85 | `FY71.xml:205-260` |
| SMG | default | 950 | 15 | 237.5 | 50 / 100 | 1.35 s | 1.00-3.20 | 10 / 3.25 | `SMG.xml:190-245` |
| Shotgun | Shotgun (7 pellets) | 240 | 10 x 7 = 70 | 280.0 | 8 / 32 | 4.0 s | 3.00-6.00 (+pellet 5.0) | 12 / 5.00 | `Shotgun.xml:199-269` |
| SOCOM | default | 450 | 17 | 127.5 | 20 / 40 | 0.85 s | 1.00-3.50 | 12 / 4.00 | `SOCOM.xml:187-261` |
| DSG1 | Single | 38 | 88 | 55.7 | 10 / 20 | 1.8 s | 3.00-6.00 | 12 / 10.0 | `DSG1.xml:150-205` |
| GaussRifle | Single | 25 | 130 | 54.2 | 5 / 10 | 2.05 s | 3.00-6.00 | 12 / 10.0 | `GaussRifle.xml:155-200` |
| Hurricane | Rapid | 1000 | 21 | 350.0 | 500 / 500 | 3.2 s | 1.35-2.35 | 10 / 1.25 | `Hurricane.xml:135-216` |

`DPS = damage_per_shot * rpm / 60`; it ignores reload, travel, misses and damage drop. For the Shotgun it assumes all
seven pellets hit. For the Hurricane the XML also has `ai_vs_player_damage=40` (`Hurricane.xml:148`); the table uses
the normal `damage=21` against players. The SOCOM Burst mode is 2 x 17 damage at 800 rpm (`SOCOM.xml:225-229,277-280`).

`spread` is the XML spread value; do not re-scale it in code. `recoil` is `max_recoil / attack` from `<recoil>`.
Full `maxx`, `maxy`, `decay`, `randomness`, `speed_m`, `rotation_m` and per-mode details are in `weapons.json`.

## Per-weapon notes

### SCAR (`SCAR.xml`)

- `rate=700`, `damage=21`, `clip_size=40`, `reload_time=1.5`, `bullet_chamber=1`, `ammo_type=bullet`
  (`:220-231`); ammo `bullet amount=40 extra=80` (`:16-18`).
- Spread `1.0/2.25`, attack `1.0`, decay `0.55`, `speed_m=1.25`, `rotation_m=0` (`:256-263`).
- Recoil `max_recoil=6`, attack `2.25`, decay `0.30`, maxx `4.5`, maxy `0.95`, randomness `0.25` (`:232-255`).
- Modes: default, Rapid (autozoom, inherits damage/rate), Single (auto_fire), disabled GrenadeLauncher (200 damage).
- Zoom: ironsight 1.4x enabled; reflex 1.4x, assaultscope 3.5x and sniper 4x/10x are `enabled=0` in the base XML
  (accessories can enable them) (`:447-...`).

### FY71 (`FY71.xml`)

- `rate=570`, `damage=22`, clip 30, reserve 60, reload 1.45, ammo `fybullet` (`:205-216`).
- Spread `1.15/2.25`, attack `1.15`, decay `0.50` (`:244-...`); recoil max 8, attack 2.85, decay 0.30, maxx 6.0,
  maxy 1.15 (`:217-...`).
- Modes: default, Rapid, Single; disabled GrenadeLauncher (`:341-...`). The ironsight spread mod is **1.0**
  (no spread reduction), while the other scopes use 0.75 (`:371-...`).

### SMG (`SMG.xml`)

- `rate=950`, `damage=15`, clip 50, reserve 100, reload 1.35, ammo `smgbullet` (`:190-201`).
- Spread `1.0/3.2`, attack `1.0`, decay `0.70` (`:229-...`); recoil max 10, attack 3.25, maxx 5.5, maxy 2.0,
  randomness 0.85 (`:202-...`).
- Modes: default, Rapid, Single. Zoom modes exist (`:326-...`), but the zoom recoil mod is unusual: maxx/maxy
  `1.10` (zoom makes horizontal/vertical recoil slightly worse, not better).

### Shotgun (`Shotgun.xml`)

- The `<firemode type="default">` base has `rate=240`, `damage=10`, clip 8, reserve 32, reload 4.0, and damage drop
  starting at 10 m at 1.0 per metre (`:199-212`).
- The selectable `Shotgun` mode has 7 pellets x 10 damage, pellet spread 5.0 (`:260-269`); `Narrow` has 7 x 10 with
  pellet spread 1.3 (`:249-258`). The table uses the 7 x 10 mode.
- Base spread `3.0/6.0`, attack 3.15, decay 1.0, speed_m 0.35 (`:224-...`); recoil max 12, attack 5.0, maxx 22,
  maxy 3.5, randomness 4.0 (`:213-...`).
- Zoom entries exist but have **no `spreadMod`**, so zooming does not reduce the shotgun spread; only recoil mod
  applies.

### SOCOM (`SOCOM.xml`)

- `rate=450`, `damage=17`, clip 20, reserve 40, reload 0.85, `lightbullet` (`:187-201`).
- Spread `1.0/3.5`, attack 1.75, decay 0.50, speed_m 1.15 (`:254-261`); recoil max 12, attack 4.0, maxx 12,
  maxy 0.95, randomness 0.5 (`:230-253`).
- Burst: `nshots=2`, internal burst rate 160, Burst fire rate 800 (`:225-229,277-280`).
- Zoom: custom `zoommode` 1.4x, zoom in/out 0.115 s; `ironsight` has no stages.

### DSG1 (`DSG1.xml`)

- Single: `rate=38`, `damage=88`, clip 10, reserve 20, reload 1.8, `sniperbullet`, `ht_SniperBullet` (`:160-171`).
- Spread is declared in the type-default block: `3.0/6.0`, attack 3.0, decay 1.0, rotation_m 1.0, speed_m 1.15
  (`:150-159`); Single inherits it.
- Recoil max 12, attack 10.0, decay 0.30, maxx 8, maxy 3.0, randomness 2.0 (`:172-182`).
- Base zoom is ironsight 1.4x. The useful sniper scope is an accessory: `SniperScope -> zoommode=sniper` 4x/10x,
  `AssaultScope -> 3.5x`, `Reflex -> 1.4x` (`:466-...`), with the scope modes `enabled=0` until attached.
- This is why a DSG1 bot without the scope attachment still has only 1.4x; `BotWeaponHandling.h:15-20` zooms at
  8 m regardless of the attachment/scope state.

### GaussRifle (`GaussRifle.xml`)

- Single: `rate=25`, `damage=130`, clip 5, reserve 10, reload 2.05, `gaussbullet` (`:165-179`).
- Spread from the type-default block: `3.0/6.0`, attack 3.5, decay 1.0, rotation_m 1.0, speed_m 1.15
  (`:155-164`); recoil max 12, attack 10.0, maxx 16, maxy 3.0, randomness 2.0 (`:189-200`).
- Same scope accessories as DSG1; base ironsight 1.4x, sniper 4x/10x when attached.

### Hurricane (`Hurricane.xml`)

- Rapid: `rate=1000`, `damage=21`, `ai_vs_player_damage=40`, clip 500, reserve 500, reload 3.2, max distance 800
  (`:135-149`).
- Spread `1.35/2.35`, attack 0.75, decay 0.50 (`:172-177`); recoil max 10, attack 1.25, decay 0.70, maxx 25,
  maxy 1.35, randomness 0.25 (`:190-199`).
- No zoom modes. `prone_not_usable=1` in the item params (the enum is set for the Hurricane).

## Zoom levels

All rifles except the Hurricane have the same four XML zoom modes:

| Mode | Stages | In/out | Base XML enabled | Spread mod max | Recoil attack / maxx / maxy |
| --- | --- | --- | --- | --- | --- |
| ironsight | 1.4x | 0.2 / 0.2 s | yes | 0.75 (FY71: 1.0) | 0.75 / 0.75 / 0.75 |
| reflexsight | 1.4x | 0.2 / 0.2 s | no (`enabled=0`) | 0.75 | 0.75 / 0.75 / 0.75 |
| assaultscope | 3.5x | 0.2 / 0.2 s | no | 0.75 | 0.75 / 0.75 / 0.75 |
| sniper | 4x / 10x | 0.15 / 0.125 s | no | 0.75 | 0.75 / 0.75 / 0.75 |

`enabled=0` means not enabled by the base item; the `<accessory zoommode="...">` entries in the weapon XML enable them
when the attachment is present (for example `DSG1.xml:466`). The bot's own handling does not inspect the attachment
name: `ZoomDistance` only uses the weapon class name (`BotWeaponHandling.h:15-20`) and `Handle` calls
`StartZoom`/`StopZoom` (`BotWeapons.cpp:271-273`).

The SMG's zoom recoil mod is `maxx/maxy 1.10` (slightly worse), not the usual 0.75. The Shotgun's zoom has no spread
mod; the SOCOM's custom zoom is 1.4x with 0.115 s transition.

## Effective range / how this feeds DS-015

The XML has **no general effective-range field** for bullet weapons. Only the Shotgun has a damage drop
(`start 10 m`, `1.0/m`, `Shotgun.xml:210-211`) and the Hurricane has `distance=800` (`Hurricane.xml:147`). The code's
two range tables are separate:

- Bot desired/strafe range (`BotProfile.h:47-64`): SOCOM 10, Shotgun 6, SMG 12, SCAR/FY71 18, DSG1/Gauss 35,
  Hurricane 20 m. Style scale and `rangeAdapt` still modify this (`:59-63`).
- Weapon ranking (`BotWeaponRank.h:25-32`): best/worst window and max:
  SCAR 5-45/90, FY71 5-40/80, Hurricane 3-20/45, SMG 2-20/40, Shotgun 0-8/18, SOCOM 0-10/30, DSG1 35-200/300,
  Gauss 30-200/300 m. This is a ranking falloff, not a hard fire gate.

Consequences for bot long range:

1. The **fire distance gate is style `fireRange`, not the XML**: Assault 32, Universal/Flanker/Hunter 40, Defender 50,
   Marksman 65 m (`BotProfile.h:97,103,112,22`; used in `BotBrain.cpp:854`). That is why the owner saw no bot hit
   beyond 40 m even though SCAR/FY71 can do damage at longer range.
2. A scoped DSG1/Gauss is the only honest "reach" weapon, but `BotProfile::WeaponRange` still wants 35 m and the
   bot only zooms at 8 m (`BotWeaponHandling.h:19`). The XML supports 4x/10x only with the `SniperScope` accessory;
   without it the base is 1.4x ironsight.
3. The Shotgun's damage falls after 10 m (`1.0/m`) and its base spread is 3-6 plus 5 pellet spread; it is not a
   20 m weapon, regardless of the bot's `fireRange` 40.
4. The Hurricane's `ai_vs_player_damage=40` (`Hurricane.xml:148`) is an AI-only value; do not use it as the player
   DPS number. Its XML `distance=800` is a projectile limit, not a practical accuracy range.

## Extraction / reproducibility

```powershell
$tmp = Join-Path $env:TEMP 'wb-weapons'
$pak = 'E:\Electronic Arts\Crytek\Crysis Wars\Game\GameData.pak'
$paths = @(
  'Scripts/Entities/Items/XML/Weapons/SCAR.xml',
  'Scripts/Entities/Items/XML/Weapons/FY71.xml',
  'Scripts/Entities/Items/XML/Weapons/SMG.xml',
  'Scripts/Entities/Items/XML/Weapons/Shotgun.xml',
  'Scripts/Entities/Items/XML/Weapons/SOCOM.xml',
  'Scripts/Entities/Items/XML/Weapons/DSG1.xml',
  'Scripts/Entities/Items/XML/Weapons/GaussRifle.xml',
  'Scripts/Entities/Items/XML/Weapons/Hurricane.xml'
)
tar -xf $pak -C $tmp $paths
```

Do not commit the extracted XML or `GameData.pak`; only `weapons.json` (numbers) and this Markdown are in the
repository. The XML is a Crytek game asset; the LICENSE.txt terms still apply to the game data.

## Open checks for the owner / Codex

- Confirm which Shotgun fire mode is selected by default (base 10 vs `Shotgun` 7x10). The table uses the 7-pellet
  mode because it is the named `Shotgun` mode; if the engine picks the base `default`, per-shell damage is 10.
- Confirm the exact scope accessory distribution in the MP loadout. The XML marks sniper/assault/reflex scopes
  `enabled=0` on the base item; the accessory entries enable them (`DSG1.xml:466-494`), but a bot spawned without the
  attachment has only ironsight 1.4x.
- `rate` values are XML rounds/minute. If the engine uses a different tick quantization, probe the actual shots per
  second in a 10 s sample before using DPS for balance.
