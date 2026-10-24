# FPS AI Lab bench results

Generated 2026-10-24T01:34Z by `fpsai_bench` (g++ 13.3 -O2, Linux x86-64 cloud VM, single thread) from source commit on `warsbots/research/fps-ai-planning`.
Synthetic decision-level scenarios: crude straight-line world, scripted enemy. **Not game results.** Damage numbers compare choices inside this toy world only.

| Scenario | Policy | Action switches | Ping-pong (A-B-A < 3 s) | Walk-back ping-pong | Stalled s | Unreachable abandoned at s | Threat reaction s | Damage taken | Damage dealt | End HP | Hidden-info leaks | us/decision mean | p99 |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| A_visible_full_clip | legacy | 2 | 0 | 0 | 0.0 | - | - | 16 | 103 | 84 | 0 | 0.07 | 0.79 |
| A_visible_full_clip | planner | 2 | 0 | 0 | 0.0 | - | - | 16 | 103 | 84 | 0 | 0.93 | 14.46 |
|  | legacy: Attack 0.0-2.2; Search 2.2-12.1;  |
|  | planner: Attack 0.0-2.2; Search 2.2-12.1;  |
|  |   t=0.0 KillEnemy -> Attack : KillEnemy u=0.90 facts=HasWeapon|Loaded|EnemyVisible|EnemyKnown|CoverAvail|HasPrimary|RouteLastSeen|FlankAvail|AltLoaded |
|  |   t=2.2 Patrol -> Search : Patrol u=0.10 facts=HasWeapon|Loaded|CoverAvail|UnderFire|HasPrimary|RouteLastSeen|FlankAvail|AltLoaded |
| B_low_health_under_fire | legacy | 4 | 0 | 0 | 0.0 | - | - | 23 | 103 | 2 | 0 | 0.05 | 0.17 |
| B_low_health_under_fire | planner | 2 | 0 | 0 | 0.0 | - | - | 4 | 0 | 21 | 0 | 0.62 | 2.45 |
|  | legacy: Attack 0.0-0.7; Retreat 0.7-3.3; Pursue 3.3-4.8; Search 4.8-12.1;  |
|  | planner: SeekCover 0.0-0.9; HoldPosition 0.9-12.1;  |
|  |   t=0.0 Survive -> SeekCover : Survive u=0.88 facts=HasWeapon|Loaded|EnemyVisible|EnemyKnown|CoverAvail|LowHealth|HasPrimary|RouteLastSeen|FlankAvail|AltLoaded |
|  |   t=0.9 Survive -> HoldPosition : Survive u=0.95 facts=HasWeapon|Loaded|EnemyKnown|InCover|CoverAvail|LowHealth|UnderFire|HasPrimary|RouteLastSeen|FlankAvail|AltLoaded |
| C_unreachable_weapon | legacy | 2 | 0 | 0 | 10.0 | 10.0 | - | 0 | 0 | 100 | 0 | 0.05 | 0.20 |
| C_unreachable_weapon | planner | 1 | 0 | 0 | 0.0 | 0.60 | - | 0 | 0 | 100 | 0 | 0.47 | 2.24 |
|  | legacy: GoToWeapon 0.0-10.0; Search 10.0-15.1;  |
|  | planner: Search 0.0-15.1;  |
|  |   t=0.0 Patrol -> Search : Patrol u=0.10 facts=HasWeapon|Loaded |
| D_enemy_hides | legacy | 4 | 1 | 0 | 0.0 | - | - | 12 | 47 | 88 | 0 | 0.05 | 0.17 |
| D_enemy_hides | planner | 3 | 0 | 0 | 0.0 | - | - | 12 | 47 | 88 | 0 | 0.58 | 1.65 |
|  | legacy: Pursue 0.0-0.4; Attack 0.4-1.0; Pursue 1.0-5.0; Search 5.0-12.1;  |
|  | planner: Attack 0.0-1.0; Pursue 1.0-4.5; Search 4.5-12.1;  |
|  |   t=0.0 KillEnemy -> Attack : KillEnemy u=0.90 facts=HasWeapon|Loaded|EnemyVisible|EnemyKnown|HasPrimary|RouteLastSeen|AltLoaded |
|  |   t=1.0 Investigate -> Pursue : Investigate u=0.60 facts=HasWeapon|Loaded|EnemyKnown|UnderFire|HasPrimary|RouteLastSeen|AltLoaded |
|  |   t=4.5 Patrol -> Search : Patrol u=0.10 facts=HasWeapon|Loaded|EnemyKnown|HasPrimary|AtLastSeen|RouteLastSeen|AltLoaded |
| E_new_threat_mid_plan | legacy | 3 | 0 | 0 | 0.0 | - | 0.00 | 101 | 78 | -1 | 0 | 0.06 | 0.15 |
| E_new_threat_mid_plan | planner | 8 | 2 | 0 | 0.0 | - | 0.00 | 66 | 102 | 34 | 0 | 0.59 | 1.58 |
|  | legacy: GoToWeapon 0.0-2.0; Attack 2.0-8.6; Retreat 8.6-10.4;  |
|  | planner: GoToWeapon 0.0-2.0; Attack 2.0-3.2; ChangeWeapon 3.2-3.3; Attack 3.3-4.5; Reload 4.5-6.5; Attack 6.5-7.5; GoToWeapon 7.5-11.3; Search 11.3-12.1;  |
|  |   t=0.0 AcquireWeapon -> GoToWeapon : AcquireWeapon u=0.43 facts=HasWeapon|Loaded|UpgradeReach|AltLoaded |
|  |   t=2.0 KillEnemy -> Attack : KillEnemy u=0.90 facts=HasWeapon|Loaded|EnemyVisible|EnemyKnown|UpgradeReach|RouteLastSeen|AltLoaded |
|  |   t=3.2 KillEnemy -> ChangeWeapon : KillEnemy u=0.86 facts=HasWeapon|EnemyVisible|EnemyKnown|UnderFire|UpgradeReach|RouteLastSeen|AltLoaded |
|  |   t=3.3 KillEnemy -> Attack : KillEnemy u=0.85 facts=HasWeapon|Loaded|EnemyVisible|EnemyKnown|UnderFire|UpgradeReach|RouteLastSeen |
|  |   t=4.5 KillEnemy -> Reload : KillEnemy u=0.81 facts=HasWeapon|EnemyVisible|EnemyKnown|UnderFire|UpgradeReach|RouteLastSeen |
|  |   t=6.5 KillEnemy -> Attack : KillEnemy u=0.74 facts=HasWeapon|Loaded|EnemyVisible|EnemyKnown|UnderFire|UpgradeReach|RouteLastSeen |
|  |   t=7.5 AcquireWeapon -> GoToWeapon : no plan for Survive |
|  |   t=11.3 Patrol -> Search : Patrol u=0.10 facts=HasWeapon|Loaded|LowHealth|HasPrimary|RouteLastSeen |
| F_close_utilities | legacy | 2 | 0 | 0 | 0.0 | - | - | 2 | 103 | 58 | 0 | 0.05 | 0.13 |
| F_close_utilities | planner | 2 | 0 | 0 | 0.0 | - | - | 2 | 103 | 58 | 0 | 0.57 | 1.49 |
|  | legacy: Attack 0.0-2.2; Search 2.2-12.1;  |
|  | planner: Attack 0.0-2.2; Search 2.2-12.1;  |
|  |   t=0.0 KillEnemy -> Attack : KillEnemy u=0.78 facts=HasWeapon|Loaded|EnemyVisible|EnemyKnown|CoverAvail|HasPrimary|RouteLastSeen|FlankAvail|AltLoaded |
|  |   t=2.2 Patrol -> Search : Patrol u=0.10 facts=HasWeapon|Loaded|CoverAvail|UnderFire|HasPrimary|RouteLastSeen|FlankAvail|AltLoaded |
| G_trap_pocket | legacy | 2 | 0 | 0 | 15.1 | - | - | 6 | 23 | 94 | 0 | 0.05 | 0.24 |
| G_trap_pocket | planner | 3 | 0 | 0 | 6.1 | - | - | 6 | 23 | 94 | 0 | 0.60 | 4.66 |
|  | legacy: Pursue 0.0-4.5; Roam 4.5-15.1;  |
|  | planner: Attack 0.0-0.5; Pursue 0.5-6.6; Unstick 6.6-15.1;  |
|  |   t=0.0 KillEnemy -> Attack : KillEnemy u=0.90 facts=HasWeapon|Loaded|EnemyVisible|EnemyKnown|HasPrimary|RouteLastSeen|AltLoaded|RoamAvail |
|  |   t=0.5 Investigate -> Pursue : Investigate u=0.60 facts=HasWeapon|Loaded|EnemyKnown|UnderFire|HasPrimary|RouteLastSeen|AltLoaded|RoamAvail |
|  |   t=6.6 Unstick -> Unstick : Unstick u=1.00 facts=HasWeapon|Loaded|EnemyKnown|HasPrimary|RouteLastSeen|Stuck|AltLoaded|RoamAvail |
| H_empty_clip_in_fight | legacy | 2 | 0 | 0 | 0.0 | - | - | 30 | 103 | 70 | 0 | 0.05 | 0.10 |
| H_empty_clip_in_fight | planner | 7 | 3 | 0 | 0.0 | - | - | 54 | 102 | 46 | 0 | 0.62 | 2.01 |
|  | legacy: Attack 0.0-4.2; Search 4.2-12.1;  |
|  | planner: ChangeWeapon 0.0-0.1; Attack 0.1-1.3; Reload 1.3-3.3; Attack 3.3-4.5; Reload 4.5-6.5; Attack 6.5-7.5; Search 7.5-12.1;  |
|  |   t=0.0 KillEnemy -> ChangeWeapon : KillEnemy u=0.90 facts=HasWeapon|EnemyVisible|EnemyKnown|CoverAvail|HasPrimary|RouteLastSeen|FlankAvail|AltLoaded |
|  |   t=0.1 KillEnemy -> Attack : KillEnemy u=0.90 facts=HasWeapon|Loaded|EnemyVisible|EnemyKnown|CoverAvail|UnderFire|HasPrimary|RouteLastSeen|FlankAvail |
|  |   t=1.3 KillEnemy -> Reload : KillEnemy u=0.87 facts=HasWeapon|EnemyVisible|EnemyKnown|CoverAvail|UnderFire|HasPrimary|RouteLastSeen|FlankAvail |
|  |   t=3.3 KillEnemy -> Attack : KillEnemy u=0.83 facts=HasWeapon|Loaded|EnemyVisible|EnemyKnown|CoverAvail|UnderFire|HasPrimary|RouteLastSeen|FlankAvail |
|  |   t=4.5 KillEnemy -> Reload : KillEnemy u=0.80 facts=HasWeapon|EnemyVisible|EnemyKnown|CoverAvail|UnderFire|HasPrimary|RouteLastSeen|FlankAvail |
|  |   t=6.5 KillEnemy -> Attack : KillEnemy u=0.76 facts=HasWeapon|Loaded|EnemyVisible|EnemyKnown|CoverAvail|UnderFire|HasPrimary|RouteLastSeen|FlankAvail |
|  |   t=7.5 Patrol -> Search : Patrol u=0.10 facts=HasWeapon|Loaded|CoverAvail|UnderFire|HasPrimary|RouteLastSeen|FlankAvail |
| I_visibility_flicker_pistol | legacy | 7 | 4 | 4 | 0.0 | - | - | 9 | 102 | 91 | 0 | 0.05 | 0.13 |
| I_visibility_flicker_pistol | planner | 9 | 2 | 0 | 0.0 | - | - | 8 | 102 | 92 | 0 | 0.74 | 2.27 |
|  | legacy: Pursue 0.0-1.5; GoToWeapon 1.5-3.1; Pursue 3.1-4.6; GoToWeapon 4.6-6.1; Pursue 6.1-7.4; GoToWeapon 7.4-11.5; Search 11.5-12.1;  |
|  | planner: Attack 0.0-1.2; ChangeWeapon 1.2-1.3; Attack 1.3-1.5; Pursue 1.5-3.1; Attack 3.1-4.1; Reload 4.1-4.6; Pursue 4.6-6.1; Attack 6.1-7.1; GoToWeapon 7.1-12.1;  |
|  |   t=0.0 KillEnemy -> Attack : KillEnemy u=0.90 facts=HasWeapon|Loaded|EnemyVisible|EnemyKnown|UpgradeReach|RouteLastSeen|AltLoaded |
|  |   t=1.2 KillEnemy -> ChangeWeapon : KillEnemy u=0.89 facts=HasWeapon|EnemyVisible|EnemyKnown|UnderFire|UpgradeReach|RouteLastSeen|AltLoaded |
|  |   t=1.3 KillEnemy -> Attack : KillEnemy u=0.89 facts=HasWeapon|Loaded|EnemyVisible|EnemyKnown|UnderFire|UpgradeReach|RouteLastSeen |
|  |   t=1.5 Investigate -> Pursue : Investigate u=0.60 facts=HasWeapon|Loaded|EnemyKnown|UnderFire|UpgradeReach|RouteLastSeen |
|  |   t=3.1 KillEnemy -> Attack : KillEnemy u=0.89 facts=HasWeapon|Loaded|EnemyVisible|EnemyKnown|UpgradeReach|RouteLastSeen |
|  |   t=4.1 KillEnemy -> Reload : KillEnemy u=0.88 facts=HasWeapon|EnemyVisible|EnemyKnown|UnderFire|UpgradeReach|RouteLastSeen |
|  |   t=4.6 Investigate -> Pursue : Investigate u=0.60 facts=HasWeapon|EnemyKnown|UnderFire|UpgradeReach|RouteLastSeen |
|  |   t=6.1 KillEnemy -> Attack : KillEnemy u=0.88 facts=HasWeapon|Loaded|EnemyVisible|EnemyKnown|UpgradeReach|RouteLastSeen |
|  |   t=7.1 AcquireWeapon -> GoToWeapon : AcquireWeapon u=0.40 facts=HasWeapon|Loaded|UnderFire|UpgradeReach|RouteLastSeen |

Randomised stress (15 bots, 10 Hz, 60 s, 9000 decisions; observations change every tick, so this is a replanning worst case):
- planner 1.05 us/decision mean, legacy model 0.07 us/decision; 15 bots at 10 Hz = 0.158 ms per second of game for the planner
- max planner nodes expanded in one decision: 4 (budget 256)
