# Playtest presets

`Shore-15-mixed.cfg` (owner 07.11, step 6): the Shore-15 server profile from the 07.11 playtest
(`E:/WarsBots/Playtests/Shore-20261107/Config/A.cfg`) plus:

- `wb_bot_difficulty 1`: the DS-014 table (reaction, turn rate, aim error, head zone, fire gate, range) per skill.
  The 07.11 playtest ran with all bots `normal` and this flag 0, so every bot played the same.
- Mixed skill: 6 normal, 6 veteran, 3 elite. The team is the parity of the bot number, so the teams get 3/3/2 and 3/3/1.
- The new feature flags of the 07.11 package, each on its own line.

To switch one feature off, set its line to 0 or delete it. The full list "how to switch off one by one" goes into the
shared playtest package.

The `.cfg` has no comment lines: CrySystem is the original binary, and how it reads comments is not confirmed in this
repository. Lines to add once their code PR is merged: `wb_bot_ps_squads 1` (SPEC-005 stage 1), `wb_bot_sub_exit 1`
(SPEC-006a).
