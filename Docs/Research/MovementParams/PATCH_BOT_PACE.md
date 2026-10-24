# WarsBots: черновик патча `wb_bot_pace` (C-013, пять пунктов)

Источник: `Madd3st/crymp-client`, ветка `warsbots/research/movement-params`, база `fb578d52`.
Черновик патча в виде unified diff **против `fb578d52`**; только markdown, к коду **не применялся**.
Игру и серверы не запускал. Код за OpenCode не меняется — ревью и применение делает Codex.

Все изменения — за флагом **`wb_bot_pace` (default 0)**. Регистрация cvar — в
`BotManager::Initialize` рядом с остальными `wb_*` (`BotManager.cpp:76,96-109`), по образцу
`wb_qa_socom_only` (`BotManager.cpp:108`, читается в `BotBrain.cpp:287-288`).

## Plumbing (однажды)

```diff
--- a/Code/CryGame/Bots/BotManager.cpp
+++ b/Code/CryGame/Bots/BotManager.cpp
@@ -76,7 +76,7 @@
-const char* kVariables[] = { "wb_revive_z_test", "wb_revive_hooks_test", "wb_sink_escape", "wb_reachable_goals", "wb_physics_diagnostics", "wb_hotspot_all_nodes", "wb_trap_recovery", "wb_probe_local_test", "wb_bot_cloak_experimental", "wb_maxbots", "wb_skill", "wb_log", "wb_progress_experiment", "wb_fire_diagnostics", "wb_qa_socom_only" };
+const char* kVariables[] = { "wb_revive_z_test", "wb_revive_hooks_test", "wb_sink_escape", "wb_reachable_goals", "wb_physics_diagnostics", "wb_hotspot_all_nodes", "wb_trap_recovery", "wb_probe_local_test", "wb_bot_cloak_experimental", "wb_maxbots", "wb_skill", "wb_log", "wb_progress_experiment", "wb_fire_diagnostics", "wb_qa_socom_only", "wb_bot_pace" };
@@ -108,6 +108,8 @@
     console->Register("wb_qa_socom_only", &g_qaSocomOnly, 0, 0, "Local IA test only: suppress weapon upgrades/fallback, retain native SOCOM/ammo; default OFF.");
+    console->Register("wb_bot_pace", &g_botPace, 0, 0, "C-013 pace parity behind one flag: base 4.5, sprint 40/20, Speed bursts 150/60, Strength 70, pseudoSpeed 1/2/3; default OFF.");
     console->Register("wb_fire_diagnostics", &g_fireDiagnostics, 0, 0, "Opt-in shot/hit diagnostics; at most 20000 records per level; default OFF.");
```

Плюс объявление глобала `int g_botPace = 0;` рядом с `g_qaSocomOnly` в `BotManager.cpp`.

---

## Пункт 1 — база 4.5 м/с вместо клампа 2.5

Сейчас бот просит 2.5 м/с (Run/Sprint) и `SetWarsBotMotion` клампит базу до 2.5, поэтому даже
обычный спринт — 3.75 м/с против 6.75 у игрока (`BotLocomotion.cpp:121`; `Player.cpp:4972`).

```diff
--- a/Code/CryGame/Bots/BotLocomotion.cpp
+++ b/Code/CryGame/Bots/BotLocomotion.cpp
@@ -119,6 +119,10 @@
 	if (intent.move)
-		accepted=m_movement.Move(actor,intent.destination,intent.aim,dt,
-			intent.gait==BotGait::Walk?1.3f:2.5f,intent.stance,sprint?2.7f:1.5f,jump||m_pending||m_airborne,intent.strafe);
+	{
+		const bool pace = gEnv->pConsole->GetCVar("wb_bot_pace") && gEnv->pConsole->GetCVar("wb_bot_pace")->GetIVal()==1;
+		const float base = intent.gait==BotGait::Walk ? 1.3f : (pace ? 4.5f : 2.5f);
+		accepted=m_movement.Move(actor,intent.destination,intent.aim,dt,
+			base,intent.stance,sprint?2.7f:1.5f,jump||m_pending||m_airborne,intent.strafe);
+	}
 	else { m_movement.Stop(actor); m_movement.Aim(actor,intent.aim); }
```

