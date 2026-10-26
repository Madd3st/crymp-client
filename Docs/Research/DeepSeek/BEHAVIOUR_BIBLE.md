# Bot Behaviour Bible — как играет хороший игрок в Crysis Wars

Дата: 2026-10-26. Документ для владельца, не код. Термины можно читать по-английски. Источники: разборы
`PLAYTEST_CHECKLIST_C075.md`, `PLAYTEST_REPORT_REVIEW.md`, `TARGETING_RANGE.md`, `AIM_REPLICATION.md`, `WEAPONS.md`,
`PS_RULES.md`, `PS_MAPS.md`, `VEHICLES.md`, `SELF_PRESERVATION.md`, текущий код `claude-sprint` и флаги `wb_*`.
Пометка **[ПРЕДПОЛОЖЕНИЕ]** означает игровое решение, а не измеренный факт.

## Как читать статусы

- **есть** — поведение реализовано; флаг может быть OFF по умолчанию.
- **частично** — есть основа, но нет важного условия/выхода.
- **нет** — поведения нет либо оно было проверено и провалилось.
- **B/E** — Beginner / Elite из `BotDifficulty.h`: реакция 0.9/0.08 с, поворот 250/1200 °/с, ошибка 2.5/0.1°,
  хед 0/95 %, огонь 20/2°, дальность ×0.55/×1.45, strafe нет/есть, jump 0/60 %, reload 0/60 %, cloak нет/есть
  (`BotDifficulty.h:30-38`). Флаги `wb_bot_difficulty` и `wb_bot_finish` по умолчанию OFF; C-093/X-108 зелёный.

## A. Спавн и оружие

**A1. Спавн с пистолетом.**
Хороший игрок: не бежит в центр с SOCOM; идёт к ближайшему нормальному оружию/патронам, по пути слушает бой.
B/E: Beginner может сразу искать врага; Elite выбирает оружие под дистанцию и не теряет время.
Сейчас: **частично** — `BotBrain::UpdateWeaponSeek`/`BotWeapons::SelectAvailable`, `wb_bot_weapon_rank` OFF по
умолчанию; стартовый SOCOM и fallback есть. Мера: время до первого primary, `REAL PICKUP`, `WEAPON RANK`.

**A2. Выбор оружия при подборе.**
Хороший игрок: SCAR/FY71 как универсал, SMG/Shotgun в тесном, DSG1/Gauss на высоте, SOCOM только fallback.
B/E: Beginner берёт ближайшее; Elite оценивает дистанцию, патроны и апгрейд.
Сейчас: **есть** — `BotWeaponRank.h`, `SelectForRange`, `PickupValue`; флаг `wb_bot_weapon_rank`, default OFF. Мера:
`WEAPON RANK`, доля времени с SCAR/DSG1, `REAL PICKUP`.

**A3. Патроны и reload.**
Хороший игрок: перезаряжается за укрытием/между боями, не начинает бой с 3 патронами.
B/E: Beginner ждёт пустой клип; Elite делает tactical reload при <60 % и не стреляет в reload.
Сейчас: **частично** — `ReloadIfNeeded`, `BotWeapons::TacticalReload` из difficulty; вызывается только вне боя
(`BotBrain.cpp:346`). Мера: `TACTICAL RELOAD`, выстрелы при 0 патронах, простой.

**A4. Добивание почти мёртвого.**
Хороший игрок: не меняет цель, пока не добил; не даёт уйти за угол на 4 HP.
B/E: Beginner часто теряет; Elite преследует и добивает.
Сейчас: **частично** — `wb_bot_finish` + 6 s cap (`BotCombat.cpp:232-241`), C-093 зелёный; флаг OFF. C-093 всё ещё
407 switch old≠0 / 357 damaged; мера: switch-away от цели <25 HP, kill share.

**A5. Лут после убийства.**
Хороший игрок: забирает патроны/лучшее оружие, если это не под огнём.
B/E: Beginner идёт дальше; Elite лутает по ситуации.
Сейчас: **частично** — `wb_bot_tactics` loot (`BotBrain.cpp:331-339,576`), default OFF. Мера: `REAL PICKUP` после
kill, death within 3 s of loot.

