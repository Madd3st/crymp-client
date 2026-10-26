# DS-041 owner moveset extractor.
# Reads owner client MOVE TRACE (kind=human) and the C-112 server bot trace (kind=bot).
# Usage:
#   powershell -File Tools/OwnerMoveset.ps1 `
#     -OwnerLogs "C091...Client.bots.log;C106...Client.bots.log;C112...Client.bots.log" `
#     -BotLog "C112...Server.bots.log" -OutPath Docs/Research/DeepSeek/owner_spots.json
param(
  [Parameter(Mandatory=$true)][string]$OwnerLogs,
  [string]$BotLog,
  [string]$OutPath = "owner_spots.json",
  [double]$HighZ = 140.0,
  [double]$FloorZ = 136.0
)
$ErrorActionPreference='Stop'

$re = 'pos=\(([^)]+)\) vel=\(([^)]+)\) speed=([0-9.]+) vz=([-0-9.]+) mode=(-?\d+) energy=([-0-9.]+) sprint=(\d+) grounded=(\d+) flying=(\d+) jump=(\d+) land=(\d+) takeoffs=(\d+) jumpEdges=(\d+) landEdges=(\d+) jumpTime=([-0-9.]+) landTime=([-0-9.]+) peakZ=([-0-9.]+) health=(-?\d+).*?yaw=([-0-9.]+) pitch=([-0-9.]+).*?stance=(-?\d+)'

function ParseTrace([string]$path,[string]$kindWanted,[string]$sessionName){
  $rows=New-Object System.Collections.ArrayList
  foreach($line in [System.IO.File]::ReadAllLines($path)){
    if($line -notmatch '^([0-9.]+)'){continue}
    $t=[double]$matches[1]
    if($line -notmatch 'MOVE TRACE actor=(\d+) kind=(\w+)'){continue}
    if($matches[2] -ne $kindWanted){continue}
    $actor=[uint64]$matches[1]
    if($line -notmatch $re){continue}
    $m=$matches
    $p=$m[1].Split(','); $v=$m[2].Split(',')
    [void]$rows.Add([pscustomobject]@{
      session=$sessionName
      t=$t; actor=$actor
      x=[double]$p[0]; y=[double]$p[1]; z=[double]$p[2]
      vx=[double]$v[0]; vy=[double]$v[1]; vz=[double]$v[2]
      speed=[double]$m[3]; mode=[int]$m[5]; energy=[double]$m[6]
      sprint=[int]$m[7]; grounded=[int]$m[8]; flying=[int]$m[9]
      jump=[int]$m[10]; land=[int]$m[11]; takeoffs=[int]$m[12]
      jumpEdges=[int]$m[13]; landEdges=[int]$m[14]
      jumpTime=[double]$m[15]; landTime=[double]$m[16]; peakZ=[double]$m[17]
      health=[int]$m[18]; yaw=[double]$m[19]; pitch=[double]$m[20]; stance=[int]$m[21]
    })
  }
  return $rows
}
function Pct($arr,[double]$q){
  if($arr.Count -eq 0){return 0}
  $s=@($arr|Sort-Object)
  return [math]::Round($s[[int][math]::Floor(($s.Count-1)*$q)],2)
}
function CellKey([double]$x,[double]$y,[double]$z){ "$([math]::Floor($x/3)*3),$([math]::Floor($y/3)*3),$([math]::Floor($z/3)*3)" }
function ModeName([int]$m){ switch($m){0{'Speed'}1{'Strength'}2{'Cloak'}3{'Armor'}default{"mode$m"}} }

