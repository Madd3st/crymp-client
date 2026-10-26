param(
    [Parameter(Mandatory)][string]$Log,
    [string]$Out = "",
    [double]$FlipWindowSec = 2.0,
    [double]$ReverseGapSec = 0.6
)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path $Log)) { throw "log not found: $Log" }
if (-not $Out) { $Out = [IO.Path]::ChangeExtension($Log, '.flips.json') }

$lines = [System.IO.File]::ReadAllLines($Log)
$created = @{}          # entityId -> @{ number; skill; style }
$numberOf = @{}         # entityId -> number
$botNumbers = @{}       # number -> $true
$states = @{}           # number -> list of @{t;s}
$goals = @{}            # number -> list of @{t;g}
$targets = @{}          # actorId -> list of @{t;target}
$lastMove = @{}         # actorId -> @{t;x;y;h}
$rev = @{}              # bot number -> count (entity id falls back if unmapped)
$ownerRev = 0
$ownerFirst = $null; $ownerLast = $null
$botRev = 0
$stateConflicts = 0
$firstT = $null; $lastT = $null

foreach ($line in $lines) {
    if ($line -match '^([0-9.]+)') { $t = [double]$matches[1]; if ($null -eq $firstT) { $firstT = $t }; if ($t -gt $lastT) { $lastT = $t } } else { continue }

    if ($line -match '#(\d+) created EntityId=(\d+) name=.*? profile=(\S+)/(\w+)') {
        $n = [int]$matches[1]; $id = [uint64]$matches[2]
        $botNumbers[$n] = $true
        $numberOf[$id] = $n
        $created[$id] = [pscustomobject]@{ number = $n; style = $matches[3]; skill = $matches[4] }
        continue
    }
    if ($line -match '#(\d+) AI state=([A-Z_]+)') {
        $n = [int]$matches[1]; $s = $matches[2]
        if (-not $states.ContainsKey($n)) { $states[$n] = New-Object System.Collections.ArrayList }
        [void]$states[$n].Add([pscustomobject]@{ t = $t; s = $s }); continue
    }
    if ($line -match '#(\d+) STATE CONFLICT') { ++$stateConflicts; continue }
    if ($line -match '#(\d+) ARBITER goal=([a-z_]+)') {
        $n = [int]$matches[1]; $g = $matches[2]
        if (-not $goals.ContainsKey($n)) { $goals[$n] = New-Object System.Collections.ArrayList }
        [void]$goals[$n].Add([pscustomobject]@{ t = $t; g = $g }); continue
    }
    if ($line -match 'TARGET CHOICE actor=(\d+) old=(\d+) target=(\d+)') {
        $a = [uint64]$matches[1]
        if (-not $targets.ContainsKey($a)) { $targets[$a] = New-Object System.Collections.ArrayList }
        [void]$targets[$a].Add([pscustomobject]@{ t = $t; old = [uint64]$matches[2]; target = [uint64]$matches[3] }); continue
    }
    if ($line -match 'MOVE TRACE actor=(\d+) kind=(\w+).*vel=\(([^)]+)\).*speed=([0-9.]+)') {
        $id = [uint64]$matches[1]; $kind = $matches[2]; $v = $matches[3].Split(','); $vx = [double]$v[0]; $vy = [double]$v[1]
        $h = [math]::Sqrt($vx*$vx + $vy*$vy)
        if ($kind -eq 'human') {
            if ($null -eq $ownerFirst) { $ownerFirst = $t }; $ownerLast = $t
        } else { ++$botRev }
        if ($lastMove.ContainsKey($id)) {
            $l = $lastMove[$id]
            if (($t - $l.t) -le $ReverseGapSec -and $l.h -gt 1.0 -and $h -gt 1.0 -and (($vx*$l.x + $vy*$l.y) -lt 0)) {
                if ($kind -eq 'human') { ++$ownerRev }
                else {
                    $n = $(if ($numberOf.ContainsKey($id)) { $numberOf[$id] } else { $id })
                    if (-not $rev.ContainsKey($n)) { $rev[$n] = 0 }
                    ++$rev[$n]
                }
            }
        }
        $lastMove[$id] = [pscustomobject]@{ t = $t; x = $vx; y = $vy; h = $h }
    }
}

$sessionSec = if ($null -ne $firstT) { $lastT - $firstT } else { 0 }
$minutes = [math]::Max(0.001, $sessionSec / 60.0)
$botCount = [math]::Max(1, $botNumbers.Count)
$ownerSec = if ($null -ne $ownerFirst) { $ownerLast - $ownerFirst } else { 0 }
$ownerMin = [math]::Max(0.001, $ownerSec / 60.0)