## B. Передвижение и вертикаль

**B1. Маршрут по карте.**
Хороший игрок: идёт по коридорам/лестницам, не режет углы в стену, не застревает на перилах.
B/E: Beginner чаще цепляет; Elite срезает безопасно.
Сейчас: **есть/частично** — `wb_bot_navmesh`, `wb_bot_corners` default ON, `Steer`/`SteerCorners`; C-078/X-078
зелёный, но X-088 с `navmesh=0` не считается. Мера: stalls, `MOTION INTENT blocked=1`, TRAP clusters.

**B2. Крыши и верхние этажи.**
Хороший игрок: знает лестницы/прыжки, занимает высоту, смотрит вниз на входы.
B/E: Beginner редко; Elite использует крышу и уходит с неё.
Сейчас: **частично** — `wb_bot_vantage` (только Marksman/Defender с DSG1/Gauss), `wb_bot_threat_scan` default OFF;
C-098: 0 glance, DSG1 roof не достигнут. Мера: `VANTAGE`, `GLANCE`, время на высоте, kills from z+3.

**B3. Прыжок в Strength на крышу/уступ.**
Хороший игрок: оценивает высоту, энергию, площадку; не прыгает в пустоту.
B/E: Beginner почти нет; Elite делает 1–3 прыжка на высоту.
Сейчас: **частично** — `BotJumpLink`, `BotLocomotion::JumpSpace`, obstacle jump; `wb_bot_jump_sanity` default ON
отключает случайные/лестничные прыжки. Нет планировщика «крыша как цель». Мера: jump takeoff/landing, fail rate,
trap level.

**B4. Strength-прыжок от стены.**
Хороший игрок: отталкивается от стены, чтобы забраться на 2–3 м.
B/E: Beginner нет; Elite да.
Сейчас: **нет** отдельного wall-jump; есть только link/dodge. Мера: probe `wall_jump`, высота/энергия.

**B5. Speed bunny hop.**
Хороший игрок: разгоняется на длинных прямых, не тратит Speed в бою.
B/E: Beginner нет; Elite 9–13 м/с на прямой.
Сейчас: **частично** — `wb_bot_pace` + `BotSuitPolicy` Speed, но нет bhop-тайминга; в бою Armor. Мера: average
speed on >30 m straight, energy, hits taken during travel.

**B6. Дверь/лестница/лифт.**
Хороший игрок: открывает дверь, не застревает в проёме, ждёт лифт.
B/E: Beginner чаще; Elite использует.
Сейчас: **частично** — двери как препятствия, лифты/лестницы не как link; `wb_bot_navmesh` + links. Мера:
`TRAP RECOVERY`, stalls at doors/lifts.

## C. Первый контакт и дистанция

**C1. Контакт 5 м.**
Хороший игрок: Shotgun/SMG, hip-fire, strafe, не scope; не стоит в двери.
B/E: Beginner поздно реагирует; Elite стреляет в голову/грудь и двигается.
Сейчас: **частично** — `rangeAdapt`, `preferredRange` 6–20, difficulty fire gate; Beginner no-strafe bug в DS-024
(`BotBrain.cpp:827-828`). Мера: first-shot delay, hits<10 m, deaths<5 m.

**C2. Контакт 20 м.**
Хороший игрок: SCAR/FY71, короткие очереди, strafe/jump, использует укрытие.
B/E: Beginner стоит; Elite strafe+jump, 2° gate.
Сейчас: **частично** — `BotCombat::Attack`, `BotAimControl`, `BotFireControl`, `wb_bot_difficulty`; C-093 aim
median 1.3° Elite, но headshot 9.9 %. Мера: hit distance p50, TTK, shots per kill.

