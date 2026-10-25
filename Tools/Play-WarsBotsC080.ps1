# C080 owner preset. PrepareOnly writes launch script; never launches a game in that mode.
[CmdletBinding()]
param([string]$BuildDirectory='build/play-b521feeb',[ValidateRange(1,15)][int]$BotCount=8,[switch]$PrepareOnly)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'Play-WarsBotsSprint.ps1') -BuildDirectory $BuildDirectory -BotCount $BotCount -ClientTraceMode 2 -CornersAndJumpSanity -PrepareOnly:$PrepareOnly
