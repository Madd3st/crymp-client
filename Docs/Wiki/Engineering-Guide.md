# Маршрут инженера

## 1. Зафиксируй, что изучаешь

Сначала `git status --short`, текущая ветка и SHA. Игровой код — `warsbots/feature/claude-sprint`; отчёты — `weapon-goals`; mailbox и вики — `steelmill-triage`. Не переключай общий checkout поверх чужих правок. Принятый C153 IA — `5ef9ee22`; C149 Shore и замороженные пакеты — `b8b444d6`. Новый C154 `3cfd8596` проходит отдельные CI и приёмку. Ссылки в таблице ниже сохраняют исторический срез b8; для новой задачи подставь её точный SHA.

Для чтения другого состояния без переключения:

```powershell
git fetch origin '+refs/heads/warsbots/*:refs/remotes/origin/warsbots/*'
git show origin/warsbots/feature/claude-sprint:Code/CryGame/Bots/BotBrain.cpp
```

Если задача привязана к SHA, используй его вместо имени ветки. Читай [PROTOCOL.md](https://github.com/Madd3st/crymp-client/blob/warsbots/research/steelmill-triage/Docs/Research/Coordination/MAILBOX/PROTOCOL.md) и актуальную задачу, затем ищи `rg` по симптому; не начинай со всего SDK.

## 2. Найди слой ответственности

| Что нужно понять | Проверенная точка входа на b8b444d6 |
| --- | --- |
| Жизненный цикл и команды | [BotManager.cpp](https://github.com/Madd3st/crymp-client/blob/b8b444d6c858f27a1fdde0d93b25a318cafc6937/Code/CryGame/Bots/BotManager.cpp), [BotInstance.cpp](https://github.com/Madd3st/crymp-client/blob/b8b444d6c858f27a1fdde0d93b25a318cafc6937/Code/CryGame/Bots/BotInstance.cpp) |
| Решения и связь подсистем | [BotBrain.cpp](https://github.com/Madd3st/crymp-client/blob/b8b444d6c858f27a1fdde0d93b25a318cafc6937/Code/CryGame/Bots/BotBrain.cpp), [BotBlackboard.h](https://github.com/Madd3st/crymp-client/blob/b8b444d6c858f27a1fdde0d93b25a318cafc6937/Code/CryGame/Bots/BotBlackboard.h) |
| Разделение режимов игры | [BotGameMode.h](https://github.com/Madd3st/crymp-client/blob/b8b444d6c858f27a1fdde0d93b25a318cafc6937/Code/CryGame/Bots/BotGameMode.h), [BotMatch.cpp](https://github.com/Madd3st/crymp-client/blob/b8b444d6c858f27a1fdde0d93b25a318cafc6937/Code/CryGame/Bots/BotMatch.cpp) |
| Движение и ввод | [BotMovement.cpp](https://github.com/Madd3st/crymp-client/blob/b8b444d6c858f27a1fdde0d93b25a318cafc6937/Code/CryGame/Bots/BotMovement.cpp), [BotLocomotion.cpp](https://github.com/Madd3st/crymp-client/blob/b8b444d6c858f27a1fdde0d93b25a318cafc6937/Code/CryGame/Bots/BotLocomotion.cpp) |
| Запросы пути и вода | [BotNavMesh.cpp](https://github.com/Madd3st/crymp-client/blob/b8b444d6c858f27a1fdde0d93b25a318cafc6937/Code/CryGame/Bots/BotNavMesh.cpp), [BotNavMeshCore.cpp](https://github.com/Madd3st/crymp-client/blob/b8b444d6c858f27a1fdde0d93b25a318cafc6937/Code/CryGame/Bots/BotNavMeshCore.cpp), [BotWaterPolicy.cpp](https://github.com/Madd3st/crymp-client/blob/b8b444d6c858f27a1fdde0d93b25a318cafc6937/Code/CryGame/Bots/BotWaterPolicy.cpp) |
| Параметры и формат данных | [BotConfig.cpp](https://github.com/Madd3st/crymp-client/blob/b8b444d6c858f27a1fdde0d93b25a318cafc6937/Code/CryGame/Bots/BotConfig.cpp), [BotDataPath.h](https://github.com/Madd3st/crymp-client/blob/b8b444d6c858f27a1fdde0d93b25a318cafc6937/Code/CryGame/Bots/BotDataPath.h) |
| Что проверяет CI | [warsbots-ci.yml](https://github.com/Madd3st/crymp-client/blob/b8b444d6c858f27a1fdde0d93b25a318cafc6937/.github/workflows/warsbots-ci.yml) |

Например, «бот плывёт, но путь не находится» требует сопоставить фактическое положение, классификацию воды, начало/цель запроса Detour и результат запроса. Исправлять скорость плавания до такого анализа преждевременно.

## 3. Сначала воспроизведение

Укажи ожидаемое и фактическое поведение, входные данные, точный SHA, пресет и хеши карты. Найди проверку, которая ловит дефект на старом коде. Для навигации предпочтительны реальные запросы NavMeshCore на той же геометрии; синтетическая политика сама по себе не подтверждает доступность пути.

Дальше — минимальная связная пачка, дешёвые проверки и независимое ревью. CI собирает клиент и сервер MSVC x64. QA на ПК владельца скачивает точную зелёную сборку; локальную сборку игры для этой работы не запускает. Рецепт и актуальное ревью сверяются снова непосредственно перед игрой.

## 4. Границы интеграции

Бот — принадлежащий менеджеру серверный актёр, без фиктивного INetChannel. Сохраняй проверки EntityId/идентичности, очистку уровня и штатные оружие/урон. Общие Player/GameRules/сеть требуют узкой интеграции только для собственных ботов и согласованной пары клиента/сервера. Геометрические исправления конкретной карты оформляются данными, а не координатными исключениями в поведении.

Новый флаг требует критерия выхода; действуют правила net-zero из протокола. Наличие экспериментального кода не разрешает менять значения по умолчанию. Тестовый teleport не является доказательством навигации.

## 5. Что сдавать

Коммит кода, результат проверок, независимое ревью, CI SHA/run, рецепт и эффективные CVar, хеши данных, сырые логи/демо, краткий вердикт и ограничения. Отдельно подтвердить архивирование и восстановление данных. [Проверки и доказательства](Testing-and-Evidence) содержит образец принятого сценария.

Для изменений Markdown достаточно проверить diff, ссылки и актуальность фактов; DLL пересобирать не нужно. Исходники вики лежат в `Docs/Wiki` на triage и публикуются в отдельный Git-репозиторий `crymp-client.wiki.git`. После публикации фиксируй оба SHA.