function Stats($rows,[double]$SpanSeconds=0){
  $n=$rows.Count
  $first=$rows[0].t; $last=$rows[-1].t
  $sec = if($SpanSeconds -gt 0){$SpanSeconds}else{[math]::Max(0.001,$last-$first)}
  $moving=@($rows|Where-Object{$_.speed -gt 0.3 -and $_.grounded -eq 1})
  $dist=0.0; $prev=$null
  foreach($r in $rows){
    if($prev -and ($r.t-$prev.t) -le 1.0 -and ($r.t-$prev.t) -gt 0){
      $d=[math]::Sqrt(($r.x-$prev.x)*($r.x-$prev.x)+($r.y-$prev.y)*($r.y-$prev.y)+($r.z-$prev.z)*($r.z-$prev.z))
      if($d -lt 15){$dist+=$d}
    }
    $prev=$r
  }
  $byMode=@{}
  foreach($g in ($rows|Group-Object mode)){ $byMode[(ModeName ([int]$g.Name))]=$g.Count }
  $airMode=@{}
  foreach($g in ($rows|Where-Object{$_.grounded -eq 0}|Group-Object mode)){ $airMode[(ModeName ([int]$g.Name))]=$g.Count }
  # speed by posture
  $posture=[ordered]@{}
  foreach($st in 0,1,2){
    $set=@($rows|Where-Object{$_.grounded -eq 1 -and $_.stance -eq $st -and $_.speed -gt 0.3}|ForEach-Object{$_.speed})
    $nm=switch($st){0{'stand'}1{'crouch'}2{'prone'}}
    $posture[$nm]=[ordered]@{samples=@($rows|Where-Object{$_.stance -eq $st}).Count; moving=$set.Count; p50=(Pct $set 0.5); p90=(Pct $set 0.9); p99=(Pct $set 0.99)}
  }
  $airSpeed=@($rows|Where-Object{$_.grounded -eq 0 -and $_.speed -gt 0.3}|ForEach-Object{$_.speed})
  $posture['airborne']=[ordered]@{samples=@($rows|Where-Object{$_.grounded -eq 0}).Count; moving=$airSpeed.Count; p50=(Pct $airSpeed 0.5); p90=(Pct $airSpeed 0.9); p99=(Pct $airSpeed 0.99)}
  # jumps / chains / z gain
  $jumps=0; $chain=0; $maxChain=0; $lastJump=-99.0
  $chainHist=@{}; $airs=New-Object System.Collections.ArrayList
  $prev=$null
  foreach($r in $rows){
    if($prev -and $r.takeoffs -gt $prev.takeoffs){
      $jumps++
      if($r.t-$lastJump -le 1.6){$chain++}else{$chain=1}
      if($chain -gt $maxChain){$maxChain=$chain}
      $lastJump=$r.t
    }
    $prev=$r
  }
  for($i=0;$i -lt $n;$i++){
    if($rows[$i].grounded -ne 0){continue}
    $j=$i
    while($j+1 -lt $n -and $rows[$j+1].grounded -eq 0){$j++}
    $before=$null; for($k=$i-1;$k -ge 0;$k--){ if($rows[$k].grounded -eq 1){$before=$rows[$k];break} }
    $after=$null; for($k=$j+1;$k -lt $n;$k++){ if($rows[$k].grounded -eq 1){$after=$rows[$k];break} }
    if($before -and $after){
      $zmax=($rows[$i..$j]|Measure-Object z -Maximum).Maximum
      [void]$airs.Add([pscustomobject]@{
        t0=$before.t; t1=$after.t; dur=[math]::Round($after.t-$before.t,2)
        gain=[math]::Round($zmax-$before.z,2); net=[math]::Round($after.z-$before.z,2)
        len=[math]::Round([math]::Sqrt(($after.x-$before.x)*($after.x-$before.x)+($after.y-$before.y)*($after.y-$before.y)),2)
        mode=([int]$before.mode); speed=[math]::Round($before.speed,2)
      })
    }
    $i=$j
  }
  $chainGains=@()
  if($airs.Count){
    $g=0.0; $start=$null; $prevA=$null
    foreach($a in $airs){
      if($prevA -and ($a.t0-$prevA.t1) -le 0.4){ $g+= $a.gain } else { if($start){$chainGains+=[math]::Round($g,2)}; $g=$a.gain; $start=$a }
      $prevA=$a
    }
    if($start){$chainGains+=[math]::Round($g,2)}
  }
  # high ground runs
  $runs=@(); $start=$null; $prev=$null
  foreach($r in $rows){
    if($r.z -ge $HighZ -and $r.grounded -eq 1){
      if(-not $start){$start=$r}
      $prev=$r
    } elseif($start){
      if($prev -and ($r.t-$prev.t) -le 1.0){ $runs += [pscustomobject]@{dur=[math]::Round($prev.t-$start.t,1);z=[math]::Round($start.z,1);x=[math]::Round($start.x,1);y=[math]::Round($start.y,1)} }
      $start=$null; $prev=$null
    }
  }
  if($start -and $prev){$runs += [pscustomobject]@{dur=[math]::Round($prev.t-$start.t,1);z=[math]::Round($start.z,1);x=[math]::Round($start.x,1);y=[math]::Round($start.y,1)}}
  # reversals
  $rev=0; $last=$null
  foreach($r in $rows){
    if($last -and ($r.t-$last.t) -gt 0.1 -and ($r.t-$last.t) -le 0.6 -and $last.speed -gt 1.5 -and $r.speed -gt 1.5 -and (($r.vx*$last.vx+$r.vy*$last.vy) -lt 0)){$rev++}
    $last=$r
  }
  # cells / holds
  $cells=@{}
  foreach($r in $rows){
    $k=CellKey $r.x $r.y $r.z
    if(-not $cells.ContainsKey($k)){$cells[$k]=[pscustomobject]@{n=0;x=0;y=0;z=0;first=$r.t;last=$r.t;hold=0;runStart=$r.t}}
    $c=$cells[$k]; $c.n++; $c.x+=$r.x; $c.y+=$r.y; $c.z+=$r.z
    if(($r.t-$c.last) -gt 0.6){$c.runStart=$r.t}
    $c.last=$r.t
    $h=$r.t-$c.runStart; if($h -gt $c.hold){$c.hold=$h}
  }
  $spots=@($cells.GetEnumerator()|ForEach-Object{
    $c=$_.Value
    [pscustomobject]@{cell=$_.Key;n=$c.n;x=[math]::Round($c.x/$c.n,1);y=[math]::Round($c.y/$c.n,1);z=[math]::Round($c.z/$c.n,1);first=$c.first;last=$c.last;hold=[math]::Round($c.hold,1)}
  }|Sort-Object n -Descending)
  # prone and dwell segments (fight proxy)
  $segs=@()
  $cur=$null
  foreach($r in $rows){
    $isDwell = ($r.speed -lt 0.8 -and $r.z -ge $HighZ)
    $isProne = ($r.stance -eq 2)
    if($isProne -or $isDwell){
      if(-not $cur){$cur=[pscustomobject]@{t0=$r.t;t1=$r.t;session=$r.session;x=0.0;y=0.0;z=0.0;n=0;firstYaw=$r.yaw;yaws=(New-Object System.Collections.ArrayList);prone=($r.stance -eq 2);last=$null}}
      $cur.t1=$r.t; $cur.x+=$r.x; $cur.y+=$r.y; $cur.z+=$r.z; $cur.n++
      [void]$cur.yaws.Add($r.yaw)
      $cur.last=$r
    } elseif($cur){
      if($cur.t1-$cur.t0 -ge 1.5){$segs += $cur}
      $cur=$null
    }
  }
  if($cur -and $cur.t1-$cur.t0 -ge 1.5){$segs += $cur}
  $fights=@($segs|ForEach-Object{
    $mid=$_.n
    $sess=$_.session
    # circular yaw span around the first sample of the segment
    $firstYaw=$_.firstYaw
    $span=0.0
    foreach($y in $_.yaws){
      $d=[math]::Abs($y-$firstYaw)
      if($d -gt 180){$d=360-$d}
      if($d -gt $span){$span=$d}
    }
    # reversals in a +-2 s window around the dwell (strafe rhythm in the fight, not inside a prone hold)
    $w0=$_.t0-2.0; $w1=$_.t1+2.0
    $frev=0; $last=$null
    foreach($r in @($rows|Where-Object{$_.session -eq $sess -and $_.t -ge $w0 -and $_.t -le $w1})){
      if($last -and ($r.t-$last.t) -gt 0.1 -and ($r.t-$last.t) -le 0.6 -and $last.speed -gt 1.5 -and $r.speed -gt 1.5 -and (($r.vx*$last.vx+$r.vy*$last.vy) -lt 0)){$frev++}
      $last=$r
    }
    [pscustomobject]@{t0=[math]::Round($_.t0,1);dur=[math]::Round($_.t1-$_.t0,1);x=[math]::Round($_.x/$_.n,1);y=[math]::Round($_.y/$_.n,1);z=[math]::Round($_.z/$_.n,1);prone=$_.prone;yawSpan=[math]::Round($span,0);rev2s=$frev}
  }|Sort-Object dur -Descending)
  [pscustomobject]@{
    samples=$n; seconds=[math]::Round($sec,1); distance=[math]::Round($dist,0)
    speed_p50=(Pct @($moving|ForEach-Object{$_.speed}) 0.5)
    speed_p90=(Pct @($moving|ForEach-Object{$_.speed}) 0.9)
    speed_p99=(Pct @($moving|ForEach-Object{$_.speed}) 0.99)
    sprint_share=[math]::Round(100.0*@($rows|Where-Object{$_.sprint -eq 1}).Count/[math]::Max(1,$n),1)
    air_share=[math]::Round(100.0*@($rows|Where-Object{$_.grounded -eq 0}).Count/[math]::Max(1,$n),1)
    high_share=[math]::Round(100.0*@($rows|Where-Object{$_.z -ge $HighZ}).Count/[math]::Max(1,$n),1)
    veryhigh_share=[math]::Round(100.0*@($rows|Where-Object{$_.z -ge ($HighZ+5)}).Count/[math]::Max(1,$n),1)
    jumps=$jumps; jumps_per_min=[math]::Round($jumps/($sec/60.0),2); max_chain=$maxChain
    jump_gain_max=if($chainGains.Count){[math]::Round(($chainGains|Measure-Object -Maximum).Maximum,2)}else{0}
    jump_gain_avg=if($chainGains.Count){[math]::Round(($chainGains|Measure-Object -Average).Average,2)}else{0}
    jump_chains=$chainGains
    airborne_segments=$airs.Count
    airborne_dur_p50=(Pct @($airs|ForEach-Object{$_.dur}) 0.5)
    airborne_dur_p90=(Pct @($airs|ForEach-Object{$_.dur}) 0.9)
    reversals=$rev; reversals_per_min=[math]::Round($rev/($sec/60.0),2)
    stance_counts=[ordered]@{stand=@($rows|Where-Object{$_.stance -eq 0}).Count;crouch=@($rows|Where-Object{$_.stance -eq 1}).Count;prone=@($rows|Where-Object{$_.stance -eq 2}).Count}
    mode_share=$byMode; airborne_mode_share=$airMode
    speed_by_posture=$posture
    high_runs=@($runs|Sort-Object dur -Descending|Select-Object -First 15)
    top_spots=@($spots|Select-Object -First 15)
    fight_spots=@($fights|Select-Object -First 15)
  }
}

