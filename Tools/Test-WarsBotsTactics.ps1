# Controlled headless comparison; never fetches, checks out, builds or replaces EXEs.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$GameDirectory,
    [Parameter(Mandatory)][string]$BaselineDirectory,
    [Parameter(Mandatory)][string]$CandidateDirectory,
    [ValidateRange(60,600)][int]$Seconds=120,
    [ValidateRange(1,4)][int]$Repeats=2,
    [ValidateRange(1,4)][int]$FirstRepeat=1,
    [int[]]$Counts=@(6,15),
    [switch]$CandidateOnly,
    [switch]$BaselineOnly,
    [string]$ExpectedCandidate="",
    [string]$ExpectedBaseline="eea973f0",
    [string]$OutputDirectory='build/tactics-qa'
)
$ErrorActionPreference='Stop'
if($CandidateOnly -and $BaselineOnly){throw 'Choose one comparison side, not both.'}
$repo=Split-Path $PSScriptRoot -Parent
$out=[IO.Path]::GetFullPath((Join-Path $repo $OutputDirectory))
New-Item -ItemType Directory -Force $out | Out-Null
$game=(Resolve-Path -LiteralPath $GameDirectory).Path
$profile=Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'My Games\Crysis'
$memory=Join-Path $profile 'WarsBots\Multiplayer_IA_Steelmill.mem'
$backup=Join-Path $out 'initial-memory.mem'
$port=64140
$sessionTag=(Split-Path $out -Leaf) -replace '[^A-Za-z0-9_-]','_' 
function Servers { @(Get-Process -Name 'CryMP-Server64' -ErrorAction SilentlyContinue | Where-Object { -not $_.HasExited }) }
if ((Servers).Count) { throw 'An existing CryMP server is active; refusing shared-memory QA.' }
if (Get-NetUDPEndpoint -LocalPort $port -ErrorAction SilentlyContinue) { throw "UDP $port is busy" }
if (-not (Test-Path -LiteralPath $memory)) { throw 'An existing Steelmill memory snapshot is required for this comparison.' }
if (Test-Path -LiteralPath $backup) { throw 'Use a fresh output directory; existing evidence will not be overwritten.' }
Copy-Item -LiteralPath $memory -Destination $backup
$memoryHash=(Get-FileHash -LiteralPath $backup).Hash
$variants=@{}
foreach($pair in @(@('baseline',$BaselineDirectory),@('candidate',$CandidateDirectory))) {
    $dir=(Resolve-Path -LiteralPath $pair[1]).Path
    $manifest=Get-Content -LiteralPath (Join-Path $dir 'claude-build.json') -Raw | ConvertFrom-Json
    $exe=Join-Path $dir 'CryMP-Server64.exe'
    if ((Get-FileHash -LiteralPath $exe).Hash -ne $manifest.server) { throw "Hash mismatch in $dir" }
    $expected=if($pair[0] -eq 'baseline'){$ExpectedBaseline}else{$ExpectedCandidate}
    if($expected -and -not ([string]$manifest.sha).StartsWith($expected)){throw "Unexpected source SHA in $dir"}
    $variants[$pair[0]]=@{exe=$exe;manifest=$manifest}
}
$runs=@()
$p=$null
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
                    $skill=$skills[($i+2*$repeat)%4]
                    $roster+=@{number=$i+1;skill=$skill}
                    $commands+=" +wb_add 1 $skill"
                }
                $args="-dir `"$game`" -headless -logfile $name +sv_bind 127.0.0.1 +sv_lanonly 1 +sv_port $port +sv_gamerules InstantAction +map multiplayer/ia/steelmill +wb_probe_local_test 1 +wb_debug 1 +wb_maxbots $bots$commands +wb_list"
                Write-Host "START $tag sha=$($v.manifest.sha) memory=$memoryHash"
                $p=Start-Process -FilePath $v.exe -ArgumentList $args -WorkingDirectory $game -WindowStyle Hidden -PassThru
                $deadline=[DateTime]::UtcNow.AddSeconds($Seconds)
                $text='';$engine='';$joined=$false
                while([DateTime]::UtcNow -lt $deadline -and -not $p.HasExited) {
                    Start-Sleep -Seconds 5
                    if(Test-Path -LiteralPath $trace){$text=Get-Content -LiteralPath $trace -Raw}
                    if(Test-Path -LiteralPath $log){$engine=Get-Content -LiteralPath $log -Raw}
                    $joined=$text -match 'clients=[1-9]'
                    if($joined -or $engine -match 'CRASH DETECTED'){break}
                }
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
                $numbers=@([regex]::Matches($text,'#(\d+) created EntityId=') | ForEach-Object {$_.Groups[1].Value} | Sort-Object -Unique)
                $ok=-not $early -and $numbers.Count -eq $bots -and $text -match 'NAV ready' -and $engine -notmatch 'CRASH DETECTED'
                $run=[ordered]@{tag=$tag;variant=$variant;repeat=$repeat+1;bots=$bots;seconds=$Seconds;source_sha=$v.manifest.sha;server_sha256=$v.manifest.server;client_sha256=$v.manifest.client;memory_sha256=$memoryHash;roster=$roster;log=[IO.Path]::GetFileName($trace);completed=$ok;kills=([regex]::Matches($text,'combat kill')).Count}
                $runs+=$run
                $runs | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath (Join-Path $out 'runs.json') -Encoding UTF8
                Write-Host "END $tag completed=$ok kills=$($run.kills)"
                if(-not $ok){throw "Runtime smoke failed for $tag"}
            }
        }
    }
} finally {
    # Never stop a server a human joined. Restore memory only when no server can be using it.
    if($p -and -not $p.HasExited -and -not $joined){Stop-Process -Id $p.Id; $null=$p.WaitForExit(15000)}
    if ((Servers).Count -eq 0) {
        Copy-Item -LiteralPath $backup -Destination $memory -Force
        if((Get-FileHash -LiteralPath $memory).Hash -ne $memoryHash){throw 'Final memory restore failed'}
    } else { Write-Warning "Live server retained; memory backup preserved at $backup" }
}