**C3. Контакт 50 м.**
Хороший игрок: SCAR одиночными/короткими, ищет угол; DSG1 если есть.
B/E: Beginner не стреляет; Elite стреляет и сближается/зумит.
Сейчас: **частично** — `fireRange` Universal 40, Marksman 65; difficulty ×1.45 только у Elite; C-098 hits>40 m
есть, but no bot hit after 40 in owner run (DS-015). Мера: hits>40 m, `FIRE SHOT class`.

**C4. Контакт 80 м.**
Хороший игрок: snipes only with scope, otherwise closes using cover.
B/E: Beginner нет; Elite DSG1/Gauss.
Сейчас: **частично** — scan 80 m, fireRange max 65, no sniper hold. Мера: kills by DSG1/Gauss, hits 50–80 m.

**C5. Враг за укрытием.**
Хороший игрок: не стреляет в стену; держит угол, кидает гранату, фланкует.
B/E: Beginner ждёт/стреляет; Elite фланкует/граната.
Сейчас: **частично** — `wb_bot_tactics` hold angle (`BotTactics.h:48`), `ShotBlocked` step-out; нет grenade/flank.
Мера: shots blocked, `HOLD angle`, kill after flank.

**C6. Враг на крыше / снайпер.**
Хороший игрок: не идёт по открытому, ищет лестницу, дым/гранату, отвечает снайперу.
B/E: Beginner не замечает; Elite проверяет высокие точки.
Сейчас: **частично/нет** — `wb_bot_threat_scan` C-098 0 glances; `wb_bot_vantage`; DS-015: bots never look up. Мера:
`GLANCE`, deaths from z+3, time to acquire roof.

**C7. Тебя снайпер держит.**
Хороший игрок: ломает LOS, меняет высоту, не выглядывает тем же углом.
B/E: Beginner повторяет; Elite уходит/фланкует.
Сейчас: **частично** — retreat/withdraw at low HP, cover hold; нет «being sniped» threat. Мера: deaths by DSG1/Gauss,
time exposed, `WITHDRAW`.

## D. Состояние и бой

**D1. Низкое HP.**
Хороший игрок: уходит за cover, лечится/реген, не меняет цель на полного врага.
B/E: Beginner дохнет; Elite отступает, контратакует.
Сейчас: **частично** — `withdraw` 2.5 s, `retreatHealth`; `SELF_PRESERVATION.md` предлагает Retreat goal (нет в
arbiter). Мера: deaths at <30 HP, time to cover, heals.

**D2. Нет патронов.**
Хороший игрок: перезаряжается/меняет оружие/уходит, не стоит с пустым.
B/E: Beginner; Elite.
Сейчас: **частично** — `ReloadIfNeeded`, `SelectAvailable`, `TacticalReload`; нет «out of ammo» retreat. Мера:
shots with 0 ammo, time empty.

**D3. Несколько врагов.**
Хороший игрок: не стоит между двумя, фокусирует одного, использует узкий проход.
B/E: Beginner; Elite.
Сейчас: **частично** — `VisibleEnemies`, target utility; только Elite low HP + 2 enemies withdraw. Нет threat
count/positioning. Мера: deaths with 2+ visible, target switch rate.

**D4. Слышал выстрелы, не видит.**
Хороший игрок: поворачивается/идёт проверить, не стреляет вслепую.
B/E: Beginner; Elite.
Сейчас: **есть частично** — `BotPerception` hearing 60/12 m, `HEARD gunshot`, `BotLookPolicy` glance; `botPerception`
default OFF. Мера: `HEARD`, time to investigate, kills from heard.

**D5. Враг в cloak.**
Хороший игрок: замечает по шуму/выстрелу, стреляет в силуэт, не идёт в открытую.
B/E: Beginner; Elite.
Сейчас: **частично** — cloak 12 m, reveal 1.5 s, `wb_bot_cloak_experimental`; difficulty Elite cloak gate bug
(DS-024). Мера: cloak kills/deaths, detection distance.

**D6. Цель почти мертва.**
См. A4. Сейчас `wb_bot_finish`; мера switch-away.