$ownerRows=@(); $sessions=@()
foreach($f in $OwnerLogs.Split(';')){
  $r=ParseTrace $f 'human' $f
  if($r.Count){ $ownerRows += $r; $sessions += [pscustomobject]@{file=[IO.Path]::GetFileName($f); stats=(Stats $r)} }
}
$spanSec=0.0; foreach($s in $sessions){$spanSec+=$s.stats.seconds}
$owner=Stats $ownerRows $spanSec
# high spots with entry route (8 s before first arrival in the cell)
$highSpots=@()
foreach($s in @($owner.top_spots|Where-Object{$_.z -ge $HighZ}|Select-Object -First 10)){
  $near=@($ownerRows|Where-Object{[math]::Sqrt(($_.x-$s.x)*($_.x-$s.x)+($_.y-$s.y)*($_.y-$s.y)) -lt 4.0 -and [math]::Abs($_.z-$s.z) -lt 3.0}|Sort-Object t)
  $entry=@()
  if($near.Count){
    $sess=$near[0].session
    $start=$near[0].t; $lastT=-99.0
    foreach($e in @($ownerRows|Where-Object{$_.session -eq $sess -and $_.t -ge ($start-8.0) -and $_.t -le $start}|Sort-Object t)){
      if($e.t-$lastT -ge 0.9){ $entry += [pscustomobject]@{t=[math]::Round($e.t,1);x=[math]::Round($e.x,1);y=[math]::Round($e.y,1);z=[math]::Round($e.z,1)}; $lastT=$e.t }
    }
  }
  $label = if($s.z -ge 157){'DSG1_roof'} elseif($s.z -ge 150){'upper_deck'} elseif($s.z -ge 145){'roof_147'} else {'ledge_142'}
  $highSpots += [pscustomobject]@{cell=$s.cell;label=$label;x=$s.x;y=$s.y;z=$s.z;height_above_floor_m=[math]::Round($s.z-$FloorZ,1);n=$s.n;hold_s=$s.hold;first=$s.first;last=$s.last;entry=$entry}
}
$bot=$null; $botPerBot=@()
if($BotLog -and (Test-Path $BotLog)){
  $b=ParseTrace $BotLog 'bot' $BotLog
  if($b.Count){
    $bot=Stats $b
    foreach($g in ($b|Group-Object actor)){
      $s=Stats $g.Group
      $botPerBot += [pscustomobject]@{
        actor=$g.Name; samples=$s.samples; seconds=$s.seconds
        speed_p50=$s.speed_p50; speed_p90=$s.speed_p90
        air_share=$s.air_share; high_share=$s.high_share; sprint_share=$s.sprint_share
        jumps=$s.jumps; jumps_per_min=$s.jumps_per_min; max_chain=$s.max_chain
        reversals_per_min=$s.reversals_per_min; mode_share=$s.mode_share
      }
    }
  }
}
$result=[ordered]@{
  schema='warsbots-owner-moveset/2'
  generated=(Get-Date).ToString('s')
  rules=@{high_ground_z=[double]$HighZ; floor_z=[double]$FloorZ; sampling_hz=4}
  sessions=$sessions
  owner=$owner
  bot_compare=$bot
  bot_per_bot=$botPerBot
  high_spots=$highSpots
  missing_fields=@(
    'owner trace has no target/enemy id or distance: "fights from 15-30 m" cannot be measured on the owner',
    'owner trace has no fire/shot/hit/damage events (client log only has MOVE TRACE)',
    'jump type (Normal vs Strength link vs wall jump) is not logged; suit mode before take-off is the only proxy',
    'wall contact and wall-jump are not logged',
    'jump/dodge cause (link, dodge, random) is not logged',
    'stance is logged, but not the input source (crouch toggle vs forced by aim)',
    'local floor height per sample is not logged; z>140 (Steelmill street ~136) is used as the high-ground proxy',
    'owner trace is 4 Hz in C-091/C-106/C-112; C-084 asks >=20 Hz for circle/wall jumps (jump chains are undercounted)',
    'health at fire time and who shot the owner are not joined to the trace',
    'yaw is absolute, not yaw rate; strafe direction is not logged'
  )
}
$result | ConvertTo-Json -Depth 12 | Set-Content $OutPath -Encoding UTF8
Write-Output ("wrote {0} ({1} bytes)" -f $OutPath,(Get-Item $OutPath).Length)
foreach($s in $sessions){ Write-Output ("{0}: {1}s p50={2} p90={3} air={4}% high={5}% jumps/min={6} chain={7} rev/min={8} mode={9}" -f $s.file,$s.stats.seconds,$s.stats.speed_p50,$s.stats.speed_p90,$s.stats.air_share,$s.stats.high_share,$s.stats.jumps_per_min,$s.stats.max_chain,$s.stats.reversals_per_min,($s.stats.mode_share.GetEnumerator()|ForEach-Object{"$($_.Key)=$($_.Value)"}) -join '/') }
Write-Output ("owner pooled: {0}s p50={1} p90={2} p99={3} air={4}% high={5}% jumps/min={6} chainMax={7} gainMax={8} rev/min={9}" -f $owner.seconds,$owner.speed_p50,$owner.speed_p90,$owner.speed_p99,$owner.air_share,$owner.high_share,$owner.jumps_per_min,$owner.max_chain,$owner.jump_gain_max,$owner.reversals_per_min)
Write-Output ("owner modes: " + (($owner.mode_share.GetEnumerator()|ForEach-Object{"$($_.Key)=$($_.Value)"}) -join ' '))
Write-Output ("owner air modes: " + (($owner.airborne_mode_share.GetEnumerator()|ForEach-Object{"$($_.Key)=$($_.Value)"}) -join ' '))
if($bot){ Write-Output ("bots pooled: {0}s p50={1} p90={2} air={3}% high={4}% jumps/min={5} chainMax={6} rev/min={7} modes={8}" -f $bot.seconds,$bot.speed_p50,$bot.speed_p90,$bot.air_share,$bot.high_share,$bot.jumps_per_min,$bot.max_chain,$bot.reversals_per_min,(($bot.mode_share.GetEnumerator()|ForEach-Object{"$($_.Key)=$($_.Value)"}) -join '/')) }
