@echo off
title Claude WarsBots runner
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Claude-Runner.ps1" >> "%~dp0runner-console.log" 2>&1
