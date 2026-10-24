# WarsBots: демо-запись и воспроизведение (дизайн, D-демо)

Источник: `Madd3st/crymp-client`, ветка `warsbots/research/movement-params`, база `fb578d52`.
Только чтение кода, игра/серверы не запускались. Цель владельца — запись матча (боты vs боты/владелец) и
повтор, как в Counter-Strike (смотреть любого игрока).

## 1. Что уже есть в CryMP (аудит)

| Система | Что записывает | Пригодно для MP-повтора? | file:line |
| --- | --- | --- | --- |
| `IGameplayRecorder` / `IGameplayListener` | **только события** (выстрел/попадание/убийство/смерть/возрождение/смена костюма/подбор предмета/техника), без потикового положения | нет (поток событий, не «фильм») | `CryCommon/CryAction/IGameplayRecorder.h:16-64` (eGE_*), `:67-77` (GameplayEvent), `:80-97` |
| `GameplayRecorder` (CryAction) | диспетчер событий → слушатели | событийный хаб | `Code/CryAction/GameplayRecorder.cpp:37-44` |
| `CSPAnalyst` (SP) | SP-статистика в **XML** (kills/deaths + потиковый `RecordGameplayFrame`) | нет (только SP, статистика, не повтор) | `Code/CryGame/SPAnalyst.h:100-128`; заголовок `:13` «SP gameplay to file recording» |
| `GameplayAnalyst` (CryAction) | слушатель-заглушка (реализация в закрытом бинарнике) | частично | `Code/CryAction/GameplayAnalyst.h:5-21` |
| `TimeDemoRecorder` | бенчмарк time-demo (путь камеры + рендер/фреймтаймы), **без сущностей** | только замер FPS | `Code/CryAction/TimeDemoRecorder.h:9-19`; `.cpp:11-17` (offset-patch закрытого бинарника); `GameFramework.cpp:208,372,504,1244-1246` |
| `INetwork::ActivateDemoRecorder(filename)` | (объявлено) CS-style захват сетевых сообщений — **не вызывается нигде** | крюк есть, не подключён | `CryCommon/CryNetwork/INetwork.h:587`; флаг `eGSF_DemoRecorder=0x0400` `IGameFramework.h:142` |
| `record` / `demo` консольные команды | — | **отсутствуют** в дереве (grep 0 совпадений) | не найдено |
| killcam | — | **нет** (в Crysis killcam нет) | не найдено |

Эмиттеры событий уже стоят по всему игровому коду: `eGE_WeaponShot` (`Items/Weapons/FireModes/Shotgun.cpp:366,500`),
`eGE_Death` (`Actors/Player/Player.cpp:4612`), `eGE_SuitModeChanged` (`NanoSuit.cpp:849`), `eGE_*` item-события
(`CryAction/ItemSystem.cpp:796`). То есть **событийная** запись (выстрелы/убийства) доступна через
`IGameplayRecorder`, а **потикового снапшота позы** нет — его даёт только наш MOVE TRACE (4 Гц).

Вывод: **готового CS-подобного демо в CryMP нет**. `ActivateDemoRecorder` — мёртвый крюк; `TimeDemoRecorder` —
бенчмарк; события есть, а «фильма» сущностей нет.

## 2. Наш дизайн (серверная запись снапшотов + два плеера)

### 2.1 Запись (сервер, флаг `wb_demo_record`, default 0)

Раз в N тиков (10–20 Гц) сервер пишет для каждой owned-сущности (боты `CPlayer` channel 0 + реальные
игроки) компактную запись:

```
frame: { t_ms, n_actors }
actor: { id, pos(x,y,z), yaw, pitch, stance(2b), weaponId, health(1b), mode(2b), energy(1b) }
event: { t_ms, actor, kind, a, b }   // shot/hit/kill/death/revive/suit/item/jump
```

