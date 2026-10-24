# Ревью Codex `3bda447` — post-revive guard (против C-014 п.2)

Читатель: OpenCode (read-only). База `fb578d52`; ветка `warsbots/feature/weapon-goals`.
Вердикт: **соответствует C-014 п.2 (max 2 retries, без kill/score), принять с 3 замечаниями, из них
1 — проверить эмпирически (re-revive живого актора).**

## Что делает

Новый header-only `BotReviveGuard` (стейт-машина 2 c / 0.3 м / max 2 retries), `BotInstance::DoRevive`
с флагом `guardRetry`, `CheckReviveGuard` в `Update`, список `m_guardFailedSpawns` для выбора другого
спавна, cvar `wb_revive_guard` (default 1). `BotBrain::RequestsMovement()` = `m_running && m_intent.move`.

## Проверка по C-014 п.2

| Требование | Статус | Где |
| --- | --- | --- |
| 2 с после revive: moved < 0.3 м при запросе движения → re-revive | ✅ | `BotReviveGuard::Check` `now-m_since<2.f → Waiting`; `moved<.3f && (requesting \|\| badBody) → Retry` |
| groundHeight invalid / bStuck | ✅ | `CheckReviveGuard` `bad=living.bStuck \|\| !isfinite(groundHeight) \|\| groundHeight<-1e8` (`BotInstance.cpp`) |
| другой безопасный спавн | ✅ | `ChooseSpawnLocation(..., &m_guardFailedSpawns)` исключает 195 и уже провалившиеся (`BotManager.cpp:277`) |
| без self-kill, без смены счёта | ✅ (по коду) | `DoRevive` → `rules->RevivePlayer(...)` (не `KillPlayer`); лог `nativeKill=0`; `EquipLikePlayer` пропущен при retry |
| лог `REVIVE GUARD actor= spawn= moved= retry=` | ✅ | `WB_LOG(0, "REVIVE GUARD actor=%u spawn=%u moved=%.3f retry=%u ...")` (superset) |
| max 2 retries | ✅ | `BotReviveGuard::Check` `if(m_retries>=2)return Exhausted` |

## Замечания (bugs/risks, file:line)

1. **`RevivePlayer` на живом акторе — проверить эмпирически.** Guard re-revive срабатывает, когда бот
   **жив, но застрял** (`guardRetry && actor->GetHealth()>0` разрешено, `<=0` отказано). Значит
   `rules->RevivePlayer(actor, position, angles, 0, !guardRetry)` вызывается на живом акторе —
   предполагается «same-actor respawn» без kill (в истории репо он подтверждён только **после**
   self-kill, не в живом состоянии). 5-й аргумент `!guardRetry` (false при retry) — семантика не видна
   из diff (`RevivePlayer` сигнатура не показана); если это `resetScore`, то `false` корректно
   сохраняет счёт, но это надо подтвердить по сигнатуре. **Главный риск — убедиться, что живой актор
   реально переезжает на новый спавн без смены счёта и без kill.** (`BotInstance.cpp` `DoRevive`,
   строка `RevivePlayer(actor, position, angles, 0, !guardRetry)`.)
2. **Retry-оживление не экипирует бота** — `if(!guardRetry)BotMatch::EquipLikePlayer(...)`
   (`BotInstance.cpp`). После guard re-revive бот остаётся с прежним/пустым снаряжением и пойдёт
   SEEKING_WEAPON. Осознанно (чтобы не задваивать экипировку/счёт), но это видимый побочный эффект.
3. **`forcedZOffset=.2f` на retry не виден в позиции** — `DoRevive(0,.2f,true)`: z-offset передаётся,
   но в diff не показано его применение к `position` (неизменённая часть). Убедиться, что `.2f`
   реально прибавляется, иначе аргумент мёртвый.
4. **Интеграция не покрыта тестами** — `Tests/BotReviveGuardTests.cpp` тестирует только чистую
   `BotReviveGuard` (тайминги/счётчик), но не `CheckReviveGuard`: вызов `RevivePlayer`, `Stop/Start`
   мозга, `m_guardFailedSpawns`, ветка `!revived → Begin(now,false)`. Средний риск (логика ветвления
   частично не проверена).

## Примечание

`m_reviveGuard.Begin(now,false)` при неудачном retry повторно взводит guard **не сбрасывая** retries —
это даёт «at most 2 attempts даже без свободной точки», что соответствует «max 2 retries». Логика
счётчика корректна (`Begin(newLife=true)` сбрасывает).

## Вердикт

Принять при условии, что п.1 (re-revive живого актора через `RevivePlayer`) подтверждён на
сервер-логе/QA-прогоне C-014 (0 frozen revives, 0 kills, счёт не изменился — что C-017 уже и показал).
