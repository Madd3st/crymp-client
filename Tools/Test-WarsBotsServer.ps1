[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$GameDirectory,
    [Parameter(Mandatory)][string]$ServerExe,
    [Parameter(Mandatory)][ValidateSet('run','sprint','left','right','normal','strength')][string]$Case,
    [int]$Port = 64120,
    [int]$Trial = 1,
    [double[]]$SpawnPosition = @()
)
$ErrorActionPreference = 'Stop'
$qaRoot = Split-Path $PSScriptRoot -Parent
$qaDir = Join-Path $qaRoot 'build/server-qa'
New-Item -ItemType Directory -Force $qaDir | Out-Null
$qaName = "WarsBots-CryMP-QA-$Case-$Trial-$([DateTime]::Now.ToString('HHmmssfff')).log"
$qaLog = Join-Path ([Environment]::GetFolderPath('MyDocuments')) "My Games/Crysis/$qaName"
$qaCommand = switch ($Case) {
    'run' { '+wb_bot_gait run' }
    'sprint' { '+wb_bot_gait sprint' }
    'left' { '+wb_bot_strafe left' }
    'right' { '+wb_bot_strafe right' }
    'normal' { '+wb_bot_jump normal 3' }
    'strength' { '+wb_bot_jump strength 3' }
}
$qaSpawn = if ($SpawnPosition.Count -eq 3) { ($SpawnPosition | ForEach-Object { $_.ToString([cultureinfo]::InvariantCulture) }) -join ' ' } else { '' }
$qaArgs = "-dir `"$GameDirectory`" -headless -logfile $qaName +sv_bind 127.0.0.1 +sv_lanonly 1 +sv_port $Port +sv_gamerules InstantAction +map multiplayer/ia/steelmill +wb_probe_local_test 1 +wb_bot_verbose 1 +wb_bot_start $qaSpawn $qaCommand"
$qaProcess = Start-Process -FilePath (Resolve-Path -LiteralPath $ServerExe).Path -ArgumentList $qaArgs -WorkingDirectory $GameDirectory -WindowStyle Hidden -PassThru
$qaDeadline = [DateTime]::UtcNow.AddSeconds(90)
$qaText = ''
$qaPass = $false
$qaReason = 'Timed out waiting for actual physical evidence'
try {
    while ([DateTime]::UtcNow -lt $qaDeadline) {
        if (Test-Path -LiteralPath $qaLog) { $qaText = Get-Content -LiteralPath $qaLog -Raw }
        if ($qaText -match 'CRASH DETECTED') { $qaReason='Engine crash'; break }
        if ($Case -in @('normal','strength')) {
            $qaStrength = if ($Case -eq 'strength') { 1 } else { 0 }
            $qaPass = $qaText -match "JUMP TAKEOFF: strength=$qaStrength " -and $qaText -match "JUMP LANDING: strength=$qaStrength "
            if ($qaText -match 'ACTION REFUSED: ([^\r\n]+)') { $qaReason=$Matches[1]; break }
        } else {
            foreach ($qaLine in ($qaText -split "`n")) {
                if ($Case -in @('left','right')) {
                    if ($qaLine -match 'MOTION INTENT: gait=1 strafe=1 requestedBase=[\d.]+ actual=([\d.]+) lateral=(-?[\d.]+) facingDot=(-?[\d.]+)') {
                        $qaActual=[double]::Parse($Matches[1],[cultureinfo]::InvariantCulture)
                        $qaLateral=[double]::Parse($Matches[2],[cultureinfo]::InvariantCulture)
                        $qaFacing=[double]::Parse($Matches[3],[cultureinfo]::InvariantCulture)
                        $qaDirection = if ($Case -eq 'left') { $qaLateral -lt -0.8 } else { $qaLateral -gt 0.8 }
                        if ($qaActual -gt 1 -and $qaDirection -and $qaFacing -gt 0.85) { $qaPass=$true }
                    }
                } elseif ($qaLine -match 'ABILITY PHYSICS: speed=([\d.]+).*sprint=(\d).*mode=(\d).*energy=([\d.]+)') {
                    $qaSpeed=[double]::Parse($Matches[1],[cultureinfo]::InvariantCulture)
                    $qaEnergy=[double]::Parse($Matches[4],[cultureinfo]::InvariantCulture)
                    if ($Case -eq 'run' -and $qaSpeed -gt 2 -and $Matches[2] -eq '0') { $qaPass=$true }
                    if ($Case -eq 'sprint' -and $qaSpeed -gt 3.5 -and $Matches[2] -eq '1' -and $Matches[3] -eq '0' -and $qaEnergy -lt 199) { $qaPass=$true }
                }
            }
        }
        if ($qaPass) { $qaReason='Actual physical evidence observed'; break }
        if ($qaProcess.HasExited) { $qaReason="Server exited ($($qaProcess.ExitCode))"; break }
        Start-Sleep -Milliseconds 500
    }
} finally {
    # Never terminate the user's session: this is a separate QA server/port, no clients.
    if (-not $qaProcess.HasExited -and $qaText -notmatch 'clients=[1-9]') {
        Stop-Process -Id $qaProcess.Id
        Wait-Process -Id $qaProcess.Id -Timeout 15 -ErrorAction SilentlyContinue
    }
    if (Test-Path -LiteralPath $qaLog) { Copy-Item -LiteralPath $qaLog -Destination (Join-Path $qaDir $qaName) }
}
[pscustomobject]@{Case=$Case;Trial=$Trial;PhysicalPass=$qaPass;Reason=$qaReason;Log=(Join-Path $qaDir $qaName)}
if (-not $qaPass) { throw "Physical test $Case failed: $qaReason" }
