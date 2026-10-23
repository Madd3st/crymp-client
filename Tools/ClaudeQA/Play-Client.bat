@echo off
title WarsBots TEST CLIENT
set /p GAME=<"%~dp0game-dir.txt"
set BIN=%USERPROFILE%\WarsBotsBuild\crymp-multibot\build\multibot-on
start "" /D "%GAME%" "%BIN%\CryMP-Client64.exe" -dir "%GAME%" -dx9 -logfile WarsBots-Play-Client.log +connect 127.0.0.1:64130
