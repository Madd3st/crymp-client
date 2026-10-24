# Ревью Codex `994aeab` — nav knee-ray A/B (против C-011)

Читатель: OpenCode (read-only). База сравнения `fb578d52`; ветка `warsbots/feature/weapon-goals`.
Вердикт: **соответствует C-011, OFF byte-identical; принять с 3 замечаниями (ни одно не блокер)**.

## Что делает

Изолирует эксперимент с «коленным» лучом за export-only гейтом `wb_nav_knee_fix` (default 0).
Новый файл `BotKneePolicy.h` с двумя чистыми функциями, `BotManager.cpp/.h` (cvar + команда
`wb_nav_audit`), `BotNav.cpp/.h` (`StartAudit`, `AuditOnly`, вариация в `Walkable`, audit-экспорт),
тест `Tests/BotKneePolicyTests.cpp`.

## Проверка по C-011

| Требование C-011 | Статус | Где |
| --- | --- | --- |
| новый default-OFF `wb_nav_knee_fix` | ✅ | `BotManager.cpp` регистрация `g_navKneeFix=0`, cvar default 0 |
| variant A = knee ray h=0.8 | ✅ | `BotKneePolicy.h` `KneeRayHeight(1)==.8f` |
| variant B = keep 0.5, принять 0.5-hit если step ≤ 0.75 | ✅ | `KneeHitIsStep(2,...)` + ветка в `Walkable` |
| export + Triage only, **нет bot-runs** | ✅ | `Add` отказ (`:312`), `FindPath` → `audit_only` (`:274`), `PickHotspot=false` (`:427`), `Save` пропуск (`:512`) |
| `wb_nav_export` на OFF/A/B | ✅ | audit-суффикс `.audit-<variant>.nav.json` (`BotNav.cpp:347`) |
| лог имён поверхностей 140/198/193/143 | ✅ | `StartAudit` печатает `NAV SURFACE id= name=` (`BotNav.cpp:57-62`) |
| **OFF byte-identical** | ✅ | `KneeRayHeight(0)==.5f` → цикл `{.5f,1.2f,1.75f}` равен старому; ветка B гейтится `m_kneeVariant==2` |

## Замечания (bugs/risks, file:line)

1. **Возможный null-deref в логе поверхностей** — `BotNav.cpp:59-61`:
   `gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager()->GetSurfaceType(id)`.
   Внешний guard проверяет `p3DEngine` и `GetMaterialManager()`, но **не** `GetSurfaceTypeManager()`.
   Если последний вернёт nullptr — разыменование. Низкий риск (audit-only, движок всегда имеет
   surface-type manager), но стоит добавить guard.
2. **Эвристика «тот же коллайдер = ступень» в variant B** — `BotNav.cpp:133-142`:
   `step = ground.pCollider == hit.pCollider && KneeHitIsStep(...)`. Стена, которая в статике разделяет
   коллайдер с полом (цельный статический объект/интерьер), при h=0.5 даст совпадение `pCollider` и
   может быть ложно принята за «ступень» (false-negative → лишнее ребро в экспорте B). Искажает
   сравнение вариантов, но не трогает runtime (export-only).
3. **`wb_nav_audit <n>` молча игнорируется** — `BotManager.cpp:821-830`: вариант берётся из cvar
   `g_navKneeFix`, а не из аргумента; `wb_nav_audit 1` (argCount=2, не "stop") попадает в
   `if(args->GetArgCount()!=1)return;` и ничего не делает. Help-текст неоднозначен
   («using knee variant»). Путаница оператора, не баг выполнения.

## Прочее

- Тест `BotKneePolicyTests.cpp` покрывает только чистые функции; интеграция variant B в `Walkable`
  (t-кламп, сдвиг `inside` на 5 см, ground-ray) тестами не покрыта. Средний риск.
- `OnPostUpdate` в audit-режиме: `Begin()+Step(2.f)` и `return` — сборка графа идёт по 2 мс/кадр,
  без ботов. Корректно, но медленно для большого уровня (приемлемо для export-only).

## Вердикт

Принять. Код корректно изолирован; OFF не меняет поведение ни на байт. Перед «промоушном» варианта
A/B в боевой граф — закрыть замечание 2 (ложные ступени) эмпирически по экспортам A vs B.
