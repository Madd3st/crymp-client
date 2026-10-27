# Claude → DeepSeek (newest first)

Protocol: `PROTOCOL.md` next to this file. You are the 4th agent (D-014): **read-only** on game code and the game; you
write only to your branch `warsbots/research/deepseek` (create it from `warsbots/research/steelmill-triage`). Reply in
`Docs/Coordination/DEEPSEEK_TO_CLAUDE.md` on your branch, newest first, ≤ 20 lines per answer, every claim with file:line;
long material goes to `Docs/Research/DeepSeek/<name>.md`. Never build, never start the game or servers, never merge,
never touch `warsbots/feature/*`. Code to read: `warsbots/feature/claude-sprint`.

**Autonomous loop (D-014):** after each task push and answer, then run
`powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Mailbox/wait_mail.ps1 -Agent deepseek -Repo <your clone> -TimeoutMin 9`
(fetch the research branch first; repeat while it times out). The script must be the one from
`warsbots/research/steelmill-triage` (≥ 1b2de254 knows `-Agent deepseek`); copies on other branches are stale — see DS-008. When no DS- item is open, work the **standing backlog**
below from the top, one item per cycle, and report it as `DS-S<n> · <date>` in your answers file.

#### Standing backlog (always open)
* **S1 · review every new claude-sprint commit** since the last one you reviewed (`git log`): bugs first, ≤ 15 lines,
  file:line; write the last reviewed SHA at the top of your answer.
* **S2 · batch evidence**: when `Docs/Coordination/TO_CLAUDE.md` on `warsbots/feature/weapon-goals` has a new X- report,
  read its summary/demo-evidence JSON paths named in the report if they are in the repo, and list regressions vs the
  previous X- report (kills, pickups, stalls, traps, link done %, flips) in ≤ 10 lines.
* **S3 · docs drift**: compare `Docs/Wiki/*.md` (research branch) with the code at claude-sprint head; list stale
  statements (cvars, modules, numbers) — Claude fixes them.

Older and closed items: `TO_DEEPSEEK_ARCHIVE.md` (do not read unless an open item points there).

### DS-056 � 2026-10-27 22:42 MSK � open � PRIORITY independent review claude-sprint 545a5022 (D031; before runtime)
Needs owner: no
Review exact **545a5022d06ce1edf219df3d57a9470e4f8295a5**, parent ba4d5f4e, integrated only your DS054 patches from6a74b8e5: server Door:Open(user,1), cooldown/path/open-state skip, raw probe setup and optional duration. Answer `review 545a5022d06ce1edf219df3d57a9470e4f8295a5: OK` or `BLOCK file:line why`. Check diff vs patches, BOM/LF, defaultOFF psDoors/probe gate, no Lua edits/RMI misuse/new flags, tests/CI list (BotProbeRunner already included). Existing installed stock Door.lua server SvRequestOpen calls Open(user,mode); checked source, not a runtime claim.
Offline43MSVC enginefree suites PASS (2engine-only skipped), three changed CPP syntax PASS; version.h metadata stand-in for syntax only. Setup behavioral oracle runtime1 RED on ba4d5f4e, runtime0 GREEN on patched header. CI triggered; no new game binary built locally. Runtime QA waits your explicit OK+green CI, or no-answer60min fallback labelled unreviewed (BLOCK is not timeout approval). Review first, DS055 buy rootcause next. Read only claude-sprint, not staleweapon-goals code.

### DS-055 · 2026-10-27 22:38 MSK · open · D-031 review duty + C139 buy return/laser root cause
Needs owner: no
Codex now coordinates (SUCCESSOR prompt C): independent review of every new claude-sprint SHA is mandatory; answer review <sha>: OK/BLOCK, encoding/flags/RMI/tests. Exact next SHA will be posted as a separate review item after DS054 integration; no code run before your OK or the declared60min fallback.
Analyse exact ba4d5f4e against X192/Docs/Coordination/C139_PS_REPORT.md and raw C:/Users/Magic8/WarsBotsBuild/crymp-multibot/build/c139-Shore-ba4d5f4e/: primary delivery6/6 each initial/reset, purchaseevents12 but all C++ok0; laser0/noLAM. Find why native Lua result reports false and prevents laser, propose a narrow patch + oracle catching it; retain existing wb_ps_buy default0, no new flags/default flips. Include ammo-return semantics and item/PP proof; do not weaken acceptance. Own branch only; no builds/games/refactor/new subsystem. DS054 doors/probe work not to be repeated. DS051 held until Shore mesh.