**D7. После убийства.**
Хороший игрок: reload, смена позиции, лут, ожидание второго.
B/E: Beginner; Elite.
Сейчас: **частично** — loot flag, tactical reload, no post-kill reposition. Мера: death within 3 s after kill,
reload, pickup.

## E. Костюм и гранаты

**E1. Armor под огнём.**
Хороший игрок: Armor при перестрелке/падении.
B/E: оба, Elite быстрее.
Сейчас: **есть** — `BotSuitPolicy` Fight/UnderFire/Falling, `BotSuit::Apply`; policy default OFF. Мера: mode at hit,
damage taken.

**E2. Speed на переходе.**
Хороший игрок: Speed на длинной прямой, снимает в бою.
B/E: Beginner осторожно; Elite агрессивно.
Сейчас: **есть частично** — `TravelGait`, `BotSuitPolicy::SpeedOk` 130/60, distance 20/8; нет bhop. Мера: travel
speed, energy, mode in fight.

**E3. Strength для прыжка.**
Хороший игрок: Strength только на прыжок, затем Armor.
B/E: Beginner; Elite.
Сейчас: **есть** — jump link/dodge + suit policy; `wb_bot_jump_sanity`; мера jump success, energy.

**E4. Cloak на фланг.**
Хороший игрок: cloak только против скрытой цели/на фланг, не в упор, не под огнём.
B/E: Beginner нет; Elite да.
Сейчас: **частично** — `BotSuitPolicy::StealthOk` 12–35 m, hidden target; `wb_bot_cloak_experimental`; difficulty
Elite sets `cloakApproach`, but `BotSuit::Apply` still rejects without experimental (DS-024 bug). Мера: cloak
activation, damage while cloaked, flank kills.

**E5. Гранаты.**
Хороший игрок: не кидает в союзника, кидает в укрытие/на точку, не стоит в своей гранате.
B/E: Beginner редко; Elite.
Сейчас: **нет** — в `Code/CryGame/Bots` нет логики гранат; оружие GrenadeLauncher есть в XML. Мера: friendly damage,
grenade kills, accidents.

## F. Power Struggle (D-018/D-019)

**F1. Команды 3–4 на команду.**
Хороший игрок: играет за tan/black, помогает своим.
B/E: нет разницы; Elite лучше.
Сейчас: **нет** — `IBotGameMode` только в спецификации `T0_GAMEMODE.md`; `BotMatch` IA-only, `SetTeam` нет. Мера:
`GetTeam`, team spawn, score.

**F2. Захват точки.**
Хороший игрок: стоит в зоне, не выходит на 90 %, защищает.
B/E: Beginner; Elite.
Сейчас: **нет** — capture цели нет, данные в `PS_MAPS.md`; мера `GetTeamId` after capture, contested.

**F3. Покупка SCAR.**
Хороший игрок: копит 150 PP, покупает в своём buy zone, не тратит на мусор.
B/E: Beginner; Elite.
Сейчас: **нет** — `PS_MIN.md`; мера PP spend, inventory SCAR, `BuyItem`.

**F4. Машина: вход/выход.**
Хороший игрок: входит водителем/пассажиром, едет по дороге, выходит у точки.
B/E: Beginner; Elite.
Сейчас: **нет** — `VEHICLES.md`; вход через `IVehicleSeat::Enter`, вождение `IVehicle::OnAction`. Мера:
`GetLinkedVehicle`, seat, 50 m drive, exit.

**F5. Машины off.**
Хороший игрок: играет пешком, если техника запрещена.
B/E: без разницы.
Сейчас: **нет** — `wb_ps_vehicles` в спецификации; мера: no vehicle goal/buy/enter.

## G. Смерть и возрождение

**G1. Смерть/5 s respawn.**
Хороший игрок: ждёт respawn, выбирает спавн/лоадаут, не бежит в тот же угол.
B/E: Beginner; Elite.
Сейчас: **есть** — BotInstance revive guard, 5 s respawn, `RevivePlayer`; нет выбора спавна по угрозе. Мера:
death→revive, same-spot death rate.

