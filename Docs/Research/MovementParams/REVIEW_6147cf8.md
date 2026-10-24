# Ревью Codex `6147cf8` — исключение spawn 195 (против C-014 п.1)

Читатель: OpenCode (read-only). База `fb578d52`; ветка `warsbots/feature/weapon-goals`.
Вердикт: **соответствует C-014 п.1, принять (2 замечания)**.

## Что делает

`ChooseSpawnLocation` пропускает spawn `EntityId 195` при `wb_spawn_exclude195==1` (default 1),
логирует один раз за уровень `SPAWN EXCLUDE id=195 reason=revive_freeze`; cvar в `kVariables[]`.
`m_spawnExcludeLogged` сбрасывается в `OnLoadingComplete`.

## Проверка по C-014 п.1

| Требование | Статус | Где |
| --- | --- | --- |
| bot spawn choice исключает 195 | ✅ | `BotManager.cpp:271-275` (`candidate==195` → `continue`) |
| лог один раз `SPAWN EXCLUDE id=195 reason=revive_freeze` | ✅ | `BotManager.cpp:272-273`; сброс в `OnLoadingComplete` (`:438`) |
| люди не затронуты | ✅ | `ChooseSpawnLocation` вызывается только из `BotInstance::Spawn`/`Revive` (бот-путь); человеческий спавн идёт нативной логикой |

## Замечания (bugs/risks, file:line)

1. **Хардкод `195` + одиночный cvar** — `BotManager.cpp:272` (`candidate==195`) и
   `wb_spawn_exclude195`. C-014 говорил «короткий список исключений»; сейчас список ровно из одного
   id, масштабируется плохо. Если добавится ещё один «замерзающий» спавн — понадобится рефактор в
   `std::vector<EntityId>`/набор. Не блокер для текущего воркэраунда.
2. **`mutable bool m_spawnExcludeLogged`** — `BotManager.h:130`, mutable ради записи из
   `const ChooseSpawnLocation`. Работает, но это «скрытое состояние» в const-методе (отмечено как
   R6/R7 в `REFACTOR_AUDIT.md`). Низкий риск.

## Примечание по default ON

`wb_spawn_exclude195` default `1` — соответствует решению владельца D-005 и C-017 («flip defaults
back ON»). В текущем коммите значение уже ON; проверять, что нет более позднего коммита
(1452fc5), который временно откатил default под soak-прогон.

## Вердикт

Принять. Минимальный и безопасный воркэраунд, людей не трогает.
