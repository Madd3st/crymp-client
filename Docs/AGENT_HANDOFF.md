# Handoff: CryMP migration

Read STATUS and WARSBOTS_PORT first. Source WarsBots snapshot `2da4bf1` is clean and
preserved in its original repository; the new primary development repo is this fork.
Master matches upstream `eb13a69`; warsbots/main starts there. Work is on
`warsbots/feature/locomotion` after `1fe4be11` MVP; only the integrator edited shared files.

Two read-only agents audited engine integration and movement/CryAI respectively;
no competing file edits or additional worktrees. Main integrator ported, compiled
and ran tests. PR360 is merged upstream: AI.RegisterWithAI is already enabled;
this does not establish navigation data or working CryAI behavior on IA maps.

Important integration lessons:

1. `Base` compiles CryMP Server/Client code that allocates CGame. Experimental
   class-layout defines MUST propagate from Base PUBLIC to CryGame and launchers.
   CryGame-only PUBLIC caused a map-load crash in CryAction offset0x2db517.
   /MAP resolved the caller to GameFramework::StartGameContext; fixed build runs.
2. Never access GameObject/GetChannelId from early SerializeSpawnInfo. Preserve
   native teamId; initialize suit after Init/Revive. Match both experimental EXEs.
3. No new address patches; native ActorSystem channel0/BindToNetwork is retained.
   CryMP channel0 IsChannelOnHold is already safe; do not reintroduce the Wars fix.
4. OFF is a real upstream protocol build, ON changes bot spawn/motion serialization.
   The `build/baseline` cache was reused and now holds ON; use the build script flags.

The user is testing the launched CryMP client and server; do not terminate their
session or replace locked EXEs without agreement. Process IDs are local build/*.txt,
but verify actual process identity and whether the player is still connected.
The first client user confirmation covered visibility/movement/shooting. Server
confirmed death/respawn of the same actor and genuine weapon damage in both directions.

Strafe intent/body-look replication and native Speed selection are implemented;
read STATUS for measured repeat trials and unproven chase/client cases.
Latest user steering prioritizes proper IA participants and the scoreboard.
Keep Strength clearance and native energy mechanics.
Investigate headshot AI-vs-player damage parity separately; no health/multiplier change
has been made. Do not claim the entire long-term request complete.

## Передача в основной чат: сохранение по лимиту

Основной проект теперь Madd3st/crymp-client, локально local/crymp-client внутри
старого WarsBots checkout. Рабочая main: caf1a815 (Combat MVP + locomotion),
сохранённая WIP-ветка: warsbots/feature/ia-participants. Старый WarsBots не удалён.

Успели: чистая MSVC x64 Release сборка CryMP; порт актуального Wars AI; реальный
сервер и клиент; видимый бот, стрельба/урон/убийство и native respawn того же EntityId.
Пользователь подтвердил движение и бой, затем левый/правый стрейф. В открытой точке
физически: strafe left2/2 right2/2, sprint2/2 (5.46м/с, расход энергии), Strength2/2
(vz9.60, подъём3.62/3.63, штатное приземление), normal1/1. Стартовый закрытый spawn
не прошёл правый стрейф: local avoidance отвёл движение. Late одиночный клиент
получил replica; два одновременных клиента НЕ подтверждены.

Последняя просьба пользователя — показывать бота в таблице и считать участником IA.
Подготовлены BotMatch/participant API, HUD и Lua wrappers, но сборка WIP упала на
BotMatch.cpp41/46 (Script::Call C2660). Эта версия не установлена/не запущена.
Продолжить с исправления реального script call API, затем собрать и проверить IA.
Не менять глобальный IsPlayer/каналы/античит. Не объявлять scoreboard готовым.

Остановились по просьбе пользователя из-за лимита. Рабочий клиент/сервер оставлены
пользователю (последние PID31740/14960, проверить перед любыми действиями).
Запуск из build/baseline с -dir оригинальной игры; root EXE могли остаться старее.
Не убивать игру и не заменять EXE без нового согласования. Никаких новых тестов
после команды сохранить не проводилось. Полные native логи локальны и не в Git.
