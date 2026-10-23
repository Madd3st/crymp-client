@echo off
title WarsBots BUILD ON
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Claude-Build-Multibot.ps1" -OnOnly
pause