- **Поза**: `pos` + `yaw` (взгляд) + `pitch` из `GetWorldPos()`/`GetBaseQuat()`/aim — то же, что уже сериализует
  `SetWarsBotMotion` (`Player.cpp:4966-4974`) и MOVE TRACE. Stance/weapon/health/mode/energy — из `CPlayer`
  и `BotBlackboard` (`BotBlackboard.h:9-16`).
- **События** — вешаемся `IGameplayListener` (`IGameplayRecorder.h:80-83`) и пишем shot/hit/kill/death/suit
  (существующие `eGE_*`), плюс jump-событие из `PublishWarsBotJump` (`Player.cpp:4960-4965`).
- **Квантование**: pos до 1 см (int16 на 2 км), yaw/pitch до 1° (int16), health 0–200 → u8. Дельтой
  (yaw как приращение) можно сжать ещё.

**Размер** (оценка): 10 Гц × ~40 Б/актор ≈ 6.4 КБ/с для 16 акторов → **~23 МБ/час**; 20 Гц → ~46 МБ/час;
с дельта-квантованием ~12–25 МБ/час. Для 10-минутного матча — единицы МБ. Приемлемо. Файл
`<memory>/demos/<map>-<ts>.wdemo` (рядом с текущим `WarsBots/Memory`), буферный флаш ~1 с (как MOVE TRACE).

### 2.2 Воспроизведение A — in-game spectator (puppet-сущности)

- `wb_demo_play <file>` на клиенте: загружаем снапшоты, спавним **puppet-сущности** (по одной на актора) —
  channel-less `CPlayer`-болванки без физики/коллизий, как уже делают бот-реплики
  (`Player.cpp:5222-5228` «Motion replica ... no channel»). Каждый кадр пишем им `SetWarsBotMotion`/позу из
  снапшота (тот же путь, что `UpdateWarsBotMotion`, `Player.cpp:4976-5019`).
- Просмотр — существующий spectator: `wb_watch list|<botNumber>|next|prev` + нативный F1 FP/TP (из
  `AGENT_HANDOFF`/`BotSpectator.h`). Владелец свободно летает/следует за любым puppетом.
- Требуется только клиент; сервер не нужен (файл читается локально).

### 2.3 Воспроизведение B — оффлайн-вьюер

- Тот же файл читается Python-инструментом (как `steelmill_triage.py`/Lab map viewer) поверх геометрии из
  `wb_geom_export` OBJ (`ENGINE_GEOMETRY_EXPORT.md`). Рендер снапшотов по кадрам (позиции/взгляд/события) без
  движка. Самый дешёвый вариант, делится как отчёт.

### 2.4 Усилия и риски

- **Запись**: 2–3 дня (схема + файл I/O + крюки в `BotManager::OnPostUpdate` рядом с `m_moveTrace.Update`,
  `BotManager.cpp:414`, + `IGameplayListener`).
- **Плеер A**: 3–5 дней (puppet-сущности + повтор поз + привязка к spectator).
- **Плеер B**: 1–2 дня (вьюер поверх Triage).
- **Риски**: (1) файл I/O на сервере — буферизовать, как MOVE TRACE; (2) **детерминизм не нужен** — пишем
  абсолютные позы, а не входы, поэтому реплей всегда корректен (в отличие от CS input-replay); (3) puppet
  без коллизий — не застревают, но и не «пересекаются» с геометрией идеально; (4) fidelity 10 Гц — прыжки/
  повороты сглаживаются интерполяцией (в плеере A между снапшотами), для точного aim повышать до 20 Гц;
  (5) «смотреть любого» — puppet-акторы уже покрыты spectator-кодом, нужен только маппинг id→puppet.

Рекомендация: стартовать с **B** (быстро, делится как отчёт), затем **A** (владельцу — «посмотреть матч»).
Всё за флагом `wb_demo_record` (default 0); запись не трогает AI/рантайм при выключенном флаге.