```diff
--- a/Code/CryGame/Actors/Player/Player.cpp
+++ b/Code/CryGame/Actors/Player/Player.cpp
@@ -4972,7 +4972,8 @@
-	m_warsBotSpeed = clamp_tpl(speed,0.0f,2.5f);
+	const bool pace = gEnv->pConsole->GetCVar("wb_bot_pace") && gEnv->pConsole->GetCVar("wb_bot_pace")->GetIVal()==1;
+	m_warsBotSpeed = clamp_tpl(speed,0.0f, pace ? 4.5f : 2.5f);
```

- **file:line**: `BotLocomotion.cpp:121` (база Run/Sprint), `Player.cpp:4972` (кламп). `2.7f` в
  `BotLocomotion.cpp:121` — длина obstacle-probe при спринте, её не трогать.
- **риск**: поднимает фактическую скорость и, как следствие, расход брони при падении/пробежках;
  реплика (`Player.cpp:5013-5014`) получит новую базу по сети — порог `m_warsBotSpeed<2.0f` для
  pseudoSpeed надо согласовать с пунктом 5, иначе анимация реплики рассинхронится с сервером.
- **MOVE TRACE**: `speed=` (горизонтальная м/с) при Run станет ~4.5 вместо ~2.5, при Sprint ~6.75
  вместо ~3.75; `sprint=1` и `mode=` не меняются от этого пункта.

---

## Пункт 2 — спринт: старт ≥ 40, стоп ≤ 20, только на ходу и без стрельбы

Сейчас единственный порог — `state.energy>45.0f` (`BotLocomotion.cpp:117`); нужна петля гистерезиса.

```diff
--- a/Code/CryGame/Bots/BotLocomotion.cpp
+++ b/Code/CryGame/Bots/BotLocomotion.cpp
@@ -115,8 +115,10 @@
-	const bool sprint=intent.move && intent.gait==BotGait::Sprint && intent.combat==BotCombatIntent::None &&
-		state.grounded && state.suitActive && state.energy>45.0f && !jump && !m_pending && !m_airborne && !actor.WarsBotWeaponFiring();
+	bool sprint=intent.move && intent.gait==BotGait::Sprint && intent.combat==BotCombatIntent::None &&
+		state.grounded && state.suitActive && !jump && !m_pending && !m_airborne && !actor.WarsBotWeaponFiring();
+	if (sprint) {
+		const bool pace = gEnv->pConsole->GetCVar("wb_bot_pace") && gEnv->pConsole->GetCVar("wb_bot_pace")->GetIVal()==1;
+		const float start = pace ? 40.0f : 45.0f, stop = pace ? 20.0f : 45.0f;
+		sprint = m_sprinting ? state.energy > stop : state.energy >= start;
+	}
+	m_sprinting = sprint;
```

(плюс `bool m_sprinting=false;` в `BotLocomotion` — `BotLocomotion.h`; сброс в `Reset()`/`Stop()`.)

- **file:line**: `BotLocomotion.cpp:116-117` (executor-гейт). Прочие места спринта: `BotBrain.cpp:73-75`
  (TravelGait `speedTravel` 55/120) и `BotBrain.cpp:542-546` (chase-sprint 100/50) — этот пункт их не
  меняет; при желании свести в единую политику отдельным шагом.
- **риск**: при стоп-пороге 20 бот дольше «висит» в Speed-сливе, чем раньше (раньше выходил при <45);
  зато не дрожит на границе 45. `combat==None && !WarsBotWeaponFiring()` сохранены — в бою не спринтует.
- **MOVE TRACE**: `sprint=` переключается в 1 при `energy>=40` и в 0 при `energy<=20`; на трейсе
  `energy=` видна полоса 40→20 вместо дрожания у 45; `speed=` при этом не растёт (множитель ×1.5 тот же).

---

## Пункт 3 — Armor по умолчанию, Speed-рывки на дистанции ≥ 30 м при энергии ≥ 150, назад в Armor ≤ 60

Сейчас `TravelGait` включает Speed при `energy>120 && distance>15` и выключает при `<55` (`BotBrain.cpp:73-75`).

