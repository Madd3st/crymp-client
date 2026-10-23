# Combat reference

## Source status

- **Magumba: FOUND (user-supplied link, 2026-10-23).** "CW fragmovie called MAGUMBA",
  https://www.youtube.com/watch?v=rGGDAkaIjRE , channel Babumbaaaa, published 2011-08-07, 4:16.
  Description: Crysis Wars fragmovie of two Russian players, **Babumba and Magic8** (Magumba =
  both nicknames), frags from ESL cups, ladders and gathers. ESL profiles:
  esl.eu player 4815149 (Babumba), 4450817 (Magic8).
- **Technique analysis: NOT DONE.** Only a 144p stream was available to the automated browser,
  and seeking did not refresh frames; suit HUD, weapons and movement are not readable. Nothing
  below is derived from this video yet.
- Best data source available: Magic8 is the project owner. A first-hand description of the
  signature patterns (suit cycles, range bands, strafe/jump timing), and later recorded server
  logs of his own matches against bots, beat any edited video for both design and imitation data.

Until then this file lists **in-engine mechanics** already confirmed in CryMP logs and how strong
classic-MP play uses them. These are design targets, not observations from a video.

## Mechanic targets (confirmed in CryMP code/logs, not in video)

| Technique | Mechanic in CryMP | Bot status |
|---|---|---|
| Aimed strafe, direction changes | independent move/look, replicated strafe flag | implemented, 2/2 per side on open ground |
| Shooting while moving | native fire during locomotion | implemented |
| Speed chase / disengage | native sprint, energy drain, hysteresis | chase implemented, vs fleeing human UNTESTED |
| Strength jump | native jump, energy ≥90, clearance check | 2/2 physical |
| Armor on damage | DEFENSE when recently hit or firing | implemented (damagedUntil 3 s) |
| Cloak approach | cloak while unseen at 12–30 m | gated OFF by default |
| Cover, flanking, height | — | not implemented (needs navigation, P5) |

## What a reviewed video entry must contain

```
### <title> — <link>
Player: <name, how confirmed> | Map/mode | Version (Crysis 1.2.1 / Wars)
00:12 strength jump to roof → speed → shotgun drop (mechanic)
00:31 slow-motion (edit, ignore)
Suit cycle: ... | Range bands: ... | Aim: tracking / flick / burst length
```
