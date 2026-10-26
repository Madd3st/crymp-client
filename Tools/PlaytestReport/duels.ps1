param(
    [Parameter(Mandatory)][string]$Log,
    [string]$Out = "",
    [double]$BurstGapSec = 0.35,
    [double]$ChoiceToShotSec = 5.0
)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path $Log)) { throw "log not found: $Log" }
if (-not $Out) { $Out = [IO.Path]::ChangeExtension($Log, '.duels.json') }

$skill = @{}            # entityId -> skill
$speed = @{}            # entityId -> m/s
$speedT = @{}           # entityId -> t
$choices = New-Object System.Collections.ArrayList
$shots = New-Object System.Collections.ArrayList
$hits = New-Object System.Collections.ArrayList
$kills = @{}            # killer skill -> count
$deaths = @{}           # victim skill -> count

foreach ($line in [System.IO.File]::ReadAllLines($Log)) {
    if ($line -match '^([0-9.]+)') { $t = [double]$matches[1] } else { continue }
    if ($line -match '#(\d+) created EntityId=(\d+) name=.*? profile=\S+/(\w+)') {
        $skill[[uint64]$matches[2]] = $matches[3]; continue
    }
    if ($line -match 'MOVE TRACE actor=(\d+).*speed=([0-9.]+)') {
        $id = [uint64]$matches[1]; $speed[$id] = [double]$matches[2]; $speedT[$id] = $t; continue
    }
    if ($line -match 'TARGET CHOICE actor=(\d+) old=(\d+) target=(\d+).*visible=(\d)') {
        [void]$choices.Add([pscustomobject]@{ t = $t; actor = [uint64]$matches[1]; target = [uint64]$matches[3]; visible = [int]$matches[4]; used = $false }); continue
    }
    if ($line -match 'FIRE SHOT shooter=(\d+) target=(\d+).*class=(\w+) mode=(\w+)') {
        [void]$shots.Add([pscustomobject]@{ t = $t; shooter = [uint64]$matches[1]; target = [uint64]$matches[2]; class = $matches[3]; delay = -1.0; burst = 0; moving = 0 }); continue
    }
    if ($line -match 'FIRE HIT phase=before-Lua target=(\d+) shooter=(\d+).*distance=([0-9.]+).*head=(\d)') {
        [void]$hits.Add([pscustomobject]@{ t = $t; shooter = [uint64]$matches[2]; target = [uint64]$matches[1]; dist = [double]$matches[3]; head = [int]$matches[4] }); continue
    }
    if ($line -match 'GameRules::KillPlayer target=(\d+) killer=(\d+)') {
        $v = [uint64]$matches[1]; $k = [uint64]$matches[2]
        if ($skill.ContainsKey($k)) { $s = $skill[$k]; if (-not $kills.ContainsKey($s)) { $kills[$s] = 0 }; ++$kills[$s] }
        if ($skill.ContainsKey($v)) { $s = $skill[$v]; if (-not $deaths.ContainsKey($s)) { $deaths[$s] = 0 }; ++$deaths[$s] }
        continue
    }
}

function Band($d) { if ($d -lt 10) { return '0-10' } elseif ($d -lt 20) { return '10-20' } elseif ($d -lt 30) { return '20-30' } elseif ($d -lt 50) { return '30-50' } else { return '50+' } }
function Median($arr) { if ($arr.Count -eq 0) { return -1 }; $s = @($arr | Sort-Object); return $s[[int][math]::Floor($s.Count/2)] }

