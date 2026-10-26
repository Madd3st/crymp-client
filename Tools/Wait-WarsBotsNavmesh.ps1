# Owner client may launch only after positive runtime mesh evidence for this server.
[CmdletBinding()] param([Parameter(Mandatory)][string]$Log,[Parameter(Mandatory)][int]$ServerPid,[int]$TimeoutSeconds=30)
$ErrorActionPreference='Stop'
$deadline=[DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
do {
 $process=Get-Process -Id $ServerPid -ErrorAction SilentlyContinue
 if(-not $process -or $process.HasExited){throw "Server exited before NAVMESH loaded; inspect $Log"}
 if(Test-Path -LiteralPath $Log){
  try {$text=Get-Content -LiteralPath $Log -Raw -ErrorAction Stop} catch [IO.IOException] {Start-Sleep -Milliseconds 250;continue}
  if($text -match 'NAVMESH none|NAVMESH.*(?:mismatch|FAILED|refused)'){throw "Navmesh unavailable; client launch refused. Inspect $Log"}
  $match=[regex]::Match($text,'(?m)^.*NAVMESH loaded Multiplayer/IA/Steelmill: [1-9][0-9]* polys,.*$')
  if($match.Success){return $match.Value.Trim()}
 }
 Start-Sleep -Milliseconds 250
} while([DateTime]::UtcNow -lt $deadline)
throw "Missing NAVMESH loaded Multiplayer/IA/Steelmill; client launch refused. Server retained PID $ServerPid; inspect $Log"
