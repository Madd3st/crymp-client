# WarsBots: черновик патча `wb_decision_commit` (из списка фиксов O-004)

Источник: `Madd3st/crymp-client`, ветка `warsbots/research/movement-params`, база `fb578d52`.
Черновик патча в виде unified diff **против `fb578d52`**; только markdown, к коду **не применялся**.
Игру и серверы не запускал. Код за OpenCode не меняется — ревью и применение делает Codex.

Цель: сломать осцилляцию `CHASING_ENEMY ↔ MOVING_TO_WEAPON` (4579 A→B→A за 48 мин, ~1.6/с),
разобранную в `DECISION_FLIPS.md`. Два механизма за флагом **`wb_decision_commit` (default 0)**:
1. **дебаунс видимости** — враг считается «потерянным» только после ~1.0 с невидимости;
2. **commitment-окно** — войдя в `MOVING_TO_WEAPON`, бот держит его ≥ 3 с, если враг не виден в
   пределах `engageRange` и бот не получил урон.

Регистрация cvar — там же, где прочие `wb_*` (`BotManager.cpp:76,96-109`), по образцу
`wb_qa_socom_only` (`BotManager.cpp:108`, читается в `BotBrain.cpp:287-288`).

```diff
--- a/Code/CryGame/Bots/BotManager.cpp
+++ b/Code/CryGame/Bots/BotManager.cpp
@@ -108,6 +108,8 @@
     console->Register("wb_qa_socom_only", &g_qaSocomOnly, 0, 0, "Local IA test only: suppress weapon upgrades/fallback, retain native SOCOM/ammo; default OFF.");
+    console->Register("wb_decision_commit", &g_decisionCommit, 0, 0, "Decision commitment + visibility debounce for CHASING vs MOVING_TO_WEAPON flips; default OFF.");
     console->Register("wb_fire_diagnostics", &g_fireDiagnostics, 0, 0, "Opt-in shot/hit diagnostics; at most 20000 records per level; default OFF.");
```

(плюс `int g_decisionCommit = 0;` рядом с `g_qaSocomOnly`; имя `wb_decision_commit` добавить в
`kVariables[]` — `BotManager.cpp:76`.)

---

## Hunk 1 — геттер `LastSeen()` (нужен для дебаунса)

`m_lastSeen` уже обновляется только когда враг виден (`BotCombat.cpp:117`), т.е. это время последней
видимости; публичного геттера нет.

```diff
--- a/Code/CryGame/Bots/BotCombat.h
+++ b/Code/CryGame/Bots/BotCombat.h
@@ -29,6 +29,7 @@
 	float SampleTime() const { return m_sampleTime; }
+	float LastSeen() const { return m_lastSeen; }
 	// Last Attack had LOS from the eyes but the barrel was blocked by cover.
```

## Hunk 2 — дебаунс видимости на входе в апгрейд оружия

Сейчас вход в `MOVING_TO_WEAPON` по апгрейду использует мгновенный `!m_combat.Visible()`
(`BotBrain.cpp:334`). Заменяем на «не виден дольше 1.0 с» (когда флаг включён).

```diff
--- a/Code/CryGame/Bots/BotBrain.cpp
+++ b/Code/CryGame/Bots/BotBrain.cpp
@@ -331,6 +331,8 @@
 	IActor* enemy = m_combat.FindEnemy(actor, m_now, m_verbose);
 	if (m_now<m_escapeUntil) enemy=nullptr;
+	const bool commit = gEnv->pConsole->GetCVar("wb_decision_commit") && gEnv->pConsole->GetCVar("wb_decision_commit")->GetIVal()==1;
+	const bool lost = commit ? (m_now - m_combat.LastSeen() > 1.0f) : !m_combat.Visible();
 	// Holding only the spawn pistol and nobody in sight: go for a rifle like a player would.
-	if (!socomQA && (!enemy || !m_combat.Visible()) && !BotWeapons::HasPrimary(actor))
+	if (!socomQA && (!enemy || lost) && !BotWeapons::HasPrimary(actor))
```

- **file:line**: `BotBrain.cpp:334` (условие), `BotCombat.cpp:117` (когда обновляется `m_lastSeen`).
- **эффект**: короткое мерцание врага за укрытием (<1 с) больше не запускает забег за оружием.

## Hunk 3 — commitment-окно на выходе из MOVING_TO_WEAPON

Добавляем таймер удержания `m_weaponCommitUntil` и ставим его при входе в `MOVING_TO_WEAPON` по апгрейду.

```diff
--- a/Code/CryGame/Bots/BotBrain.h
+++ b/Code/CryGame/Bots/BotBrain.h
@@ -59,6 +59,7 @@
 	float m_now = 0, m_tick = 0, m_stateTime = 0, m_weaponSearchStarted = 0, m_nextSearch = 0, m_deadTime = 0;
+	float m_weaponCommitUntil = 0;
```