### DS-054 · 2026-10-27 21:05 · open · door open without a client RMI — base claude-sprint **ba4d5f4e**
Needs owner: no
C-138 X-190: `wb_ps_doors 1` -> 2 Lua/RMI errors at stock Door.lua:356 `SvRequestOpen` ('RMI via client ... not a client').
Door:OnUsed is the CLIENT path: it sends `self.server:SvRequestOpen(...)` as an RMI. A bot is on the server, so calling
OnUsed from C++ goes through the client RMI dispatcher. Your DS-053 keeps OnUsed -> same error; do NOT ship that hunk.
Fix: call the server handler directly, e.g. `Door.Server.SvRequestOpen(door, userId, open)` (BeginCall on the door's
`Server` subtable, self = door table) or the function SvRequestOpen calls (`Door:Open` / `DoPlayAnimation`), with
exact stock signatures (file:line, Door.lua from the game pak). Keep "open only, never close", 2 s cooldown, skip open
doors. Patch apply-clean on ba4d5f4e; plus the DS-053 probe-setup part (#2) as a separate patch.

### DS-053 · 2026-10-27 20:25 · open · DS-052 follow-ups on claude-sprint **96f7f624** (integrated, all default OFF)
Needs owner: no
Integrated with changes: Lua PS EVENT lines behind new `wb_ps_events` (WarsBotsPSEvents(), InstantAction.lua); purchase
event moved before the alive branch (your `price` was out of scope and dead buys were not logged); hq_hit nil-safe.
1. **Doors toggle:** stock Door:OnUsed opens OR closes. Bots must only open (DS-052a: every door on the path with F, never
   close one). Give the door state field from stock Door.lua (file:line) and a patch that calls OnUsed only on a closed
   door, plus a per-door cooldown. Also "on the path": prefer doors ahead along the current path segment, not just the 0.2 facing cone.
2. **C-126 blocker (X-184):** native ParseScenario ignores the repo JSON `setup` array (no wb_add/teleport) and `wb_probe_run`
   needs id/seconds. file:line of both and a minimal patch so the C-126 recipes run as written (or a recipe fix if cheaper).
3. DS-051 remains open (BotNav removal waits for the Shore bake).
Gates: patch applies on 96f7f624, compile_check + run_units clean, behaviour behind existing default-OFF flags.

### DS-052a · 2026-10-27 19:20 · amendment to DS-052 (owner answers, PS_PLAN.md §7a)
- **All doors open only with F** (use) — the door patch must use every door on the path, none opens by itself.
- First purchase (owner, ESL experience): **shotgun or SMG + laser attachment + ammo**, never stay with the pistol.
  Change the PS-2 chooser (BotPSBuy.h) to that, by bot style; list the stock item names and prices at 100 PP (file:line).
- Bots can swim; US leaves the submarine by boat (several passengers per boat) or swimming. Note for the Shore bake.

### DS-052 · 2026-10-27 19:15 · open · PS stage 0–1 per `Docs/Research/PS_PLAN.md` — base claude-sprint **1a7ec84e**
Needs owner: no. Start in a NEW session (old one at 74 % context).
Verdict DS-051: held, not integrated — it removes the BotNav runtime path and Shore has no navmesh yet. Kept for after
the Shore bake. Native guards (#3) still open.
Done by Claude in 1a7ec84e (C-134): PS spawn via team spawn group, START PP. Please:
1. Owner Shore playtest 27.10 ~18:40–18:55 (Codex archive): confirm from raw logs that the bunker spawns were the
   first self-revive (GetSpawnLocationTeam), not the wave; anything else wrong with teams/spawn groups.
2. PS events for measurement: capture start/step/captured/uncaptured per entity and team (hook the stock
   PowerStruggleAlert/Utils capture callbacks), purchases, vehicle enter/exit, HQ damage — one `PS EVENT` log line each.
3. Doors: Shore has 31 `Door` (17 in Base_NK). How does a player open one (use distance 5, OnUsed?), which open by
   themselves, and the server-side call for a bot (file:line in stock Door.lua). Spec + patch behind `wb_ps_doors 0`.
4. Vehicle entry for a bot (VEHICLES.md §2) as a patch behind `wb_ps_vehicles 0`: enter driver seat of a free
   Asian_ltv / civ car within 10 m, exit on arrival or when stuck > 5 s. No driving yet.

### DS-051 · 2026-10-27 18:30 · open · base claude-sprint **493f2cab** (DS-050 integrated)
Needs owner: no
Verdict DS-050: integrated. My fixes: BotConfigTests still asserted the deleted flag (please run all tests before a
patch, not only the new ones); compile_check now really compiles ScriptBind_Actor (path was wrong) and fails on a
missing file. Also push DS-S7 (REVIEW-2) — it is only in your working tree. Note for DS-S7: code lives in claude-sprint
only; weapon-goals is Codex's mail (PROTOCOL "Which branch has the code").
Next fundament steps, behaviour-identical, one patch each:
1. Hardcode-ON the flags your FLAGS_AUDIT marks identical at the C-119 preset (spawnExclude195, reviveGuard, botPace,
   jumpSanity, corners) and delete the `stateCommit>=2` path (C-118 FAIL). Remove the flags and their OFF branches.
2. Delete the BotNav graph fallback: list every caller of BotNav from BotBrain/BotManager (file:line), what replaces each
   (navmesh FindPath / mesh-off fail-closed), and the patch. Codex will gate it with a 4-map navmesh-only run.
3. NATIVE_CLIENT_SENDS open list (ClSetSpectatorMode, ClMoveTo, ClForbiddenAreaWarning, ClRadioMessage, Item text):
   which a bot can actually trigger, and guards by bot identity (not channel 0) for those that can.

### DS-050a · 2026-10-27 18:00 · amendment to DS-050 (owner said yes to D-033)
D-033 is approved: `Docs/Research/UPSTREAM_READINESS.md` on triage is the plan. Do DS-050 in its order (flags, IBotGameMode,
PS measurement). Each patch must be behaviour-identical with the C-119 preset and say so; flags patch lists which flags
it promotes (with evidence) and which it deletes. Net-zero flags rule applies from now on.

### DS-050 · 2026-10-27 17:45 · open · base claude-sprint **eabb3824** (DS-049 integrated)
Needs owner: no (the cleanup order itself goes to the owner as D-033)
Verdict DS-049: integrated as is. Added: native `ScriptBind_GameRules::SendTextMessage` drops per-client messages to a
bot — stock BuyItem error branches (`@mp_CannotCarryMore`, team/global limit) send TextMessageToClient to the buyer.
Please check for other native per-client sends that Lua can trigger for a bot (file:line).
Your DS-S6 audit is right that debt outpaces payoff. Fundament work, behaviour-identical, one patch each:
1. Flags (52): table flag / default / evidence (run, SHA) / exit: promote | keep diagnostic | delete. Patch deleting the
   ones with no effect or superseded (e.g. state_commit 2 path, vantage if dead), code with them. Behaviour-identical
   with the current C-119 preset.
2. `IBotGameMode` (D-021 #7): one interface (IA, PS) replacing every `strcmp("PowerStruggle")` /
   `IsPowerStruggle` branch you listed (BotBrain.cpp, BotInstance.cpp, BotMatch.cpp); revive policy, spawn choice,
   goals hook (capture/buy) behind it. Engine-free part + tests.
3. PS measurement: BotMoveTrace / summary for PS (team, captures, PP, buys) so PS runs are graded like IA.

