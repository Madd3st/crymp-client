# WarsBots: обязательный рефактор, волна 1 (для Codex C-027)

Источник: `Madd3st/crymp-client`, ветка `warsbots/research/movement-params`; цель — weapon-goals HEAD.
Только чтение кода. Требование: **поведение-идентичные** перемещения, никаких изменений рантайма.

## 1. Инвентарь `wb_*` cvar (регистрация + чтения + hot-path)

Регистрация — `BotManager::Initialize` (`BotManager.cpp:96-113`), список `kVariables[]` (`:76`). Каждый
cvar зарегистрирован с бэкинг-глобалом `&g_xxx`/`&m_xxx`, **но горячие пути читают строковым
`GetCVar("wb_...")`** — это и есть R3-антипаттерн (C-010). Цель — заменить на поля `BotConfig`.

| cvar | бэкинг | регистрация | чтения (file:line) | per-tick `GetCVar`? | → `BotConfig` поле |
| --- | --- | --- | --- | --- | --- |
| `wb_probe_local_test` | `m_localTestEnabled` | `:96` | через член `LocalIAGate` | нет (член) | `gateOpen` |
| `wb_bot_cloak_experimental` | `m_cloakExperimental` | `:98` | `BotSuit.cpp:10` (`CloakEnabled`) | **да** (каждый `BotSuit::Apply`) | `cloakExperimental` |
| `wb_hotspot_all_nodes` | `g_hotspotAllNodes` | `:99` | `BotNav.cpp:402` (`PickHotspot`) | **да** (10 Гц/бот) | `hotspotAllNodes` |
| `wb_sink_escape` | `g_sinkEscape` | `:100` | `BotBrain.cpp:584` (`RecoverTrap`), `BotNav.cpp:444` (`InClosedComponent`) | **да** (каждый тик) | `sinkEscape` (или удалить, см. §3) |
| `wb_reachable_goals` | `g_reachableGoals` | `:101` | `BotNav.cpp:420,443` (`GoalFilterEnabled`/`KnownUnreachable`) | **да** (поиск оружия) | `reachableGoals` (или удалить) |
| `wb_revive_z_test` | `g_reviveZTest` | `:102` | `BotManager.cpp:756-762` (`ReviveAtCommand`, ручной debug) | нет (ручной) | удалить (§3) |
| `wb_revive_hooks_test` | `g_reviveHooksTest` | `:103` | `BotInstance.cpp:288` (`DoRevive`), `BotManager.cpp:763-767` | per-revive | удалить (§3) |
| `wb_physics_diagnostics` | `g_physicsDiagnostics` | `:104` | `BotInstance.cpp:193` (`Update`) | **да** (каждый Update) | `physicsDiagnostics` |
| `wb_trap_recovery` | `g_trapRecovery` | `:105` | `BotBrain.cpp:579` (`RecoverTrap`) | **да** (каждый тик) | `trapRecovery` |
| `wb_progress_experiment` | `g_progressExperiment` | `:106` | `BotBrain.cpp:565` (`NoProgress`), `BotManager.cpp:490` (`Status`) | **да** (при движении) | удалить (§3) |
| `wb_qa_socom_only` | `g_qaSocomOnly` | `:108` | `BotBrain.cpp:287` (`Update`, каждый тик решения) | **да** | `qaSocomOnly` |
| `wb_fire_diagnostics` | `g_fireDiagnostics` | `:109` | читается как `g_fireBudget`/`g_fireDiagnostics` в `BotManager` (проверить `BotCombat` shot-path) | нет строкового `GetCVar` в grep | `fireDiagnostics` |
| `wb_log` | `m_logLevel` | `:110` | `BotLog.cpp:42` (**каждый** `WB_LOG`!) | **да — самый горячий** | `logLevel` (§4) |
| `wb_maxbots` / `wb_skill` | `m_maxBots`/`m_skill` | `:111-112` | через члены | нет | уже члены |
| `wb_move_trace` | `g_moveTrace` | (dae506db) | внутри `MoveTrace::Update` (`BotManager.cpp:414` цепочка) | по кадру | `moveTrace` |
| `wb_nav_knee_fix` | `g_navKneeFix` | (994aeab) | `BotManager` `NavAuditCommand` → `StartAudit(g_navKneeFix)` (латчится) | нет (audit-only) | `navKneeFix` |
| `wb_spawn_exclude195` | `g_spawnExclude195` | (6147cf8) | `ChooseSpawnLocation` читает глобал напрямую (тот же файл) | per-spawn (не строковый) | `spawnExclude195` |
| `wb_revive_guard` | `g_reviveGuard` | (3bda447) | `BotInstance::DoRevive`/`CheckReviveGuard` — `GetCVar("wb_revive_guard")` | per-revive | `reviveGuard` |
| `wb_bot_pace` | `g_botPace` | (ef25395e) | `BotPace.h` `BotPaceEnabled()` = `GetCVar("wb_bot_pace")`; зовётся в `Player.cpp:4972,5014`, `PlayerMovement.cpp:1072`, `BotLocomotion::Execute` (10 Гц), `BotSuit::Apply`, `BotBrain::TravelGait/ExecuteIntent` | **да — самый широкий** | `botPace` |

