# P0 iteration 1: trigger control and TTK diagnostics, 2026-10-24

Game code `2fcb4c0f`, branch `warsbots/feature/weapon-goals`.
[Machine-readable verification](fire-control-2026-10-24.json).
This is a compiled, unit-tested candidate, **not a server/visual-validated replacement**
for owner-tested `5e8daa65`. P0 remains open; spectator/map export/ML not started.

## Cause and minimal correction

`CSingle::StartFire` fires once; its auto-fire helper requires native IsPlayer=true.
Owned channel-0 actors have IsPlayer=false. Old BotCombat retained the trigger until
profile burst end and only pressed again at the style/skill burst-gap deadline,
regardless of single/pump/automatic fire mode. This explains an avoidable firing-policy
limitation; the owner's approximately one-second pistol rhythm is not yet measured.

New BotFireControl reads actual IFireMode::GetType/GetFireRate, not a parallel weapon
stat table. Single/pump modes use native StartFire then StopFire per trigger press,
with a human floor .24-.38 s and native 60/RPM minimum interval. Native CanFire,
reload, ammo, readiness, bolt/pump timing remain authoritative. Spin-up modes retain
a hold long enough to avoid cancelling spin-up. Rapid/Automatic/Burst retain burst
control; all current profile burst/gap combinations retain their automatic start
intervals. Pending reaction/retarget deadlines are preserved. No damage/health/energy,
spread, ShotValidator, channel, standard player or Lua rule modification.

Candidate policy examples (unit assertions, **not observed shot intervals**):
SOCOM Single 450 RPM, Elite => >=.24 s between requests; Beginner >=.38 s.
Shotgun 80 RPM => >=.75 s; DSG1 38 RPM => >=1.579 s; Gauss 30 RPM => >=2 s.
Decision/frame timing and native acceptance can only lengthen actual intervals.
Weapon switching releases an outstanding trigger on the old weapon.

Inspected original MP weapon XML: SOCOM in GameData.pak, other five weapons in
ZPatch1.pak; SOCOM default rate450/damage20 with Single and Burst, SCAR default
rate700/damage26 with Rapid/Single, FY71 default rate640/damage32 with Rapid/Single,
Shotgun rate80/damage16, DSG1 rate38/damage125, Gauss rate30/damage350. Attachments,
patches and selected mode can change runtime parameters: these are inspected XML
values, not an assertion about every loaded loadout. Candidate diagnostics record
actual native mode/RPM/clip. Assets unchanged and not copied into Git.

## Damage anomaly investigation

SinglePlayer.lua (ZPatch1) GetDamageTable classifies actors through IsPlayer; its
PlayerToAI head multiplier is 50. Native InstantAction PvP head multiplier is 2.25.
The existing IA-only WarsBots wrapper maps marked bots to PvP already; preserved.
A **reconstructed** 26-damage head hit at x50, with HP100 and Armor energy200,
yields floor(100 - (26*50 - 200/1.5)) = -1067. This is consistent with historical
misclassification, but old impact material/suit context is insufficient to prove
that exact sequence occurred. It does not explain current rapid human deaths by itself.

Offline review of the archived 11-Elite log: 3943 paired hits on actors alive before
impact, 316 negative-health results, minimum -640; no exact -1067. Includes bot-only
tail, does not identify all human TTKs. High native Gauss overkill is not HP damage.
Do not present negative HP alone as an illegal damage multiplier.

## Opt-in actual-shot/hit trace and existing exporter

`wb_fire_diagnostics 1` (default0) enables at most 20,000 extra records per level.
CWeapon::OnShoot emits actual native shot callbacks for owned bot shooters under
WARSBOTS_EXPERIMENTAL_SPAWNER; no listener lifetime or gameplay change. Shot record:
shooter, intended target, projectile, weapon/class, actual mode/RPM/remaining clip.
Existing before/after-Lua hook adds actor-only records: identities, real connected
human marker, HP, suit mode/energy, distance, material/head-material flag, body part,
base input damage. Actual impact may hit a different actor from intended target.
Nonactor props are excluded. Authoritative diagnostics are **not policy/ML inputs**.

Tools/wb_tactics_report.py remains the exporter; additive `fire_diagnostics` schema1
section in existing schema2 report. Existing schema2 JSONL episodes unchanged.
TTK here means first registered impact (including Armor absorption) to lethal impact,
not detection/reaction time or first attempted/missed shot. Exact projectile-ID joins
can provide the launch time of the first impacting projectile; missing first shots
remain explicitly unknown. Lists actual contributing shooters and last-second
contributors, not every unseen/missing attacker. HP loss clamped to current HP;
Armor energy expenditure is separate, not HP damage. Revive/entity-done/gaps/end of
log censor episodes; unmatched pairs and cap drops reported. Max1000 TTK rows/run,
max4096 pending contexts/projectile launches. Output becomes unavailable for old
logs lacking new records, rather than fabricating TTK. No human TTK claim yet.

Diagnostics use the existing synchronous logger; default OFF and event cap limit
extra work, but nonblocking/background logging is **not** implemented. The cap does
not bound all historical general telemetry. No trained policy, action propensity or
human-demonstration infrastructure added; these raw outcomes are not bandit-ready.

## Verification and isolation

- UNIT TEST PASS: MSVC BotFireControlTests with assertions: native-rate/human floors,
  single/pump release, sniper/Gauss timing, unchanged current automatic intervals,
  unknown mode/nonfinite RPM fallback, spin-up guard.
- UNIT TEST PASS: 7 Python tests, including multi-shooter/Armor/head/overkill pairing,
  missing context, revive censorship, old-log unavailability and caps. ResourceWarning
  treated as error after fixing the file context manager.
- BUILD PASS: MSVC19.44 x64 Release experimental client/server at exact `2fcb4c0f`.
  Configure/build output redirected to `build/firecontrol-bin` using existing
  `build/multibot-on` cache. Logs `build/firecontrol-{config,final-build}.log`.
  Commands: `cmake -S . -B build/multibot-on -G Ninja -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_RC_FLAGS=/c65001 -DWARSBOTS_EXPERIMENTAL_SPAWNER=ON
  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=<checkout>/build/firecontrol-bin`, then
  `./Tools/Build-WarsBots.ps1 -Experimental -BuildDirectory build/multibot-on -Jobs 6`.
- SERVER QA / controlled 1v1 NOT RUN: owner's PID150204 remains on old play-5e8daa65
  server. A distinct port alone would still share %USER% Steelmill map memory;
  no verified independent profile exists. Do not start the existing QA runner or
  overwrite/restore shared memory while this server is active.
- CLIENT/VISUAL NOT RUN. Default-OFF not rebuilt in this increment. No new overall
  AI effectiveness or native cadence/TTK before-after measurements claimed.

Frozen candidate pair: `build/play-2fcb4c0f/`, manifest `claude-build.json` includes
source/hashes and untested runtime status. No client launched, no server stopped,
no EXE in owner session replaced. Default experimental switch remains OFF.

Next: once server resources can be isolated, native same-weapon 1v1 trials (SOCOM,
SCAR/FY71, Shotgun, DSG1/Gauss), identical resources/positions and repeats. Measure
actual native shot intervals, misses/material, true effective HP, Armor and all
contributors before adjusting close-range aim/settling or automatic bursts. Verify
trigger replication and reload/weapon switching visually afterwards. Head-choice,
tracking-error model and fire-range/distance tactics remain P0 work; do not jump to
ML or a large spectator/network refactor to avoid these unresolved checks.
