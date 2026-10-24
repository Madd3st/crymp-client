# Controlled headless comparison; never fetches, checks out, builds or replaces EXEs.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$GameDirectory,
    [ValidateRange(1024,49151)][int]$Port=46040,
    [Parameter(Mandatory)][string]$BaselineDirectory,
    [Parameter(Mandatory)][string]$CandidateDirectory,
    [ValidateRange(60,1800)][int]$Seconds=120,
    [ValidateRange(1,4)][int]$Repeats=2,
    [ValidateRange(1,4)][int]$FirstRepeat=1,
    # Four repeats with step=1 place every skill in every slot. Archived October
    # comparisons used step=2; request that explicitly to reproduce their rosters.
    [ValidateRange(1,3)][int]$SkillRotationStep=1,
    [int[]]$Counts=@(6,15),
    [switch]$CandidateOnly,
    [switch]$BaselineOnly,
    [switch]$FireDiagnostics,
    [ValidateSet('wb_bot_look','wb_bot_turn_cap','wb_bot_weapon_rank','wb_bot_arbiter','wb_bot_navmesh','wb_bot_perception','wb_bot_aim_model')][string[]]$BaselineFeatureGates=@(),
    [ValidateSet('wb_bot_look','wb_bot_turn_cap','wb_bot_weapon_rank','wb_bot_arbiter','wb_bot_navmesh','wb_bot_perception','wb_bot_aim_model')][string[]]$CandidateFeatureGates=@(),
    [switch]$InspectFeatureGates,
    [switch]$FixedRoster,
    [ValidateRange(-1,1)][int]$BaselineDemoRecord=-1,
    [ValidateRange(-1,1)][int]$CandidateDemoRecord=-1,
    [ValidateSet("wall","fixed","scale")][string]$TimeMode="wall",
    [ValidateRange(1,8)][double]$Speed=1,
    [ValidateRange(-1,1)][int]$BaselineBotPace=-1,
    [ValidateRange(-1,1)][int]$CandidateBotPace=-1,
    [ValidateRange(-1,1)][int]$CandidateTrapRecovery=-1,
    [ValidateRange(-1,1)][int]$BaselineTrapRecovery=-1,
    [ValidateRange(-1,1)][int]$CandidatePhysicsDiagnostics=-1,
    [ValidateRange(-1,1)][int]$BaselinePhysicsDiagnostics=-1,
    [ValidateRange(-1,1)][int]$CandidateMoveTrace=-1,
    [ValidateRange(-1,1)][int]$BaselineMoveTrace=-1,
    [ValidateRange(-1,1)][int]$CandidateReachableGoals=-1,
    [ValidateRange(-1,1)][int]$BaselineReachableGoals=-1,
    [int[]]$ExplainNodes=@(),
    [string]$ExplainPairsPath='',
    [ValidateRange(-1,1)][int]$CandidateHotspotAllNodes=-1,
    [ValidateRange(-1,1)][int]$BaselineHotspotAllNodes=-1,
    [switch]$NavExports,
    [ValidateSet("auto","universal")][string]$Style="auto",
    [string]$ExpectedCandidate="",
    [ValidateRange(-1,1)][int]$CandidateProgress=-1,
    [ValidateRange(-1,1)][int]$BaselineProgress=-1,
    [string]$ExpectedBaseline="eea973f0",
    [string]$OutputDirectory='build/tactics-qa',
    [string]$PythonExecutable='',
    [string]$TriageScript=(Join-Path $PSScriptRoot 'SteelmillTriage/steelmill_triage.py')
)
$ErrorActionPreference='Stop'
if($CandidateOnly -and $BaselineOnly){throw 'Choose one comparison side, not both.'}
$repo=Split-Path $PSScriptRoot -Parent
$out=[IO.Path]::GetFullPath((Join-Path $repo $OutputDirectory))
New-Item -ItemType Directory -Force $out | Out-Null
$game=(Resolve-Path -LiteralPath $GameDirectory).Path
$profile=Join-Path $env:LOCALAPPDATA 'WarsBots/Logs'
$dataMemory=Join-Path $env:LOCALAPPDATA 'WarsBots/Memory'
$memory=Join-Path $dataMemory 'Multiplayer_IA_Steelmill.mem'
$backup=Join-Path $out 'initial-memory.mem'
$port=$Port
$sessionTag=(Split-Path $out -Leaf) -replace '[^A-Za-z0-9_-]','_' 
function Servers { @(Get-Process -Name 'CryMP-Server64' -ErrorAction SilentlyContinue | Where-Object { -not $_.HasExited }) }
if ((Servers).Count) { throw 'An existing CryMP server is active; refusing shared-memory QA.' }
if(Get-Process CryMP-Client64 -ErrorAction SilentlyContinue){throw 'Owner client active; heavy QA deferred'}
if (Get-NetUDPEndpoint -LocalPort $port -ErrorAction SilentlyContinue) { throw "UDP $port is busy" }
if (-not (Test-Path -LiteralPath $memory)) { throw 'An existing Steelmill memory snapshot is required for this comparison.' }
if (Test-Path -LiteralPath $backup) { throw 'Use a fresh output directory; existing evidence will not be overwritten.' }
Copy-Item -LiteralPath $memory -Destination $backup
$memoryHash=(Get-FileHash -LiteralPath $backup).Hash
$variants=@{}
foreach($pair in @(@('baseline',$BaselineDirectory),@('candidate',$CandidateDirectory))) {
    $dir=(Resolve-Path -LiteralPath $pair[1]).Path
    $manifest=Get-Content -LiteralPath (Join-Path $dir 'claude-build.json') -Raw | ConvertFrom-Json
    if($manifest.data_layout -ne 'local-v1'){throw 'QA requires local-v1 build after C-008; frozen legacy builds use old profile paths'}
    $exe=Join-Path $dir 'CryMP-Server64.exe'
    if ((Get-FileHash -LiteralPath $exe).Hash -ne $manifest.server) { throw "Hash mismatch in $dir" }
    $expected=if($pair[0] -eq 'baseline'){$ExpectedBaseline}else{$ExpectedCandidate}
    if($expected -and -not ([string]$manifest.sha).StartsWith($expected)){throw "Unexpected source SHA in $dir"}
    $variants[$pair[0]]=@{exe=$exe;manifest=$manifest}
}
$runs=@()
$p=$null
$qaCompleted=$false
try {
    for($repeat=$FirstRepeat-1;$repeat -lt $FirstRepeat-1+$Repeats;$repeat++) {
        foreach($bots in $Counts) {
            if($bots -notin @(6,15)) { throw 'Only 6/15-bot comparisons are supported' }
            $order=if($repeat%2 -eq 0){@('baseline','candidate')}else{@('candidate','baseline')}
            foreach($variant in $order) {
                if($CandidateOnly -and $variant -eq 'baseline'){continue}
                if($BaselineOnly -and $variant -eq 'candidate'){continue}
                if ((Servers).Count) { throw 'Another server appeared; refusing to change shared memory.' }
                Copy-Item -LiteralPath $backup -Destination $memory -Force
                if ((Get-FileHash -LiteralPath $memory).Hash -ne $memoryHash) { throw 'Memory restore failed' }
                $v=$variants[$variant]
                if ((Get-FileHash -LiteralPath $v.exe).Hash -ne $v.manifest.server) { throw 'EXE changed during QA' }
                $tag="$variant-r$($repeat+1)-n$bots"
                $name="WarsBots-Tactics-$sessionTag-$tag.log"
                $log=Join-Path $profile $name
                $trace=$log -replace '\.log$','.bots.log'
                if(Test-Path -LiteralPath $log){throw "Log already exists: $log"}
                $roster=@()
                $commands=''
                $skills=@('beginner','normal','veteran','elite')
                for($i=0;$i -lt $bots;$i++) {
                    $rotation=if($FixedRoster){0}else{$SkillRotationStep*$repeat}
                    $skill=$skills[($i+$rotation)%4]
                    $roster+=@{number=$i+1;skill=$skill;style=$Style}
                    $commands+=" +wb_add 1 $skill"
                    if($Style -ne "auto"){$commands+=" $Style"}
                }
                $value=if($variant -eq 'candidate'){$CandidateTrapRecovery}else{$BaselineTrapRecovery}
                if($value -ge 0){$commands=" +wb_trap_recovery $value"+$commands}
                $value=if($variant -eq 'candidate'){$CandidateHotspotAllNodes}else{$BaselineHotspotAllNodes}
                if($value -ge 0){$commands=" +wb_hotspot_all_nodes $value"+$commands}
                if($NavExports){$commands=" +wb_nav_export after 15 +wb_nav_export after 25"+$commands}
                $value=if($variant -eq 'candidate'){$CandidateMoveTrace}else{$BaselineMoveTrace}
                if($value -ge 0){$commands=" +wb_move_trace $value"+$commands}
                $value=if($variant -eq 'candidate'){$CandidatePhysicsDiagnostics}else{$BaselinePhysicsDiagnostics}
                if($value -ge 0){$commands=" +wb_physics_diagnostics $value"+$commands}
                $value=if($variant -eq 'candidate'){$CandidateReachableGoals}else{$BaselineReachableGoals}
                if($value -ge 0){$commands=" +wb_reachable_goals $value"+$commands}
                if($ExplainNodes.Count){
                    if($ExplainNodes.Count -ne 2 -or $ExplainNodes[0] -lt 0 -or $ExplainNodes[1] -lt 0){throw 'ExplainNodes requires two nonnegative node IDs'}
                    $commands=" +wb_nav_explain after 15 $($ExplainNodes[0]) $($ExplainNodes[1])"+$commands
                }
                if($ExplainPairsPath) {
                    $pairs=@(Get-Content -LiteralPath $ExplainPairsPath -Raw | ConvertFrom-Json)
                    if($pairs.Count -ne 40){throw 'C-006 fixture requires exactly40 pairs'}
                    foreach($pair in $pairs){
                        if($pair.frm -isnot [long] -and $pair.frm -isnot [int]){throw 'Invalid from node'}
                        if($pair.to -isnot [long] -and $pair.to -isnot [int]){throw 'Invalid to node'}
                        if($pair.frm -lt 0 -or $pair.to -lt 0){throw 'Negative node'}
                        $commands=" +wb_nav_explain after 15 $($pair.frm) $($pair.to)"+$commands
                    }
                }
                $pace=if($variant -eq 'candidate'){$CandidateBotPace}else{$BaselineBotPace}
                if($pace -ge 0){$commands=" +wb_bot_pace $pace"+$commands}
                $demo=if($variant -eq 'candidate'){$CandidateDemoRecord}else{$BaselineDemoRecord}
                if($demo -ge 0){$commands=" +wb_demo_record $demo"+$commands}
                $gates=if($variant -eq 'candidate'){@($CandidateFeatureGates)}else{@($BaselineFeatureGates)}
                if($InspectFeatureGates){foreach($name in @('wb_bot_look','wb_bot_turn_cap','wb_bot_weapon_rank','wb_bot_arbiter','wb_bot_navmesh','wb_bot_perception','wb_bot_aim_model')){$commands=" +$name"+$commands}}
                foreach($name in $gates){$commands=" +$name 1"+$commands}
                if($FireDiagnostics){$commands=" +wb_fire_diagnostics 1"+$commands}
                $progress=if($variant -eq 'candidate'){$CandidateProgress}else{$BaselineProgress}
                if($progress -ge 0){$commands=" +wb_progress_experiment $progress"+$commands}
                $timeCommands=''
                if($TimeMode -ne 'wall'){
                    # Diagnostic-only QA. Never expose a sped-up server to an accidental client.
                    $password=[guid]::NewGuid().ToString('N')
                    $rate=[math]::Round(30*$Speed)
                    $scale=$Speed.ToString([Globalization.CultureInfo]::InvariantCulture)
                    $timeCommands=if($TimeMode -eq 'fixed'){" +t_FixedStep 0.033333 +t_Scale 1 +sv_DedicatedMaxRate $rate"}else{" +wb_qa_time_scale $scale +sv_DedicatedMaxRate $rate"}
                    $timeCommands=" +sv_password $password"+$timeCommands
                }
                $args="-dir `"$game`" -headless -logfile `"$log`" +sv_bind 127.0.0.1 +sv_lanonly 1 +sv_port $port +sv_gamerules InstantAction +map multiplayer/ia/steelmill +wb_probe_local_test 1 +wb_debug 1 +wb_maxbots $bots$commands$timeCommands +wb_list"
                Write-Host "START $tag sha=$($v.manifest.sha) memory=$memoryHash"
                $wallWatch=[Diagnostics.Stopwatch]::StartNew()
                $p=Start-Process -FilePath $v.exe -ArgumentList $args -WorkingDirectory $game -WindowStyle Hidden -PassThru
                $deadline=[DateTime]::UtcNow.AddSeconds($(if($TimeMode -eq 'wall'){$Seconds}else{$Seconds*2}))
                $gameObserved=0.;$timeError=''

                $text='';$engine='';$joined=$false
                $navPath=Join-Path $dataMemory 'Multiplayer_IA_Steelmill.nav.json'
                $navCaptured=0;$navStamp=[DateTime]::UtcNow
                while([DateTime]::UtcNow -lt $deadline -and -not $p.HasExited) {
                    Start-Sleep -Seconds $(if($TimeMode -eq 'wall'){5}else{1})
                    if(Test-Path -LiteralPath $trace){$text=Get-Content -LiteralPath $trace -Raw}
                    if(Test-Path -LiteralPath $log){$engine=Get-Content -LiteralPath $log -Raw}
                    if($TimeMode -ne 'wall'){
                        $times=[regex]::Matches($text,'(?m)^([0-9.]+) ')
                        if($times.Count){$gameObserved=[double]$times[$times.Count-1].Groups[1].Value}
                        if($engine -match 'Unknown command: (t_FixedStep|t_Scale|sv_DedicatedMaxRate|wb_qa_time_scale)'){$timeError='Native timer/rate CVar unavailable';break}
                        if($text -match 'QA TIME SCALE refused'){$timeError='Dedicated/password/gate refused time scale';break}
                        if($wallWatch.Elapsed.TotalSeconds -gt 30 -and $gameObserved/$wallWatch.Elapsed.TotalSeconds -lt 1.2){$timeError='Requested acceleration not observed after30s';break}
                        if($gameObserved -ge $Seconds){break}
                    }
                    if($NavExports -and $navCaptured -lt 2 -and (Test-Path -LiteralPath $navPath)){
                        $navFile=Get-Item -LiteralPath $navPath
                        if($navFile.LastWriteTimeUtc -gt $navStamp){
                            $navCaptured++;$navStamp=$navFile.LastWriteTimeUtc
                            Copy-Item -LiteralPath $navPath -Destination (Join-Path $out "$tag-nav-$navCaptured.json")
                        }
                    }
                    $joined=$text -match 'clients=[1-9]'
                    if($joined -or $engine -match 'CRASH DETECTED'){break}
                }
                $wallWatch.Stop();$wallSeconds=$wallWatch.Elapsed.TotalSeconds
                $early=$p.HasExited
                if($joined){throw 'A real client joined; QA left that server running and stopped the batch.'}
                if(-not $p.HasExited){Stop-Process -Id $p.Id}
                if(-not $p.WaitForExit(15000)){throw 'Own QA server did not finish exiting'}
                $p.Dispose(); $p=$null
                Start-Sleep -Milliseconds 300
                foreach($source in @($log,$trace)) {
                    if(Test-Path -LiteralPath $source){Copy-Item -LiteralPath $source -Destination $out}
                }
                $text=if(Test-Path -LiteralPath $trace){Get-Content -LiteralPath $trace -Raw}else{''}
                $engine=if(Test-Path -LiteralPath $log){Get-Content -LiteralPath $log -Raw}else{''}
                $observed=([regex]::Matches($text,'(?m)^([0-9.]+) ') | ForEach-Object {[double]$_.Groups[1].Value} | Measure-Object -Maximum).Maximum
                $numbers=@([regex]::Matches($text,'#(\d+) created EntityId=') | ForEach-Object {$_.Groups[1].Value} | Sort-Object -Unique)
                $ok=-not $early -and $numbers.Count -eq $bots -and $text -match 'NAV ready' -and $engine -notmatch 'CRASH DETECTED|Lua Error|Script Error|\[Error\]'
                if($NavExports -and $navCaptured -ne 2){$ok=$false}
                if($ExplainPairsPath -and ([regex]::Matches($text,'NAV explain from=')).Count -ne 80){$ok=$false}
                if($Seconds -ge 660 -and $observed -lt $(if($TimeMode -eq 'wall'){600}else{$Seconds})){$ok=$false}
                if($timeError){$ok=$false}
                $run=[ordered]@{feature_gates=@($gates);inspect_feature_gates=[bool]$InspectFeatureGates;demo_record_requested=$demo;time_mode=$TimeMode;requested_speed=$Speed;wall_seconds=$wallSeconds;game_seconds=$observed;speedup=$(if($wallSeconds -gt 0){$observed/$wallSeconds}else{0});time_error=$timeError;bot_pace_requested=$pace;fixed_roster=[bool]$FixedRoster;move_trace_requested=$(if($variant -eq 'candidate'){$CandidateMoveTrace}else{$BaselineMoveTrace});reachable_goals_requested=$(if($variant -eq 'candidate'){$CandidateReachableGoals}else{$BaselineReachableGoals});explain_nodes=$ExplainNodes;explain_pairs_path=$ExplainPairsPath;physics_diagnostics_requested=$(if($variant -eq 'candidate'){$CandidatePhysicsDiagnostics}else{$BaselinePhysicsDiagnostics});observed_seconds=$observed;nav_exports=$navCaptured;hotspot_all_nodes_requested=$(if($variant -eq 'candidate'){$CandidateHotspotAllNodes}else{$BaselineHotspotAllNodes});tag=$tag;session=$sessionTag;progress_requested=$progress;trap_recovery_requested=$(if($variant -eq 'candidate'){$CandidateTrapRecovery}else{$BaselineTrapRecovery});fire_diagnostics=[bool]$FireDiagnostics;style_mode=$Style;skill_rotation_step=$SkillRotationStep;variant=$variant;repeat=$repeat+1;bots=$bots;seconds=$Seconds;source_sha=$v.manifest.sha;server_sha256=$v.manifest.server;client_sha256=$v.manifest.client;memory_sha256=$memoryHash;roster=$roster;log=[IO.Path]::GetFileName($trace);completed=$ok;kills=([regex]::Matches($text,'combat kill')).Count}
                $runs+=$run
                $runs | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath (Join-Path $out 'runs.json') -Encoding UTF8
                Write-Host "END $tag completed=$ok kills=$($run.kills)"
                if(-not $ok){throw "Runtime smoke failed for $tag"}
            }
        }
    }
    $qaCompleted=$true
} finally {
    # Never stop a server a human joined. Restore memory only when no server can be using it.
    if($p -and -not $p.HasExited -and -not $joined){Stop-Process -Id $p.Id; $null=$p.WaitForExit(15000)}
    if ((Servers).Count -eq 0) {
        Copy-Item -LiteralPath $backup -Destination $memory -Force
        if((Get-FileHash -LiteralPath $memory).Hash -ne $memoryHash){throw 'Final memory restore failed'}
    } else { Write-Warning "Live server retained; memory backup preserved at $backup" }
    if(Test-Path -LiteralPath (Join-Path $out 'runs.json')) {
        try {
            & (Join-Path $PSScriptRoot 'Invoke-WarsBotsTriage.ps1') -OutputDirectory $out -PythonExecutable $PythonExecutable -TriageScript $TriageScript
        } catch {
            if($qaCompleted){throw}
            Write-Warning "Triage also failed while handling interrupted/failed QA: $_"
        }
    }
}