**Правило переноса**: все `GetCVar("wb_*")` → поле `BotConfig` (struct в `BotConfig.h`), заполняемое один
раз в `BotManager::Initialize` (из `g_*`/`m_*` значений) и обновляемое в `OnPostUpdate` (или через
`Register(..., onChange)`); hot-пути читают `BotConfig::Get().<field>`. Строковые `GetCVar` уходят из
`SetWarsBotMotion`/`BotLocomotion::Execute`/`BotSuit::Apply`/`BotLog::Log` — это основной выигрыш.

## 2. Разбиение `BotBrain::Update` (fb578d52 `BotBrain.cpp:202-561`)

Текущая функция — 360 строк (C-010 R1). Разбиение по состояниям, с сохранением порядка `return`-веток:

| Метод | Строки | Что делает | Куда уходят локальные |
| --- | --- | --- | --- |
| `UpdateCommon` | `:204-210` | running-гейт, dt-кламп, `m_now`, `Sample`, `Observe` | — |
| `UpdateTelemetry` | `:212-243` | verbose-телеметрия | — |
| `UpdateDead` | `:244-268,273` | health<=0 → `DEAD`/`RESPAWNING` | `m_deadTime` (уже член) |
| `UpdateTest` | `:277-285` | тестовые команды (`m_testUntil`) | — |
| `UpdateWeaponSeek` | `:287-329` | `!SelectAvailable` → `SEEKING_WEAPON`/`MOVING_TO_WEAPON` | `qaWeapon`/`socomQA` → параметр/`BotConfig.qaSocomOnly` |
| `UpdateUpgrade` | `:331-376` | `FindEnemy` + апгрейд оружия (`MOVING_TO_WEAPON`) | `enemy` (IActor*) → возврат из `FindEnemy` |
| `UpdateRoam` | `:377-429` | `!enemy` → `SEARCHING_ENEMY` (патруль) | `feet` → локальный |
| `UpdateCombat` | `:431-550` | враг есть → `ATTACKING`/`CHASING_ENEMY` | см. ниже |
| `UpdatePost` | `:551-561` | `ExecuteIntent` + `NoProgress` | — |

`UpdateCombat` дополнительно делится:
- `:431-436` видимость/`destination`/`distance` → `visible`, `destination`, `distance` (возврат/члены).
- `:437-448` трекинг aim + шум → `aim`, `error` (локальные, из `m_trackedAim`).
- `:449-459` `withdraw`-триггер → `desiredRange`, `disadvantage` (локальные).
- `:460-469` `withdraw`-исполнение.
- `:470-519` `ATTACKING` (retreat/strafe/fire) → `travel`, `toward`, `lateral`, `m_strafeSide` (члены).
- `:520-550` `CHASING_ENEMY` (fire-while-closing, blocked-patience, `TravelGait`, cloak).

**Локальные, которые должны стать членами/параметрами**: `enemy`, `visible`, `destination`, `distance`,
`aim`, `held`, `desiredRange`, `disadvantage`, `socomQA`, `decisionTime`. Уже члены: `m_trackedAim`,
`m_trackedTarget`, `m_trackedSelection`, `m_combat` (весь боевой стейт), `m_chaseBlockedSince`,
`m_shotBlockedSince`, `m_retreating`, `m_strafeSide`, `m_nextStrafe`.

**Порядок вызовов (обязательно сохранить)**: `UpdateCommon → UpdateDead → UpdateTest → UpdateWeaponSeek →
FindEnemy → UpdateUpgrade → UpdateRoam → UpdateCombat → UpdatePost`. Каждая ветка завершается `return`
(обработала тик) — при переносе в под-методы `return` заменяется `return` из `Update` (или bool
«handled»), не меняя семантику.

## 3. «Мёртвые» флаги к удалению (проверка)