## Сводная таблица (IA priority)

| # | Ситуация | Приоритет IA | Статус | Тест |
| --- | --- | --- | --- | --- |
| A1 | спавн с пистолетом | высокий | частично | время до primary |
| A2 | выбор оружия | высокий | есть/flag | `WEAPON RANK` |
| A3 | reload | высокий | частично | `TACTICAL RELOAD` |
| A4 | добивание | высокий | частично/flag | switch-away |
| A5 | лут | средний | частично/flag | pickup after kill |
| B1 | маршрут | высокий | есть | stalls |
| B2 | крыши | высокий | частично | `VANTAGE`, kills z+3 |
| B3 | Strength прыжок | высокий | частично | jump success |
| B4 | wall jump | средний | нет | probe |
| B5 | bhop | средний | частично | speed on straight |
| B6 | двери/лифты | средний | частично | stalls |
| C1 | 5 м | высокий | частично | hits<10 m |
| C2 | 20 м | высокий | частично | TTK |
| C3 | 50 м | высокий | частично | hits>40 m |
| C4 | 80 м | средний | частично | sniper kills |
| C5 | укрытие | высокий | частично | blocked shots |
| C6 | крыша/снайпер | высокий | частично/нет | `GLANCE` |
| C7 | тебя снайпер | высокий | частично | deaths by sniper |
| D1 | низкое HP | высокий | частично | deaths<30 HP |
| D2 | нет патронов | высокий | частично | empty shots |
| D3 | несколько врагов | высокий | частично | deaths 2+ |
| D4 | слышал выстрелы | средний | есть/flag | `HEARD` |
| D5 | cloak | средний | частично | detection |
| D6 | почти мёртв | высокий | частично | switch-away |
| D7 | после kill | средний | частично | death<3 s |
| E1 | Armor | высокий | есть | damage taken |
| E2 | Speed | средний | есть | travel speed |
| E3 | Strength | высокий | есть | jump success |
| E4 | Cloak | средний | частично | cloak kills |
| E5 | гранаты | средний | нет | friendly damage |
| F1 | команды | PS high | нет | `GetTeam` |
| F2 | захват | PS high | нет | capture |
| F3 | buy SCAR | PS high | нет | PP/SCAR |
| F4 | vehicle | PS high | нет | seat/drive |
| F5 | vehicles off | PS high | нет | no enter |

## 10 главных gaps для «похож на человека»

1. **Крыши/снайпер:** нет взгляда вверх; `threat_scan` 0 glances в C-098; DS-015.
2. **Дальний бой:** fireRange 40 у большинства, Elite 58; hits>40 редки; DS-015.
3. **Добивание:** finish green C-093, но switch-away 407; DS-015/DS-018.
4. **Укрытие/отступление:** нет `ArbiterGoal::Retreat`, withdraw только по HP; `SELF_PRESERVATION.md`.
5. **Гранаты:** логики нет; friendly damage/accidents.
6. **Cloak Elite:** `cloakApproach` не проходит `BotSuit::Apply` без `wb_bot_cloak_experimental`; DS-024.
7. **Передвижение Beginner:** no-strafe fallback идёт вперёд 4 м, а не стоит; DS-024.
8. **PS T0:** нет `IBotGameMode`/team spawn/registration; `T0_GAMEMODE.md`.
9. **PS capture/buy:** нет capture/buy целей; `PS_MIN.md`.
10. **PS vehicles:** нет входа/выхода/вождения; `VEHICLES.md`.

## Что мерить всегда

`stalls`, `TRAP RECOVERY` clusters, kills/deaths, hit distance p50/p90, hits>40 m, headshot share, `TARGET CHOICE`
switch-away, `INTENT WHY`/`LOOK TRACE` (почему), `TACTICAL RELOAD`, `VANTAGE`/`GLANCE`, `GetLinkedVehicle`, capture
team change, PP/buy. Один прогон не доказывает поведение: нужен парный прогон или probe.
