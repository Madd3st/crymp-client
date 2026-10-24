# F.E.A.R. AI architecture (source study)

Source: `github.com/jsj2008/lithtech`, directory `FEAR/ObjectDLL/` (1109 files; 145 `AIGoal*`,
246 `AIAction*`, 116 `AISensor*`). Every file carries
`(c) 2003 Monolith Productions, Inc. All Rights Reserved` — this is the public F.E.A.R. SDK,
**not** an open licence. Studied for architecture only; no code or identifiers were copied into
WarsBots. All descriptions below are paraphrases with file references.

## The loop

```
Sensors (distributed) -> WorkingMemory facts -> Blackboard flags
        -> GoalMgr: relevance per goal -> try best goal -> Planner builds plan
        -> Plan executes one action at a time; actions validate and complete
        -> events set "select action" -> relevance re-evaluated -> maybe replan
```

| Piece | File(s) | What it actually does |
| --- | --- | --- |
| World state | `AIWorldState.h` | ~22 symbolic keys (`AtNode`, `CoverStatus`, `WeaponLoaded`, `TargetIsDead`, `TargetIsSuppressed`, `PositionIsValid` ...). Small on purpose. |
| Actions | `AIActionAbstract.h/.cpp` + 120 subclasses | Symbolic preconditions/effects, `ValidateContextPreconditions` (real-world check, e.g. a usable node), `ValidateAction` while running, `IsActionComplete`, `IsActionInterruptible`, a cost and an optional activation probability. |
| Planner | `AIPlanner.cpp`, `AIAStarPlanner*` | A* *regressive* search from the goal state back to the current state through actions whose effects satisfy unsatisfied keys (`BuildPlan`). Each plan step stores the world state it is expected to achieve. |
| Plan execution | `AIPlanner.cpp` `CAIPlan::*` | Advances when the current action completes; re-validates context before activating the next step; `IsPlanValid` asks the running action whether it still holds. |
| Goals | `AIGoalMgr.cpp`, `AIGoal*.cpp` | Each goal computes a relevance; `UpdateGoalRelevances` takes the most relevant goal that can build a plan, otherwise tries the next (`HandleBuildPlanFailure`). |
| Commitment | `AIGoalMgr.cpp` `UpdateGoalRelevances` | If the best goal has the **same** relevance as the current one, keep the current. Goals declare an interrupt priority; during locked animations only a higher-priority goal may interrupt. `ReplanRequired()` per goal. Optional random activation chance. |
| Replanning trigger | 44 sites of `SetBBSelectAction(true)` (sensors, damage, navigation, commands) | Replanning is **event-driven**: sensors/actions raise a blackboard flag; goals are not re-planned every frame. |
| Working memory | `AIWorkingMemory.h/.cpp` | Facts typed `Character`, `Damage`, `Danger`, `Desire`, `Disturbance`, `Node`, `PathInfo`, `Task`, `Knowledge`, each attribute with its own **confidence** and update time. Target knowledge decays instead of being ground truth. |
| Sensors | `AISensorMgr.cpp` `UpdateSensors` | Cheap sensors run every update; expensive ones are round-robin and at most one per AI per update; `AIMgr.cpp:506-544` lets only **one AI per frame** do distributed sensor work. |
| Cover | `AIGoalCover.cpp`, `AINode*`, `AINodeValidators.cpp` | Cover is **designer-placed nodes** with a forward direction. Validators check threat FOV relative to the node, threat radius, "threat is aiming at node", locked by another AI, damaged, path blocked, expiration. |
| Squads | `AISquad.cpp`, `AICoordinator.h`, `AIActivity*.cpp` | Squad "activities" (`AdvanceCover`, `GetToCover`, `OrderlyAdvance`, `Search`, `ExchangeWeapons`) pick participants and write **Task facts** into members' working memory; members' own goals then act on those tasks. No central puppet control. |

## How the behaviours asked about map onto this

- *Target selection*: separate `AITargetSelect*` classes (character, damager, disturbance,
  urgent, squad) scored by the target-select manager; the goal system never chooses targets.
- *Attack → defence*: different goals (`Cover`, `Retreat`, `Flee`, attack goals) win on relevance;
  the switch happens through relevance plus interrupt priority, not through state edges.
- *Cover*: only valid against the current threat position (FOV/radius validators). Invalidated
  cover makes the running action invalid and raises replanning.
- *Contradictory actions*: impossible by construction — one plan, one running action, and
  actions declare whether they may be interrupted.
- *Coordination*: squad tasks + node locking (`LockedByOther`), so two AIs do not take the same
  node.

## What made it look smart (and what did not)

The often-quoted point holds in the code: **plans are short**. Most goal/action pairs resolve
to "go to a valid node, then do X". The perceived intelligence came from (1) many small actions
with honest context checks, (2) hand-placed, annotated cover/ambush nodes with validators,
(3) squad tasks that create flanking and suppression, (4) event-driven replanning, and (5)
barks/animation that *explain* the decision to the player. The symbolic search itself is the
smallest part.

## Fit for WarsBots

| F.E.A.R. idea | Transfer | Why |
| --- | --- | --- |
| Goal relevance + commitment + interrupt priority | **yes** | Directly addresses measured ping-pong (WARSBOTS_AI_GAPS G2). |
| Context preconditions verified against the world | **yes** | Our bots walk at unverified goals (G3, Steelmill F3). |
| Event-driven replanning, distributed sensing | **yes** | Scales to 15+ bots; we already throttle A* and target scans. |
| Working memory with confidence/decay | partly | We keep last-seen position and age; confidence per attribute is not needed yet. |
| Designer cover nodes | **no data** | Stock PS/IA maps ship header-only CryAI navigation (CryMP audit, `CRYMP_CAPABILITIES.md`). Cover must be *generated* from BotNav with ray probes. |
| Regressive symbolic planner | optional | Our measured plans are 1–2 steps (prototype: max 4 node expansions). Value appears only with multi-step tasks such as PS buy → equip → capture. |
| Squad tasks in member memory | **yes, for PS** | Matches the planned PSCommander: commander writes tasks, bots keep their own combat loop. |
| Story-driven enemy behaviours (ambush scripts, flush-out) | no | Multiplayer participants with symmetric rules. |