```diff
--- a/Code/CryGame/Bots/BotBrain.cpp
+++ b/Code/CryGame/Bots/BotBrain.cpp
@@ -72,8 +72,10 @@
 	if (!m_profile.useSpeed) { m_intent.gait=BotGait::Run; return; }
-	if (m_speedTravel) { if (m_blackboard.energy < 55.0f || distance < 8.0f) m_speedTravel = false; }
-	else if (m_blackboard.energy > 120.0f && distance > 15.0f) m_speedTravel = true;
+	if (m_speedTravel) { if (m_blackboard.energy <= 60.0f || distance < 8.0f) m_speedTravel = false; }
+	else if (m_blackboard.energy >= 150.0f && distance >= 30.0f) m_speedTravel = true;
 	m_intent.gait = m_speedTravel ? BotGait::Sprint : BotGait::Run;
-	if (!m_speedTravel) return;
+	if (!m_speedTravel) { m_intent.suitMode = NANOMODE_DEFENSE; return; }
 	m_intent.suitMode = NANOMODE_SPEED;
```

- **file:line**: `BotBrain.cpp:73-75` (пороги `speedTravel`), `BotIntent.h:23` (дефолт `suitMode=NANOMODE_DEFENSE`
  = Armor уже стоит). Возврат в Armor — явный `NANOMODE_DEFENSE` в ветке `!m_speedTravel`.
- **риск**: Speed-рывок реже и короче (нужны длинные перегоны); на коротких дистанциях бот в Armor
  тратит энергию медленнее. Смена режима по-прежнему проходит через `BotSuit::Apply` с кулдауном
  `nextSuitChange=+2.0f` (`BotSuit.cpp:39`).
- **MOVE TRACE**: `mode=` держит 3 (Armor) почти всё время, переключается в 0 (Speed) только при
  перегоне `distance>=30` и `energy>=150`, и обратно в 3 при `energy<=60`; `energy=` показывает слив
  150→60, `speed=` — ×1.5 спринт только внутри рывка.

---

## Пункт 4 — множители Speed-mode до ботов по bot-пути (не трогая человеческий `IsPlayer()`)

`AdjustMovementForEnvironment` вызывается только для `IsPlayer()` (`PlayerMovement.cpp:1070-1071`),
поэтому бот не получает `×1.3` ходьбы и `×1.5` слота Speed (`PlayerMovement.cpp:1422-1433`). Добавляем
эквивалент в bot-путь, не меняя человеческую ветку.

```diff
--- a/Code/CryGame/Bots/BotLocomotion.cpp
+++ b/Code/CryGame/Bots/BotLocomotion.cpp
@@ -119,8 +119,12 @@
 	if (intent.move)
 	{
 		const bool pace = gEnv->pConsole->GetCVar("wb_bot_pace") && gEnv->pConsole->GetCVar("wb_bot_pace")->GetIVal()==1;
-		const float base = intent.gait==BotGait::Walk ? 1.3f : (pace ? 4.5f : 2.5f);
+		float base = intent.gait==BotGait::Walk ? 1.3f : (pace ? 4.5f : 2.5f);
+		// Bot-side equivalent of AdjustMovementForEnvironment's Speed-mode bonuses
+		// (PlayerMovement.cpp:1422-1433), which are IsPlayer()-gated at :1070-1071.
+		if (pace && state.mode==NANOMODE_SPEED)
+			base *= (sprint ? 1.5f : 1.3f);
 		accepted=m_movement.Move(actor,intent.destination,intent.aim,dt,
 			base,intent.stance,sprint?2.7f:1.5f,jump||m_pending||m_airborne,intent.strafe);
 	}
```

- **file:line**: `BotLocomotion.cpp:121` (умножаем базу до `SetWarsBotMotion`), отсылка на
  `PlayerMovement.cpp:1070-1071,1422-1433`. Человеческий `IsPlayer()` не трогаем.
- **риск**: закладываем множитель в базу (а не в native-множитель), поэтому `m_warsBotSpeed` и реплика
  видят уже увеличенное число — дельту надо не задвоить с native-спринтом `×1.5` из
  `PlayerMovement.cpp:1041-1045` (тот и так применяется к любому актору с `ACTION_SPRINT`).