| Флаг | Чтения (кроме регистрации) | QA-скрипт использует | Вердикт |
| --- | --- | --- | --- |
| `wb_revive_z_test` | `BotManager.cpp:756-762` (ручной `wb_revive_at` debug) | **`Tools/Test-WarsBotsRevive.ps1:37`** | не удалять без снятия скрипта; иначе — удалить с `ReviveAtCommand`-веткой z-test |
| `wb_revive_hooks_test` | `BotInstance.cpp:288` (`DoRevive` UpdateAreas), `BotManager.cpp:763-767` | **`Tools/Test-WarsBotsRevive.ps1:37`** | то же |
| `wb_sink_escape` | `BotBrain.cpp:584` (`RecoverTrap`), `BotNav.cpp:444` (`InClosedComponent`) | **`Tools/Test-WarsBotsTactics.ps1:97`** (`CandidateSinkEscape`) | эксперимент C-003 закрыт (keep OFF); удалить вместе с `RecoverTrap`-sink-веткой и `InClosedComponent`, **но** скрипт завязан — снять флаг из скрипта первым |
| `wb_progress_experiment` | `BotBrain.cpp:565` (`NoProgress`), `BotManager.cpp:490` (`Status`) | **`Tools/Test-WarsBotsTactics.ps1:104`** (`$progress`) | удалить с `NoProgress`-гейтом; скрипт — снять `$progress` |

**Вывод**: ни один не «чисто мёртвый» — все завязаны на QA-скрипты (`Tools/Test-WarsBotsTactics.ps1`,
`Tools/Test-WarsBotsRevive.ps1`). Порядок удаления: (1) снять флаги из скриптов, (2) удалить cvar +
ветку. `wb_sink_escape`/`wb_reachable_goals`/`wb_hotspot_all_nodes`/`wb_nav_knee_fix` — по плану
Detour-миграции (`DETOUR_INTEGRATION.md` §5) станут мёртвыми целиком, сейчас не трогать.

## 4. Логи → хелпер `BotEvent` (формат Triage не менять)

`WB_LOG(level, ...)` → `WarsBots::Log` (`BotLog.h:11`, `BotLog.cpp:33-53`). Внутри `Log` — **строковый
`GetCVar("wb_log")` на каждый вызов** (`BotLog.cpp:42`) — самый горячий `GetCVar` во всём коде (каждая
строка лога). Хелпер:

```cpp
// BotEvent.h — единая точка логирования; формат сообщений FIXED (Triage разбирает "ключ=значение").
namespace WarsBots {
void Event(int level, const char* kind, const char* fmt, ...);   // печатает "[WarsBots] <kind> <fmt>"
}
```

Переносимые вызовы (таблица kind → текущий site, **текст/порядок полей НЕ менять**):

| kind | текущий `WB_LOG` site | формат (сохранить) |
| --- | --- | --- |
| `AI state=` | `BotBrain.cpp:44` (`Change`) | `AI state=%s` |
| `BOT ACTION:` | `BotLocomotion.cpp:112` (jump), `Player.cpp:4954` (sprint) | `BOT ACTION: JUMP/SPRINT ...` |
| `JUMP TAKEOFF/LANDING` | `BotLocomotion.cpp:41,49` | `JUMP TAKEOFF/LANDING: strength= ...` |
| `REAL PICKUP` | `BotWeapons.cpp:135` | `REAL PICKUP actor= item= acquired=` |
| `GOAL SKIP` | `BotWeapons.cpp:113` | `GOAL SKIP unreachable ...` |
| `NAV fallback` | `BotBrain.cpp:104` | `NAV fallback ...` |
| `TARGET CHOICE/REACQUIRE/acquired enemy` | `BotCombat.cpp:187,104,190` | `TARGET CHOICE ...` |
| `ACTION REFUSED` | `BotLocomotion.cpp:32` | `ACTION REFUSED: <reason> ...` |
| `TACTIC SAMPLE` / `AI decision` / `ABILITY PHYSICS` / `MOTION INTENT` / `AI telemetry` | `BotBrain.cpp:217-241` | (телеметрия, level 2) |
| `MOVE TRACE` | `MoveTrace` (см. `BotManager.cpp:414`) | `MOVE TRACE actor= ...` |
| `PROGRESS ESCAPE` / `TRAP RECOVERY` | `BotBrain.cpp:571,595` | (удалить с §3) |
| `SPAWN EXCLUDE` / `REVIVE GUARD` | C-014 (`BotManager`, `BotInstance`) | `SPAWN EXCLUDE id= ...` / `REVIVE GUARD actor= ...` |
| `AI start refused/stopped` | `BotBrain.cpp:159,193` | `AI start refused ...` |

Выигрыш: `BotEvent` кеширует `wb_log` (уровень читается один раз в `BotConfig`), единый `[WarsBots]`
префикс, структурированный `kind`; **сами строки (ключ=значение) не трогать** — их разбирает Triage
(`Tools/wb_log_report.py`, `steelmill_triage.py`).

## Порядок применения (Codex)

1. `BotConfig` struct + перенос всех `GetCVar("wb_*")` → поля (поведение идентично).
2. `BotBrain::Update` → под-методы по §2 (порядок `return`-веток сохранить).
3. Удаление 4 флагов §3 после снятия из QA-скриптов.
4. `BotEvent` хелпер + перенос логов (формат не менять).

Каждый пункт — отдельный коммит; после каждого парный soak (`summary.json` без регресса).
