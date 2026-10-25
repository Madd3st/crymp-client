# Playtest report

`python3 Tools/PlaytestReport/playtest_report.py <Server.bots.log> --source <BotBrain.cpp of that build> --video <ShadowPlay mp4>... --out report.html`

One self-contained HTML page per session: per-bot table (K/D, hits, weapons held over time, pickups, time with a sniper
rifle), a top-down map of every path with problem markers and hotspots (3 m clusters), the aim error over time, which
BotBrain rule asked for each Strength jump (INTENT WHY line → function), and an event list where each event carries the
time inside the owner's video (wall clock from the ShadowPlay file name vs the DEMO START stamp).
Events: stuck ≥ 3 s outside combat, dithering (≥ 3 direction reversals in 6 s outside combat), goal flapping (≥ 4 arbiter
changes in 10 s), Strength jumps, trap recovery level ≥ 2. Read-only.
