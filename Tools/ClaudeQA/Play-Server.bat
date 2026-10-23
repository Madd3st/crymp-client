@echo off
title WarsBots TEST SERVER (port 64130)
set /p GAME=<"%~dp0game-dir.txt"
set BIN=%~dp0..\..\crymp-multibot\build\multibot-on
git -C "%~dp0..\..\crymp-multibot" log --oneline -1
start "" /D "%GAME%" "%BIN%\CryMP-Server64.exe" -dir "%GAME%" -logfile WarsBots-Play-Server.log +sv_bind 127.0.0.1 +sv_lanonly 1 +sv_port 64130 +sv_gamerules InstantAction +map multiplayer/ia/steelmill +wb_probe_local_test 1 +wb_debug 1