- **MOVE TRACE**: при `mode=0` (Speed) и `sprint=0` `speed=` ≈ базе ×1.3, при `sprint=1` ≈ ×1.5;
  сравнивается с теми же кадрами при `wb_bot_pace=0`, где множитель отсутствует.

---

## Пункт 5 — Strength-гейт 70, кулдаун прыжка 0.5 с, pseudoSpeed 1/2/3

```diff
--- a/Code/CryGame/Bots/BotLocomotion.cpp
+++ b/Code/CryGame/Bots/BotLocomotion.cpp
@@ -105,5 +105,5 @@
-		else if (strength && (!state.suitActive || state.mode!=NANOMODE_STRENGTH || state.energy<90.0f)) Refused(actor,"jump_strength_mode_or_energy",now);
+		else if (strength && (!state.suitActive || state.mode!=NANOMODE_STRENGTH || state.energy<70.0f)) Refused(actor,"jump_strength_mode_or_energy",now);
@@ -111,5 +111,5 @@
-			m_startZ=m_apex=state.position.z; state.nextJump=now+3.0f;
+			m_startZ=m_apex=state.position.z; state.nextJump=now+0.5f;
```

(гейт 70/кулдаун 0.5 — только при `wb_bot_pace=1`; `JumpSpace`-проверки безопасности `BotLocomotion.cpp:64-93`
остаются и в pace-режиме.)

pseudoSpeed: заменить серверную 0.3/0.5 на 1/2/3, а реплику — на ту же шкалу.

```diff
--- a/Code/CryGame/Bots/BotMovement.cpp
+++ b/Code/CryGame/Bots/BotMovement.cpp
@@ -108,5 +108,5 @@
-	request.SetPseudoSpeed(speed<2.0f?0.3f:0.5f);
+	request.SetPseudoSpeed(pseudo);
```

(в `BotMovement::Move` добавить параметр `float pseudo`; вызывающий в `BotLocomotion::Execute` передаёт
`intent.gait==BotGait::Walk?1.0f : (sprint ? (state.mode==NANOMODE_SPEED?3.0f:2.0f) : 1.0f)` — шкала
игрока `CalculatePseudoSpeed` `Player.cpp:2474-2486`.)

```diff
--- a/Code/CryGame/Actors/Player/Player.cpp
+++ b/Code/CryGame/Actors/Player/Player.cpp
@@ -5014,5 +5014,5 @@
-		request.SetPseudoSpeed(m_warsBotSpeed>0.01f ? (m_warsBotSprint?1.0f:(m_warsBotSpeed<2.0f?0.3f:0.5f)) : 0.0f);
+		request.SetPseudoSpeed(m_warsBotSpeed>0.01f ? (m_warsBotSprint?2.0f:1.0f) : 0.0f);
```

- **file:line**: `BotLocomotion.cpp:105` (Strength 90→70), `:111` (кулдаун 3.0→0.5),
  `BotMovement.cpp:108` и `Player.cpp:5014` (pseudoSpeed). Сверка с игроком: `PlayerMovement.cpp:1138`
  (энергия ≥70), кулдаун «сразу после приземления» (`Player.cpp:3529-3535,3790`).
- **риск**: прыжков больше (кулдаун 0.5 против 3.0) и Strength доступен раньше — выше частота
  `jump_clearance_or_landing` отказов (`JumpSpace`), заметнее нагрузка на raycast. pseudoSpeed 2/3
  меняет только анимацию, не скорость (`PlayerMovementController.cpp:1298-1305`).
- **MOVE TRACE**: `jump=`/`jumpEdges=`/`takeoffs=`/`landEdges=` растут (чаще прыжки), `mode=1` (Strength)
  появляется уже при `energy>=70`, а не ≥90; `jumpTime`/`landTime` — чаще; `speed=` не зависит от pseudoSpeed.

## Сводка рисков (общая)

- Всё за `wb_bot_pace=0` — дефолт-билд и дефолт-рантайм не меняются.
- Пункты 1 и 4 складываются: база 4.5 + Speed-множитель дают пик выше, чем любой прежний потолок бота
  (но не выше игрока 14.73 м/с). Проверять парным прогоном `wb_bot_pace 0` vs `1`.
- Пункт 5 меняет частоту прыжков — смотреть на счётчики `takeoffs`/`landEdges`, а не на `speed`.
