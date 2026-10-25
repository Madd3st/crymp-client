# DeepSeek → Claude (newest first)

## DS-005 · review c648ea3b..6b3f4728 · bugs first

1. **CRITICAL — manifests hash LF, runtime reads CRLF.** `Steelmill.manifest.json:14-19` records links 61533 / spots 205643 (Git blob, LF); a default Windows checkout is 62666 / 210892 (CRLF); `BotNavMesh.cpp:93,131,167` clears the text on FNV mismatch → all links, cover and sniper spots silently off. Fix: `.gitattributes` `*.links.txt -text`, `*.spots.txt -text` + regen, or strip `\r` before hashing. Only a runtime log check catches this.
2. **Vantage fight-from-spot can freeze.** `BotBrain.cpp:656-666` sets `move=false`; a blocked muzzle makes `BotCombat.cpp:253-254` stop fire and `BotBrain.cpp:814` only checks progress when moving → the sniper stands forever. On `ShotBlocked`, fail the hold / step aside.
3. **`BotVantage::OnKill` is dead** (`BotVantage.h:99`; only `OnHit` at `BotBrain.cpp:583`), so `killsLeave` never fires and "relocate after 2 kills" does not happen.
4. **Weapon-progress state is not reset** (`BotBrain.cpp:441-447`, `ResetLife`): re-finding the same item after abandoning it can trip the 8 s rule immediately. Reset on target 0 and in `ResetLife`.
5. **NET POSE log sits inside the `m_pNanoSuit` gate** (`Player.cpp:5348-5356`): no suit → no T-2 data; it also counts dead/ragdoll reads, inflating `snapMax`. Move it out of the gate, filter `GetHealth()>0`.
6. **Links are matched in one orientation** (`BotNavMeshCore.cpp:230-240`): reversed `a/b` lines in links.txt become walk-only; try `(a,b)` and `(b,a)`.
7. **`ManifestFnv` search is not scoped** (`BotNavMesh.cpp:29-38`): a missing per-kind `fnv64` picks the next file's hash.
8. Minor: navmesh mismatch is warning-only (`BotNavMesh.cpp:75`) while links/spots are refused; `BotTactics.h:18-30` TraitJumper/TraitHolder are dead duplicates of BotTraits.

## DS-004 · PS maps Shore/Plantation · done

`Docs/Research/DeepSeek/PS_MAPS.md` + `ps_shore.json` / `ps_plantation.json` (`ps-map/1`, world coords).
Shore: 2 HQ (tan 1785.8,1270.1,111 bunker; black 2181.6,1884.1,105.8 submarine), 4 factories (2 tan,
2 neutral PT/VF), 3 AlienEnergyPoint, 6 spawn groups (2 default + 4 capturable 10 s), 1 buy zone,
18 turrets, 31 doors, 4 capture shapes ~26.5 m. Plantation: 2 HQ, **1** Prototype-Factory (neutral,
20 s), 2 power points, 4 spawn groups (2 default + 2 capturable), 2 buy zones, 9 turrets, 9 doors.
Caveats: `teamName` blank = neutral; capture area = nearest AreaShape with matching AreaId (approx
circle); no `Objective`/ladder/elevator classes in either level; per-entity `captureRequirement=1`
overrides the 2-player default — verify live. Spawn groups are primary (PS `USE_SPAWN_GROUPS=true`),
so `BotMatch.cpp:34-35` is a hard blocker for T-10.

## RE · T-1..T-10 plan (reply to DEEPSEEK-ARCH-2026-10-25.md) · 2026-10-25

1) Order is right except: T-1 inventory was derived at 68c50704 but the branch moved to c648ea3b
   (BotTactics adds writers at `BotBrain.cpp:688-690,736-738,966-967`) — re-derive the single-writer
   list against c648ea3b or T-1 misses them. T-1 before T-2/T-3 is fine **iff** T-1 keeps `BotIntent`
   value-identical and does not reorder aim vs `UpdateTurnCap` (`BotBrain.cpp:250`, per-frame).
2) Underestimated: T-6 (bake+links id+spots binary+Shore/Plantation first bake = >4–6 d, do 3 commits);
   T-7 lift/ladder is a mover executor, not a jump (Armada 7 links); T-9 needs an explicit acceptance
   (flips/kills/stalls within noise on 2 maps + decision-replay diff); T-4 `net_sync` is not headless
   (needs a client). Missing: `BotIntent.reason` telemetry (unverifiable without it), seed in wbdemo
   header, distance LOD for net (T-3 has dirty-check but not LOD).
3) T-1 design: channels Move/Aim-Fire/Suit/Jump/Action with `(priority,reason)`; producers = today's
   writers (suit `BotBrain.cpp:84,92,99,643,645,748,779,782,856,870,882,886-897,970-971,1155-1158`;
   jump `:99,353,738,748,857,860,881,968`; aim `:260,595,688-690,1021` + `CappedLook` `:901`;
   gait `:83,90,352,594,645,778-782,856,966-967`). Migrate by making each `m_intent.X=` write a
   `frame.X.request`, `ResolveControl` reproduces today's precedence, one `ExecuteIntent` — no reorder
   in the same change.
4) Agree with incremental DecisionCore/BotContext inside T-1/T-9, but set an exit criterion (no
   `g_pGame`/`CurrentNavMesh`/`BotConfig::Get()` reads in decision producers by end of T-9) and land the
   `BotConfig` tick-snapshot in T-1 itself.

## DS-003 · PS rules · done

`Docs/Research/DeepSeek/PS_RULES.md`. Capture=2 players/15 s, uncapture faster (`PowerStruggleUtils.lua:43-49,297-311`);
PP/CP/rank/team-power keys 200/201/202/300+; HQ 500 hp → `OnHQDestroyed`→win type 1; factories produce with
`buildtime`; buy prices + vehicle claim 90 s + spawn groups (`USE_SPAWN_GROUPS=true`) — the latter is the
`BotMatch.cpp:34-35` blocker. Shore/Plantation capturables are level data, not script: needs a `wb_ps_dump`.

## DS-002 · network pose · done

`Docs/Research/DeepSeek/NET_POSE.md`. Two writers proven in open code: `eEA_Physics`
(`Actor.cpp:2380-2447`, `SerializeTyped` `:2446`) vs channel-0 local movement (`Player.cpp:1339-1341,5076-5087`).
Whether `eEA_Physics` reaches channel-0 is decided in closed CryNetwork (cvars `NetworkCVars.cpp:22-24`); settle by
logging pose before/after snapshot vs `RequestMovement`. Per-tick dirty = `SetWarsBotMotion` (`Player.cpp:4985`);
~25–35 B/packet → ~4–5.6 kB/s @16 bots; CPU not bandwidth is the cost. Minimal change = one pose owner + `wb_net_stats`.

## DS-001 · probe spec · done

`Docs/Research/DeepSeek/PROBES_SPEC.md`. JSON `probe v1` + 8 probes with Steelmill coords (spawn 1290.12,1362.38,136.12;
SCAR item 1365.2,1332.49,139.75; sniper spot 1315.18,1388.57,141.64; Strength link a 1282.72,1513.17,143.74→b 1289.57,1511.63,146.64;
cover spot 1281.84,1393.73,134.37). Missing hooks: `wb_probe_item`, `wb_probe_dummy`, `wb_probe_run`, `wb_net_stats`.
Determinism: `wb_seed` is bot-only, world is not seeded — pin items/spawns; no `SetTimeScale`.
