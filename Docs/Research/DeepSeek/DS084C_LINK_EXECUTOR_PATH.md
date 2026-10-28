# DS084c - what the one-Strength-link run can do in code today (offline prediction)

Companion to `DS084_DATA_RECIPE_REVIEW.md` / `DS084_INSTALL_PRECONDITION.md`. Read-only source audit of
`b06681ea`; no game/build. Purpose: pre-register the expected success/failure signature so the one native run is
interpretable, and confirm there is no *code* blocker for a +2.89 m rise.

## The chain (file:line)

1. Start gate `BotBrain.cpp:1422-1433`: only with an active mesh path, moving, not firing; take-off within 2 m
   horizontally and |dz| <= 2 m of `a`; logs `LINK start kind=strength`.
2. Rise accepted: `BotJumpLink.h:58,94` caps Strength links at `maxStrengthRise=3.72`; the candidate's rise is
   **2.890**, so it is not rejected as `too_high`.
3. Prepare `BotJumpLink.h:118-122`: asks for Strength mode and `energy>=90`, fails `no_energy` after 6 s.
4. Act `BotJumpLink.h:133-153` + `BotBrain.cpp:1465-1466`: sets gait **Sprint**, then `LinkJumpRequest::Strength`
   -> `BotLocomotion.cpp:107,115` requires Strength mode + 90 energy; `linkJump` **bypasses** the `JumpSpace`
   clearance/landing rays (`BotLocomotion.cpp:117`), otherwise a +2.89 m rise would be refused by its +-1.5 m
   landing test (`:82`). So the executor *can* physically take off upward here.
5. Landing `BotJumpLink.h:160`: needs grounded, 2-D distance to `b` <= 3.0 m and `|dz| < 1.5`.
6. Failure `BotBrain.cpp:1449-1458`: first failure only notes; the **second** `NoteLinkFailure` bans the link for
   the level (no ban on `no_energy`; `too_high` bans at once).

## Pre-registered expected outcome

The forced Sprint in Act is the only speed control; with the apex-4.7 arc at the audit's full-sprint `vh`=6.75 m/s
the take-off ends **7.29 m past `b`** (`upper-strength-arc-apex4.7.log`), i.e. beyond `landRadius` 3.0. So the
hypothesis to test is:

* expected: `LINK start kind=strength` -> `BOT ACTION: JUMP strength=1` -> `LINK failed kind=strength
  reason=missed_landing`, twice, then the link is banned and the bot re-paths (dry loop again);
* a **`LINK done`** instead would mean the native ballistic arc/air steering is not the modelled constant-arc and
  the held speed cap `3ec91e44` must NOT be applied;
* `reason=no_takeoff` / `no_energy` / `too_high` / no `LINK start` at all point at different causes (approach,
  energy/suit, geometry/rise, goal selection) and must not be reported as "speed".

## Limits

This predicts code behaviour, not physics: it does not prove the native Strength jump reaches 3.62/4.7 m in a PS
round, the true horizontal speed, or where the bot actually lands. Only the run decides that; the run still needs
the DS084 conditions (links file live, initial-upper check, selected capture target).
