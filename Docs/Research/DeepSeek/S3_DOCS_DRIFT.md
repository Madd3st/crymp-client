# S3 docs drift: Docs/Wiki vs claude-sprint `b8b444d6`

Read-only comparison (research-branch wiki pages vs `origin/warsbots/feature/claude-sprint` code). Claude fixes the
pages; this is the list. Each line: page:line -> what is stale -> correction.

1. `Console-Commands.md:25` "Фичи спринта (все по умолчанию 0)" -> `corners=1` and `jumpSanity=1` are ON by default
   (`BotConfig.h`, D-016), and several pace/protection flags default 1 -> say "по умолчанию 0, кроме отмеченных".
2. `Console-Commands.md:25-39` -> the PS feature flags are missing entirely -> add
   `wb_ps_capture`, `wb_ps_buy`, `wb_ps_doors`, `wb_ps_vehicles`, `wb_ps_events` (all default 0, PS-only, gated).
3. `Navigation.md:31` "сетка работает на всех картах" -> Shore is **not** shipped: only the diagnostic candidate
   `E2235993` (reviewed, used by C149, **not installed/accepted**; `%LOCALAPPDATA%\WarsBots\Navmesh` has no Shore) ->
   distinguish the four installed maps from the temporary Shore candidate.
4. `Navigation.md:37-38` -> mesh description lacks the water area (`area 4`, cost 4) and the bounded water query start
   (water-only, plane-sized, behind `BotWaterAllowed` + `waterMovement`); `wb_bot_jump_links` also exposes the
   Strength link used by the upper route (C149), not only drop/jump -> add one paragraph.
5. `Status-and-Roadmap.md:39` -> PS "дальше покупка, двери, навмеш Shore, техника, захват" is outdated: buy measured
   (C142), doors implemented (C140 partial), capture **route** reached 31.2 m from 654 (C149), vehicles implemented
   behind `wb_ps_vehicles` but runtime-untested, Shore mesh still only a candidate.
6. `Status-and-Roadmap.md:43` -> the 27.10 "боты на нейтральных бункерах, ничего не покупали" is a historical snapshot;
   spawn/team fixed (C127/C134) and buy works (C142) -> mark as history, not current state.
7. `Status-and-Roadmap.md:68` -> "Навигационная сетка Shore, удаление старой навигации | следующий" -> a reviewed Shore
   candidate exists and the upper route is proven (C149); what remains is acceptance/installation + removing the old
   BotNav fallback, not the first mesh.
8. `Status-and-Roadmap.md:92` -> "боты пока не покупают, не открывают двери и не садятся в технику; у Shore нет
   навигационной сетки" -> stale: buy/doors implemented and gated (buy measured), vehicles untested; the Shore
   navmesh exists as a diagnostic candidate only (no default install, no data promotion).

Not stale (checked): `Navigation.md` link/`wb_bot_jump_links` gating, `Console-Commands.md:41` install path,
`Status-and-Roadmap.md:50` sniper 0/3.
