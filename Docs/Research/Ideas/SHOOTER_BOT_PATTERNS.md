# Shooter bot patterns: fragmovies + UT3 / Quake 3 / CS / Killzone 2 / Battlefield (2026-10-25)

Author: Claude. Ideas only, our own code (no copied source: UT3/UDK EULA, Valve SDK licence, Q3 GPL vs our licence).

## 1. What the fragmovies show (sAng9 & shOz — IA jungle/Plantation-type maps; MAGUMBA — Babumba & Magic8, PS night maps)

Fragmovies are cut highlights, so they show *what good players do in a winning fight*, not averages. Seen repeatedly:

| # | Pattern | Where | For bots |
| --- | --- | --- | --- |
| 1 | Laser always on, fights at 5–20 m **from the hip**; zoom/scope almost never in close maps | both | laser done (fe08fb70); zoom only far (≥ 20 m) — keep |
| 2 | **Hold an angle from hard cover** (pillar, tree, concrete block, doorway), shoot enemies crossing open ground, step back between kills | both | needs cover/angle spots (§3 A) |
| 3 | **Never stand still in the open**: strafe + jump while shooting, constant side movement in duels | sAng9 | combat strafe exists; add jump-strafe + "don't stand in the open" |
| 4 | **Take the dead enemy's weapon/ammo right after the kill** ("You picked up SCAR"), then move on | sAng9 | pickup of drops after kills (bots ignore drops now) |
| 5 | **Chain kills fast**: after a kill the crosshair is already where the next enemy appears (pre-aim corners at chest/head height) | both | pre-aim at the next corner / last seen; look policy |
| 6 | Fight **through damage** (screen red) instead of turning away; finish the kill, then heal behind cover | sAng9 | arbiter: low-HP retreat only when losing the duel |
| 7 | **Grenades** into rooms/around corners, CQB indoors (PS bunkers/factories) | MAGUMBA | grenade throw (offhand) — PS stage |
| 8 | Vehicles as transport between bases (PS) | MAGUMBA | PS stage: drive only (owner) |

## 2. What other games' bots teach (sources at the end)

**Quake III Arena (van Waveren thesis)** — the closest in spirit (fast FFA arena):
* Layers: area awareness (navigation) → basic actions → **goals with fuzzy weights** (items/weapons valued by need: health, armor, ammo, weapon) → team AI/chat.
* **Character files** with numeric traits: aim accuracy, aim skill, reaction time, attack skill, *jumper*, *croucher*, *camper*, aggression, weapon preferences, chat style. → our continuous traits idea (UT3 has the same).
* Item **respawn timing** and **camp spots**.

**Counter-Strike bots (nav mesh data)** — per nav area the map stores **hiding spots** flagged `IN_COVER`, `GOOD_SNIPER_SPOT`, `IDEAL_SNIPER_SPOT`, `EXPOSED`, plus **encounter paths/spots** (where you meet enemies coming from area A to B) and place names. Bots pick spots by role (sniper → sniper spots, defender → cover spots near the objective) and pre-aim encounter spots. → exactly what we need for "snipers on roofs" and pattern 2/5, and we can compute it offline from our Recast mesh + collision.

**Killzone 1/2 MP bots (Verweij, Guerrilla)** — the template for **Power Struggle**:
* 3 layers: **faction commander** (picks objectives to attack/defend, how many bots each, uses only info a human has, chooses spawn points per task) → **squad leader** (concrete orders) → **individual** (fights, picks positions).
* KZ1 position picking: candidate waypoints scored by distance, **line of fire to the main threat, cover from the others, preferred range**; threat memory = 4 biggest threats with position prediction.
* KZ2: HTN planner, individuals replan ~5×/s, squads ~2×/s; short plans because the world changes fast.
* Lesson they report: KZ1 bots had almost no coordination → the commander layer is what makes team modes work.

**Unreal Tournament 3** (see UT3_BOTS.md): squad AI + objectives, path costs that learn kill zones, special moves as link annotations, adaptive skill, dodge on incoming projectiles.

**Battlefield (DICE AutoPlayers, BF V/2042)** — public material is mostly about test bots at 64-player scale and "hard to tell from humans" marketing; useful takeaways: bots must handle vehicles/seats as first-class actions, and they are evaluated at full-server scale. **Call of Duty**: no public design detail on MP bots (offline "Combat Training" since Black Ops); skip.

## 3. What to build (ordered), mapped to WarsBots

A. **Tactical spots offline** (CS hiding/sniper spots + KZ position scoring): from the navmesh + collision compute per area
   cover spots (a wall at chest height in ≥ 1 direction), sniper spots (high, long sightlines over hotspots/spawns),
   exposed spots; store next to `<map>.navmesh`. Runtime: style picks spots (Marksman/Defender hold; Assault/Flanker move
   through), score = distance + line of fire to the threat + cover from others + preferred weapon range (KZ1).
   → snipers/Gauss on roofs (owner), angle holding (pattern 2).
B. **Encounter / pre-aim**: precompute where routes into an area become visible; bots moving or holding pre-aim those
   points at chest height (patterns 5, 2). Uses the look policy we have.
C. **Combat movement**: strafe + jump-strafe by trait "jumper"; no standing in the open while shooting (pattern 3);
   fight-or-retreat by duel state, not raw HP (pattern 6).
D. **Loot after kills**: dropped weapons/ammo near the kill are pickups (pattern 4).
E. **Traits** (Q3/UT3 character files): aim, reaction, jumper, croucher (prone only for elite marksman), camper,
   aggression, favourite weapon → one profile struct feeding all modules; names show level (done).
F. **Owner-learned links and spots**: from the owner's demos — his holding spots, routes, and the Gauss-tower
   Speed+Strength+circle-jump entry → new link kinds and spot lists.
G. **Power Struggle** (Shore, Plantation; vehicles = driving only): commander (objectives, squads, spawns) → squad →
   individual, KZ-style; economy (buy weapons/vehicles), capture/defend factories and bunkers, drive between bases;
   navmesh bake for both maps first.

## Sources
* van Waveren, *The Quake III Arena Bot* — https://www.cs.rochester.edu/u/brown/242/docs/QuakeIII.pdf
* Valve NAV file format (hiding spots, encounter paths) — https://developer.valvesoftware.com/wiki/NAV_(file_format)
* Verweij, *A Hierarchically-Layered Multiplayer Bot System for a First-Person Shooter* (Killzone 2) —
  https://www.guerrilla-games.com/read/a-hierarchically-layered-multiplayer-bot-system-for-a-first-person-shooter
* Killzone AI (GDC Europe 2005) — https://www.guerrilla-games.com/media/News/Files/gdce05_killzone_ai.pdf
* Gillberg, *AI for Testing: Bots that Play Battlefield V* (GDC 2019) — https://www.gdcvault.com/play/1025905
* UDK AI overview — https://docs.unrealengine.com/udk/Three/AIOverview.html
