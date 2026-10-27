# C140 NK base exit: map-grounded classification (DS-057)

Source: `build/c140-Shore-545a5022/nk-first20.json`, raw run log, and the Shore mission XML
(`ps-Shore.xml`, extracted from `level.pak`). No runtime replay, no build.

## Reproducible boundary

Door 104 is `Door48` in `Base_NK`: `Pos="1842.8513,1255.3259,111.27002"`, rotation ~identity,
`fUseDistance="5"`, `fFrontAxis="y"` (`ps-Shore.xml:1790-1795`). It is the only `Base_NK` door on the
spawn-room corridor: the room is `VisArea232` at `1837.94,1246.12`, height 3.25 (`ps-Shore.xml:1798-1802`).

Use the door threshold plane as the spawn-room/base gate:

* interior (spawn side): `y < 1255.3259` and within `2D distance <= 5 m` of the door;
* exit: `y > 1255.3259` at the door aperture (the stock `fUseDistance` radius is the conservative aperture proxy);
* the full NK perimeter is larger (`HQ_NK` at `1785.75,1270.125`, `ps-Shore.xml:5390`), and no collision mesh is in
  the repo. So this gate proves leaving the spawn room, not the whole NK perimeter; full-perimeter exit is **unknown**.

## Classification at 20 s (all still inside)

| set | bot | actor | y at 20 s | signed y-1255.33 | 2D dist to door | max disp 0-20 s | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- |
| initial | 1 | 65486 | 1251.77 | -3.55 | 4.40 | 19.24 | inside |
| initial | 3 | 65476 | 1250.72 | -4.61 | 4.62 | 11.91 | inside |
| initial | 5 | 65466 | 1251.81 | -3.52 | 3.91 | 8.77 | inside |
| reset | 5 | 131013 | 1249.72 | -5.60 | 6.27 | 11.01 | inside |
| reset | 3 | 131003 | 1247.67 | -7.65 | 7.69 | 17.81 | inside |
| reset | 1 | 130993 | 1250.60 | -4.72 | 5.36 | 8.40 | inside |

No actor has `y > 1255.3259`; maximum displacement (8.4-19.24 m) is movement along the spawn-room corridor, not a
base exit. `max_displacement` is therefore not an exit metric.

## Why the open request is at 31.50, not <=20 s

* `PS DOOR open entity=104 actor=65486 state=2` at 31.50 (`door-evidence.log:1`, `run.bots.log:1109`); `state=2` is
  `DOOR_CLOSE`, so it is an open request, not a passage proof.
* Bot 1 was already within 5 m of the door at 20 s (3.91-4.40 m), yet no request fired. The request coincides with
  the 31.50 path/goal change (`run.bots.log:1110`, goal `(2291.47,1536.97)`); the door scan is path-segment/heading
  gated, so proximity alone does not trigger it.
* The next event is the game reset at 31.53 (`run.bots.log:1116`), 0.03 s later. There is no post-state or passage
  line, so physical crossing is **unproven**; the request itself is still at `y=1251.62`, inside the boundary.

Verdict: **0/6 exited the NK spawn-room/base gate by 20 s** (initial and reset separately). C140's `NK exit <=20 s`
gate is FAIL, not UNKNOWN, against this map-defined gate; the whole-base perimeter remains unknown without collision
geometry. The 31.50 request is too late and reset-truncated to count.
