# Combat reference

## Source status

- **Magumba: NOT FOUND.** Web search (2026-10-23) returned no fragmovie, channel or player page
  under this name. Nothing below is attributed to Magumba.
- No other fragmovie was verified in this session either (the search tool does not index YouTube
  well). Next step: the user supplies links (YouTube/archive), or a session with the browser
  reviews them. Each entry must get: link, timestamps, techniques, and "game mechanic vs. edit".

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