```diff
--- a/Code/CryGame/Bots/BotBrain.cpp
+++ b/Code/CryGame/Bots/BotBrain.cpp
@@ -357,6 +357,7 @@
 		IEntity* item = m_upgradeTarget ? gEnv->pEntitySystem->GetEntity(m_upgradeTarget) : nullptr;
 		if (item)
 		{
+			if (commit) m_weaponCommitUntil = m_now + 3.0f;
 			Change(State::MOVING_TO_WEAPON);
 			const Vec3 feet = actor.GetEntity()->GetWorldPos();
```

Теперь в enemy-ветке не даём `MOVING_TO_WEAPON` уйти в `CHASING_ENEMY` внутри окна, пока враг не
«боеспособно близко» и бот не под уроном. Вместо погони держим курс на `m_upgradeTarget`.

```diff
--- a/Code/CryGame/Bots/BotBrain.cpp
+++ b/Code/CryGame/Bots/BotBrain.cpp
@@ -520,6 +520,26 @@
 	}
 	else
 	{
+		const bool commitHold = commit && m_state == State::MOVING_TO_WEAPON && m_now < m_weaponCommitUntil;
+		const bool engage = visible && distance <= m_profile.engageRange;
+		const bool hurt = m_now < m_blackboard.damagedUntil;
+		if (commitHold && !engage && !hurt)
+		{
+			WB_LOG(1, "[WarsBots] DECISION COMMIT actor=%u from=%s to=%s held=%.2f.",
+				actor.GetEntityId(), NameOf(m_state), "MOVING_TO_WEAPON", m_now - m_stateTime);
+			IEntity* item = m_upgradeTarget ? gEnv->pEntitySystem->GetEntity(m_upgradeTarget) : nullptr;
+			if (item)
+			{
+				const Vec3 step = Steer(actor.GetEntity()->GetWorldPos(), item->GetWorldPos());
+				m_intent.MoveTo(step, step + Vec3(0,0,1.2f));
+				TravelGait((item->GetWorldPos() - actor.GetEntity()->GetWorldPos()).GetLength());
+				ExecuteIntent(actor, decisionTime);
+				return;
+			}
+			// цель оружия исчезла — окно снимается, падаем в обычную погоню
+			m_weaponCommitUntil = 0;
+		}
 		Change(State::CHASING_ENEMY);
 		m_intent.MoveTo(Steer(actor.GetEntity()->GetWorldPos(),destination),aim);
```

- **file:line**: `BotBrain.cpp:357-360` (ставим окно), `:520-522` (удержание вместо погони),
  `BotBrain.h:59-60` (новое поле), `BotProfile.h:16` (`engageRange`), `BotBlackboard.h:13`
  (`damagedUntil`).
- **семантика**: `engage = visible && distance <= engageRange` — если враг реально «в бою», окно
  отпускает; `hurt = m_now < m_blackboard.damagedUntil` — полученный урон тоже снимает окно.
  Движение к оружию переиспользует код апгрейда (`BotBrain.cpp:361-371`).
- **лог**: строка `DECISION COMMIT actor= from= to= held=` доказывает, сколько раз окно подавило
  переключение и сколько бот пробыл в `MOVING_TO_WEAPON`.

## Ожидаемый эффект на A→B→A

Осцилляция (`DECISION_FLIPS.md` §5) — произведение двух мгновенных условий: вход в апгрейд по
голому `!Visible()` и вход в `CHASING_ENEMY` по «виден, но далеко». Дебаунс 1.0 с отсекает
субсекундное мерцание (большинство из ~1.6 переключений/с), а окно 3 с гасит обратный переход,
когда враг на миг показался. Ожидаемое падение A→B→A — **в ~1–2 порядка**: остаточные переключения
только от реальной потери/повторного обнаружения врага (>1 с), от входа в `engageRange` или от урона.
Метрика — тот же счётчик «A→B→A» из triage; до/после на одном бинарнике `wb_decision_commit 0` vs `1`.

## Риски

- Оба механизма за `wb_decision_commit=0` — дефолт не меняется.
- Дебаунс 1.0 с добавляет задержку реакции на реально скрывшегося врага (до 1 с бот продолжает
  «считать» его потерянным позже); на pace-сценариях это сознательный трейд.
- Commitment-окно 3 с может кратковременно удерживать бота на оружии, пока рядом стреляют — поэтому
  `hurt` снимает окно; проверить, что `damagedUntil` ставится и по урону от снайпера/взрывов.
- Новое поле в `BotBrain` сериализуется только как часть клиент/серверного билда — флаг не меняет
  сетевой раскладки (это серверный таймер решения), но парный билд клиент/сервер обязателен по
  инвариантам.