# burst length per shooter: consecutive shots with <= BurstGapSec gaps
foreach ($g in ($shots | Group-Object shooter)) {
    $a = @($g.Group | Sort-Object t); $n = 0
    for ($i = 0; $i -lt $a.Count; $i++) {
        if ($i -eq 0 -or ($a[$i].t - $a[$i-1].t) -gt $BurstGapSec) { $n = 1 } else { $n++ }
        $a[$i].burst = $n
    }
}
# moving flag from the last MOVE TRACE within 0.5 s
foreach ($s in $shots) {
    if ($speedT.ContainsKey($s.shooter) -and ($s.t - $speedT[$s.shooter]) -le 0.5) { $s.moving = $(if ($speed[$s.shooter] -gt 1.0) { 1 } else { 0 }) }
}
# first shot after a visible target choice
$byPair = @{}
foreach ($c in $choices) {
    if ($c.visible -eq 1) {
        $key = "$($c.actor)|$($c.target)"
        if (-not $byPair.ContainsKey($key)) { $byPair[$key] = New-Object System.Collections.ArrayList }
        [void]$byPair[$key].Add($c)
    }
}
foreach ($s in $shots) {
    $key = "$($s.shooter)|$($s.target)"
    if ($byPair.ContainsKey($key)) {
        $best = $null
        foreach ($c in $byPair[$key]) {
            if (-not $c.used -and $c.t -le $s.t -and ($s.t - $c.t) -le $ChoiceToShotSec) { if ($null -eq $best -or $c.t -gt $best.t) { $best = $c } }
        }
        if ($best) { $s.delay = $s.t - $best.t; $best.used = $true }
    }
}

$skills = @('beginner', 'normal', 'veteran', 'elite')
$per = [ordered]@{}
foreach ($sk in $skills) {
    $ss = @($shots | Where-Object { $skill.ContainsKey($_.shooter) -and $skill[$_.shooter] -eq $sk })
    $hh = @($hits | Where-Object { $skill.ContainsKey($_.shooter) -and $skill[$_.shooter] -eq $sk })
    $head = @($hh | Where-Object { $_.head -eq 1 })
    $delay = @($ss | Where-Object { $_.delay -ge 0 } | ForEach-Object { $_.delay })
    $burst = @($ss | ForEach-Object { $_.burst })
    $moving = @($ss | Where-Object { $_.moving -eq 1 })
    $bands = [ordered]@{}
    foreach ($b in @('0-10', '10-20', '20-30', '30-50', '50+')) {
        $bands[$b] = @($hh | Where-Object { (Band $_.dist) -eq $b }).Count
    }
    $h20 = $bands['20-30'] + $bands['30-50'] + $bands['50+']
    $per[$sk] = [ordered]@{
        kills = $(if ($kills.ContainsKey($sk)) { $kills[$sk] } else { 0 })
        deaths = $(if ($deaths.ContainsKey($sk)) { $deaths[$sk] } else { 0 })
        shots = $ss.Count
        hits = $hh.Count
        hit_rate = [math]::Round($hh.Count / [math]::Max(1, $ss.Count), 3)
        headshots = $head.Count
        headshot_share = [math]::Round($head.Count / [math]::Max(1, $hh.Count), 3)
        first_shot_delay_median_s = [math]::Round((Median $delay), 2)
        first_shot_delay_samples = $delay.Count
        burst_median = [math]::Round((Median $burst), 1)
        moving_shot_share = [math]::Round($moving.Count / [math]::Max(1, $ss.Count), 3)
        hits_by_band = $bands
        hits_20m_share = [math]::Round($h20 / [math]::Max(1, $hh.Count), 3)
    }
}
$allHits = $hits.Count; $allShots = $shots.Count
$result = [ordered]@{
    schema = 'warsbots-duels/1'
    source = [IO.Path]::GetFileName($Log)
    session = [ordered]@{ shots = $allShots; hits = $allHits; hit_rate = [math]::Round($allHits / [math]::Max(1, $allShots), 3) }
    per_skill = $per
}
$result | ConvertTo-Json -Depth 6 | Set-Content -Path $Out -Encoding UTF8
Write-Output "duels: $Out"
foreach ($sk in $skills) {
    $p = $per[$sk]
    Write-Output ("{0}: kills={1} deaths={2} shots={3} hits={4} rate={5} delay={6}s burst={7} 20m+={8}" -f `
        $sk, $p.kills, $p.deaths, $p.shots, $p.hits, $p.hit_rate, $p.first_shot_delay_median_s, $p.burst_median, $p.hits_20m_share)
}
