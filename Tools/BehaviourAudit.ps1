# DS-042 night-batch behaviour metrics for the 35 BEHAVIOUR_BIBLE situations.
# Usage:
#   powershell -File Tools/BehaviourAudit.ps1 -Log "<...>.bots.log" -OutPath out.json
param(
  [Parameter(Mandatory=$true)][string]$Log,
  [string]$OutPath = "behaviour_audit.json"
)
$ErrorActionPreference='Stop'

$created=@{}; $skill=@{}; $style=@{}
$shots=@(); $hits=@(); $kills=@(); $deaths=@()
$targetChoices=@(); $telemetry=@{}; $suitMode=@{}; $pickups=@(); $withdraws=@()
$trap=@(); $heard=0; $glance=0; $vantageTravel=0; $vantageRelocate=0; $highGround=0; $holdAngle=0
$moveRows=@{}; $blocked=0; $weaponRank=@(); $firstPrimary=@{}; $lastHitByTarget=@{}; $lastHitByPair=@{}
$cloakShots=0; $shotsAtClip0=0; $tacticalReload=0; $grenade=0; $cloakSamples=0; $jumpTotal=0; $strengthTakeoffs=0; $jumpEvents=@{}; $linkStarts=0; $linkStrength=0; $linkFailed=0; $linkDone=0

function T($line){ if($line -match '^([0-9.]+)'){return [double]$matches[1]}; return -1.0 }
function P($s){ $a=$s.Split(','); return @([double]$a[0],[double]$a[1],[double]$a[2]) }