function FlipCount($map, $field, $window) {
    $changes = 0; $flips = 0; $per = @{}
    foreach ($k in $map.Keys) {
        $arr = @($map[$k] | Sort-Object t)
        $c = 0; $f = 0
        for ($i = 1; $i -lt $arr.Count; $i++) { if ($arr[$i].$field -ne $arr[$i-1].$field) { ++$c } }
        for ($i = 2; $i -lt $arr.Count; $i++) {
            if ($arr[$i].$field -eq $arr[$i-2].$field -and $arr[$i].$field -ne $arr[$i-1].$field -and ($arr[$i].t - $arr[$i-2].t) -le $window) { ++$f }
        }
        $changes += $c; $flips += $f; $per[$k] = [pscustomobject]@{ changes = $c; flips = $f }
    }
    [pscustomobject]@{ changes = $changes; flips = $flips; per = $per }
}

$st = FlipCount $states 's' $FlipWindowSec
$gl = FlipCount $goals 'g' $FlipWindowSec
$tg = FlipCount $targets 'target' $FlipWindowSec

# ATTACKING <-> CHASING pair count
$ac = 0
foreach ($n in $states.Keys) {
    $arr = @($states[$n] | Sort-Object t)
    for ($i = 1; $i -lt $arr.Count; $i++) {
        $a = $arr[$i-1].s; $b = $arr[$i].s
        if (($a -eq 'ATTACKING' -and $b -eq 'CHASING_ENEMY') -or ($a -eq 'CHASING_ENEMY' -and $b -eq 'ATTACKING')) { ++$ac }
    }
}
$revTotal = 0; foreach ($v in $rev.Values) { $revTotal += $v }

$botRows = @()
foreach ($n in ($botNumbers.Keys | Sort-Object)) {
    $botRows += [pscustomobject]@{
        number = $n
        reversals = $(if ($rev.ContainsKey($n)) { $rev[$n] } else { 0 })
        state_changes = $(if ($st.per.ContainsKey($n)) { $st.per[$n].changes } else { 0 })
        state_flips = $(if ($st.per.ContainsKey($n)) { $st.per[$n].flips } else { 0 })
    }
}

$result = [ordered]@{
    schema = 'warsbots-flips/1'
    source = [IO.Path]::GetFileName($Log)
    session = [ordered]@{
        seconds = [math]::Round($sessionSec, 1)
        minutes = [math]::Round($minutes, 2)
        bots = $botNumbers.Count
    }
    owner = [ordered]@{
        active_seconds = [math]::Round($ownerSec, 1)
        reversals = $ownerRev
        reversals_per_min = [math]::Round($ownerRev / $ownerMin, 2)
    }
    bots = [ordered]@{
        reversals = $revTotal
        reversals_per_bot_min = [math]::Round($revTotal / ($minutes * $botCount), 2)
        state_changes = $st.changes
        state_changes_per_bot_min = [math]::Round($st.changes / ($minutes * $botCount), 2)
        state_ab_a_2s = $st.flips
        state_ab_a_2s_per_bot_min = [math]::Round($st.flips / ($minutes * $botCount), 2)
        state_conflicts = $stateConflicts
        state_conflicts_per_bot_min = [math]::Round($stateConflicts / ($minutes * $botCount), 2)
        goal_ab_a_2s = $gl.flips
        goal_ab_a_2s_per_bot_min = [math]::Round($gl.flips / ($minutes * $botCount), 2)
        target_ab_a_2s = $tg.flips
        target_ab_a_2s_per_bot_min = [math]::Round($tg.flips / ($minutes * $botCount), 2)
        target_switches = $tg.changes
        attacking_chasing = $ac
        attacking_chasing_per_bot_min = [math]::Round($ac / ($minutes * $botCount), 2)
        per_bot = $botRows
    }
}
$result | ConvertTo-Json -Depth 6 | Set-Content -Path $Out -Encoding UTF8
Write-Output "flips: $Out"
Write-Output ("state_ab_a_2s/bot-min={0} reversals/bot-min={1} attacking_chasing/bot-min={2} conflicts={3}" -f `
    $result.bots.state_ab_a_2s_per_bot_min, $result.bots.reversals_per_bot_min, `
    $result.bots.attacking_chasing_per_bot_min, $result.bots.state_conflicts)