foreach($line in [System.IO.File]::ReadAllLines($Log)){
  if($line -notmatch '^([0-9.]+)'){continue}
  $t=[double]$matches[1]
  if($line -match '#\d+ created EntityId=(\d+) name=\S+ \[(\w+)/(\w+)\] profile=\w+/(\w+)'){
    $id=[uint64]$matches[1]; $created[$id]=$true; $skill[$id]=$matches[2]; $style[$id]=$matches[3]
    $telemetry[$id]=New-Object System.Collections.ArrayList; $suitMode[$id]=New-Object System.Collections.ArrayList
    $moveRows[$id]=New-Object System.Collections.ArrayList
    continue
  }
  if($line -match 'FIRE SHOT shooter=(\d+) target=(\d+) projectile=(\d+) weapon=(\d+) class=(\w+) mode=\w+ rpm=[0-9.]+ clip=(-?\d+) remote=\d distance=([0-9]+(?:\.[0-9]+)?)'){
    $s=[uint64]$matches[1]
    $shots += [pscustomobject]@{t=$t;shooter=$s;target=[uint64]$matches[2];class=$matches[5];clip=[int]$matches[6];distance=[double]$matches[7]}
    if([int]$matches[6] -le 0){$shotsAtClip0++}
    continue
  }
  if($line -match 'FIRE HIT phase=(\S+) target=(\d+) shooter=(\d+) projectile=\d+ weapon=\d+ class=(\w+) actor=\d+ human=\d health=(-?\d+) suit=(\d+) energy=[0-9.]+ invulnerable=\d distance=([0-9]+(?:\.[0-9]+)?) material=\d+ head=(\d) part=\d+ damage=([0-9]+(?:\.[0-9]+)?)'){
    if($matches[1] -ne 'before-Lua'){continue}
    $target=[uint64]$matches[2]; $shooter=[uint64]$matches[3]
    $hits += [pscustomobject]@{t=$t;target=$target;shooter=$shooter;class=$matches[4];health=[int]$matches[5];suit=[int]$matches[6];distance=[double]$matches[7];head=[int]$matches[8];damage=[double]$matches[9]}
    $rec=[pscustomobject]@{t=$t;health=[int]$matches[5];class=$matches[4];shooter=$shooter}; $lastHitByTarget[$target]=$rec; $lastHitByPair["$target-$shooter"]=$rec
    continue
  }
  if($line -match 'combat kill shooter=(\d+) target=(\d+)'){
    $kills += [pscustomobject]@{t=$t;shooter=[uint64]$matches[1];target=[uint64]$matches[2]}
    continue
  }
  if($line -match 'death observed EntityId=(\d+) health=(-?\d+) lastHitShooter=(\d+)'){
    $deaths += [pscustomobject]@{t=$t;target=[uint64]$matches[1];health=[int]$matches[2];shooter=[uint64]$matches[3]}
    continue
  }
  if($line -match 'TARGET CHOICE actor=(\d+) old=(\d+) target=(\d+) skill=(\w+) style=(\w+) score=[0-9.]+ visible=(\d+) health=(-?\d+) ammo=(\d+)'){
    $targetChoices += [pscustomobject]@{t=$t;actor=[uint64]$matches[1];old=[uint64]$matches[2];target=[uint64]$matches[3];visible=[int]$matches[6];health=[int]$matches[7];ammo=[int]$matches[8]}
    continue
  }
  if($line -match 'AI telemetry EntityId=(\d+) state=(\w+) pos=\(([^)]+)\) health=(-?\d+) weapon=(\d+) ammo=(-?\d+) target=(\d+)'){
    $id=[uint64]$matches[1]; $p=P $matches[3]
    if($telemetry.ContainsKey($id)){[void]$telemetry[$id].Add([pscustomobject]@{t=$t;x=$p[0];y=$p[1];z=$p[2];ammo=[int]$matches[6];state=$matches[2]})}
    continue
  }
  if($line -match 'TACTIC SAMPLE actor=(\d+) skill=(\w+) style=\w+ weapon=(\w+) suit=(\d+) distance=([0-9]+(?:\.[0-9]+)?) action=(\w+) target=(\d+) visible=(\d)'){
    $id=[uint64]$matches[1]; $w=$matches[3]
    if($w -ne 'SOCOM' -and $w -ne 'none' -and -not $firstPrimary.ContainsKey($id)){$firstPrimary[$id]=$t}
    continue
  }
  if($line -match 'BOT SUIT: mode=(\d+) energy=[0-9.]+ active=\d+ actor=(\d+)'){
    $id=[uint64]$matches[2]
    if($suitMode.ContainsKey($id)){[void]$suitMode[$id].Add([pscustomobject]@{t=$t;mode=[int]$matches[1]})}
    continue
  }
  if($line -match 'REAL PICKUP actor=(\d+) item=(\d+) acquired=(\d)'){
    $pickups += [pscustomobject]@{t=$t;actor=[uint64]$matches[1];item=[uint64]$matches[2]}
    continue
  }
  if($line -match 'TACTIC actor=(\d+) skill=\w+ style=\w+ action=withdraw health=(-?\d+) energy=[0-9.]+ distance=([0-9]+(?:\.[0-9]+)?) visible=(\d)'){
    $withdraws += [pscustomobject]@{t=$t;actor=[uint64]$matches[1];health=[int]$matches[2];distance=[double]$matches[3]}
    continue
  }
  if($line -match 'TRAP RECOVERY .* actor=(\d+)'){
    $trap += [pscustomobject]@{t=$t;actor=[uint64]$matches[1]}
    continue
  }
  if($line -match 'JUMP TAKEOFF: strength=(\d) vz=[-0-9.]+ energy=[-0-9.]+ actor=(\d+)'){ $jumpTotal++; if($matches[1] -eq '1'){$strengthTakeoffs++}; $id=[uint64]$matches[2]; if(-not $jumpEvents.ContainsKey($id)){$jumpEvents[$id]=New-Object System.Collections.ArrayList}; [void]$jumpEvents[$id].Add($t); continue }
  if($line -match '#\d+ LINK start kind=(\w+)'){ $linkStarts++; if($matches[1] -eq 'strength'){$linkStrength++}; continue }
  if($line -match '#\d+ LINK failed'){$linkFailed++;continue}
  if($line -match '#\d+ LINK done'){$linkDone++;continue}
  if($line -match 'MOTION INTENT: .*blocked=1'){$blocked++;continue}
  if($line -match 'HEARD gunshot'){$heard++;continue}
  if($line -match 'GLANCE spot='){$glance++;continue}
  if($line -match 'VANTAGE travel'){$vantageTravel++;continue}
  if($line -match 'VANTAGE relocate'){$vantageRelocate++;continue}
  if($line -match 'HIGH GROUND start actor=(\d+)'){$highGround++;continue}
  if($line -match 'HOLD angle from'){$holdAngle++;continue}
  if($line -match 'WEAPON RANK actor=(\d+) select=(\w+) distance=([0-9]+(?:\.[0-9]+)?)'){$weaponRank += [pscustomobject]@{t=$t;actor=[uint64]$matches[1];cls=$matches[2];distance=[double]$matches[3]};continue}
  if($line -match 'MOVE TRACE actor=(\d+) kind=bot .*? t=([0-9.]+) .*?pos=\(([^)]+)\) vel=\(([^)]+)\) speed=([0-9.]+) vz=([-0-9.]+) mode=(-?\d+) energy=([-0-9.]+) sprint=(\d+) grounded=(\d+) flying=(\d+) jump=(\d+) land=(\d+) takeoffs=(\d+) jumpEdges=(\d+) landEdges=(\d+)'){
    $id=[uint64]$matches[1]; $p=P $matches[3]
    if($moveRows.ContainsKey($id)){
      [void]$moveRows[$id].Add([pscustomobject]@{t=$t;z=$p[2];speed=[double]$matches[5];vz=[double]$matches[6];mode=[int]$matches[7];energy=[double]$matches[8];sprint=[int]$matches[9];grounded=[int]$matches[10];jump=[int]$matches[11];land=[int]$matches[12];takeoffs=[uint64]$matches[13];jumpEdges=[uint64]$matches[14];landEdges=[uint64]$matches[15]})
    }
    continue
  }
}
# counts
$beforeHits=@($hits)
$distances=@($beforeHits|ForEach-Object{$_.distance}|Sort-Object)
function Pct($a,$q){ if($a.Count -eq 0){return 0}; return [math]::Round($a[[int][math]::Floor(($a.Count-1)*$q)],2) }
$bands=[ordered]@{lt5=0;b5_10=0;b10_20=0;b20_40=0;b40_60=0;gt60=0}
foreach($h in $beforeHits){ $d=$h.distance; if($d -lt 5){$bands.lt5++}elseif($d -lt 10){$bands.b5_10++}elseif($d -lt 20){$bands.b10_20++}elseif($d -lt 40){$bands.b20_40++}elseif($d -lt 60){$bands.b40_60++}else{$bands.gt60++} }
$killsClass=@{}; $killDist=@()
foreach($k in $kills){
  $pairKey="$($k.target)-$($k.shooter)"; $lh=if($lastHitByPair.ContainsKey($pairKey)){$lastHitByPair[$pairKey]}else{$null}
  if($lh -and ($k.t-$lh.t) -le 1.5){ if(-not $killsClass.ContainsKey($lh.class)){$killsClass[$lh.class]=0}; $killsClass[$lh.class]++; $killDist += $lh.distance }
}
# per skill
$perSkill=@{}
foreach($id in $created.Keys){
  $sk=$skill[$id]
  if(-not $perSkill.ContainsKey($sk)){$perSkill[$sk]=[pscustomobject]@{bots=0;kills=0;deaths=0;shots=0;hits=0;head=0;dist=@()}}
  $perSkill[$sk].bots++
}
foreach($s in $shots){ if($created.ContainsKey($s.shooter)){$perSkill[$skill[$s.shooter]].shots++} }
foreach($h in $beforeHits){ if($created.ContainsKey($h.shooter)){$perSkill[$skill[$h.shooter]].hits++; if($h.head -eq 1){$perSkill[$skill[$h.shooter]].head++}; $perSkill[$skill[$h.shooter]].dist += $h.distance} }
foreach($k in $kills){ if($created.ContainsKey($k.shooter)){$perSkill[$skill[$k.shooter]].kills++}; if($created.ContainsKey($k.target)){$perSkill[$skill[$k.target]].deaths++} }
foreach($k in $perSkill.Keys){ $v=$perSkill[$k]; $v | Add-Member -NotePropertyName acc -NotePropertyValue $(if($v.shots){[math]::Round(100.0*$v.hits/$v.shots,1)}else{0}) -Force; $v | Add-Member -NotePropertyName headPct -NotePropertyValue $(if($v.hits){[math]::Round(100.0*$v.head/$v.hits,1)}else{0}) -Force; $v | Add-Member -NotePropertyName distP50 -NotePropertyValue (Pct @($v.dist|Sort-Object) 0.5) -Force; $v | Add-Member -NotePropertyName distP90 -NotePropertyValue (Pct @($v.dist|Sort-Object) 0.9) -Force; $v.PSObject.Properties.Remove("dist") }
# target switch-away from a target at <=25 HP
$switchAway=0; $drops=0
foreach($tc in $targetChoices){
  if($tc.old -ne 0 -and $tc.target -eq 0){$drops++}
  if($tc.old -ne 0 -and $tc.target -ne $tc.old -and $lastHitByTarget.ContainsKey($tc.old)){
    $h=$lastHitByTarget[$tc.old]
    if(($tc.t-$h.t) -le 1.0 -and $h.health -le 25){$switchAway++}
  }
}
# loot after own kill, death after loot
$lootAfterKill=0; $deathsAfterPickup=0
foreach($k in $kills){ if(@($pickups|Where-Object{$_.actor -eq $k.shooter -and $_.t -ge $k.t -and $_.t -le ($k.t+3.0)}).Count -gt 0){$lootAfterKill++} }
foreach($p in $pickups){ if(@($deaths|Where-Object{$_.target -eq $p.actor -and $_.t -ge $p.t -and $_.t -le ($p.t+3.0)}).Count -gt 0){$deathsAfterPickup++} }
# deaths at low HP: health before the fatal hit
$lowHpDeaths=0
foreach($d in $deaths){ if($lastHitByTarget.ContainsKey($d.target)){ $h=$lastHitByTarget[$d.target]; if(($d.t-$h.t) -le 0.5 -and $h.health -le 30){$lowHpDeaths++} } }
# deaths after withdraw
$deathsAfterWithdraw=0
foreach($w in $withdraws){ if(@($deaths|Where-Object{$_.target -eq $w.actor -and $_.t -ge $w.t -and $_.t -le ($w.t+2.5)}).Count -gt 0){$deathsAfterWithdraw++} }
# move trace aggregate by skill
$jumps=0; $strengthJumps=0; $maxChain=0; $air=0; $high=0; $moveN=0; $sprint=0; $speeds=@()
foreach($id in $moveRows.Keys){
  $rows=@($moveRows[$id]|Sort-Object t); if($rows.Count -eq 0){continue}
  $prev=$null; $chain=0; $lastJump=-99.0
  foreach($r in $rows){
    $moveN++; if($r.grounded -eq 0){$air++}; if($r.z -ge 140){$high++}; if($r.sprint -eq 1){$sprint++}; if($r.speed -gt 0.3){$speeds += $r.speed}
    if($prev -and $r.takeoffs -gt $prev.takeoffs){
      $jumps++; if($r.mode -eq 1){$strengthJumps++}
      if($r.t-$lastJump -le 1.6){$chain++}else{$chain=1}
      if($chain -gt $maxChain){$maxChain=$chain}
      $lastJump=$r.t
    }
    $prev=$r
  }
}
# suit share from MOVE TRACE (4 Hz) and cloak shots
$modeSamples=@{}; $cloaked=0
$maxJumpChain=0
foreach($id in $jumpEvents.Keys){
  $arr=@($jumpEvents[$id]|Sort-Object); $chain=0; $last=-99.0
  foreach($t in $arr){
    if($t-$last -le 1.6){$chain++}else{$chain=1}
    if($chain -gt $maxJumpChain){$maxJumpChain=$chain}
    $last=$t
  }
}
foreach($id in $moveRows.Keys){
  foreach($r in $moveRows[$id]){ $m=$r.mode; if(-not $modeSamples.ContainsKey($m)){$modeSamples[$m]=0}; $modeSamples[$m]++ }
}
foreach($s in $shots){
  if($moveRows.ContainsKey($s.shooter)){
    $last=$null; foreach($r in $moveRows[$s.shooter]){ if($r.t -le $s.t){$last=$r}else{break} }
    if($last -and $last.mode -eq 2){$cloaked++}
  }
}
# heights at kill: last telemetry (2 s) for shooter and target
$heightKills=0; $heightN=0
foreach($k in $kills){
  if(-not $telemetry.ContainsKey($k.shooter) -or -not $telemetry.ContainsKey($k.target)){continue}
  $a=$null; foreach($r in $telemetry[$k.shooter]){ if($r.t -le $k.t){$a=$r}else{break} }
  $b=$null; foreach($r in $telemetry[$k.target]){ if($r.t -le $k.t){$b=$r}else{break} }
  if($a -and $b -and ($k.t-$a.t) -le 3.0 -and ($k.t-$b.t) -le 3.0){ $heightN++; if(($a.z-$b.z) -ge 3.0){$heightKills++} }
}
$duration=0.0
$all=@($shots + $hits + $kills + $deaths)
if($all.Count){$duration=($all|Measure-Object t -Maximum).Maximum}
$primaryTimes=@($firstPrimary.Values)
$modeOut=[ordered]@{}; foreach($mk in $modeSamples.Keys){$modeOut["mode$mk"]=$modeSamples[$mk]}
$result=[ordered]@{
  log=[IO.Path]::GetFileName($Log)
  duration_s=[math]::Round($duration,1)
  bots=$created.Count
  kills=$kills.Count; deaths=$deaths.Count; kd=if($deaths.Count){[math]::Round(1.0*$kills.Count/$deaths.Count,2)}else{0}
  shots=$shots.Count; hits=$beforeHits.Count; shots_per_kill=if($kills.Count){[math]::Round(1.0*$shots.Count/$kills.Count,1)}else{0}
  accuracy_pct=if($shots.Count){[math]::Round(100.0*$beforeHits.Count/$shots.Count,1)}else{0}
  hit_dist_p50=(Pct $distances 0.5); hit_dist_p90=(Pct $distances 0.9); hit_dist_max=if($distances.Count){$distances[-1]}else{0}
  hit_bands=$bands
  headshots=@($beforeHits|Where-Object{$_.head -eq 1}).Count
  headshot_pct=if($beforeHits.Count){[math]::Round(100.0*@($beforeHits|Where-Object{$_.head -eq 1}).Count/$beforeHits.Count,1)}else{0}
  damage_total=[math]::Round((($beforeHits|Measure-Object damage -Sum).Sum),0)
  hit_target_armor_pct=if($beforeHits.Count){[math]::Round(100.0*@($beforeHits|Where-Object{$_.suit -eq 3}).Count/$beforeHits.Count,1)}else{0}
  kill_class=$killsClass
  kill_dist_p50=(Pct @($killDist|Sort-Object) 0.5)
  per_skill=$perSkill
  target_choices=$targetChoices.Count; drops=$drops; switch_away_low_hp=$switchAway
  pickups=$pickups.Count; loot_after_kill=$lootAfterKill; deaths_within_3s_of_pickup=$deathsAfterPickup
  trap_recoveries=$trap.Count; blocked_motion=$blocked
  withdraws=$withdraws.Count; deaths_after_withdraw=$deathsAfterWithdraw; deaths_at_low_hp=$lowHpDeaths
  heard=$heard; glances=$glance; vantage_travel=$vantageTravel; vantage_relocate=$vantageRelocate; high_ground_starts=$highGround; hold_angle=$holdAngle
  weapon_rank=$weaponRank.Count
  first_primary_p50_s=(Pct @($primaryTimes|Sort-Object) 0.5); first_primary_p90_s=(Pct @($primaryTimes|Sort-Object) 0.9)
  shots_at_clip0=$shotsAtClip0; cloak_shots=$cloaked
  jumps=$jumpTotal; jumps_per_min=if($duration){[math]::Round($jumpTotal/($duration/60.0),2)}else{0}; strength_jumps=$strengthTakeoffs; max_chain=$maxJumpChain; link_starts=$linkStarts; link_strength=$linkStrength; link_failed=$linkFailed; link_fail_pct=if($linkStarts){[math]::Round(100.0*$linkFailed/$linkStarts,1)}else{0}
  speed_p50=(Pct @($speeds|Sort-Object) 0.5); speed_p90=(Pct @($speeds|Sort-Object) 0.9)
  air_pct=if($moveN){[math]::Round(100.0*$air/$moveN,1)}else{0}
  high_pct=if($moveN){[math]::Round(100.0*$high/$moveN,1)}else{0}
  sprint_pct=if($moveN){[math]::Round(100.0*$sprint/$moveN,1)}else{0}
  suit_mode_samples=$modeOut
  kills_from_height=$heightKills; kills_with_height_known=$heightN; height_kill_pct=if($heightN){[math]::Round(100.0*$heightKills/$heightN,1)}else{0}
  tactical_reload=0; grenades=0
}
$result | ConvertTo-Json -Depth 8 | Set-Content $OutPath -Encoding UTF8
$result | ConvertTo-Json -Depth 4




