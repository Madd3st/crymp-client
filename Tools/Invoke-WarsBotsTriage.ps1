# Offline only: run after logs have been copied and the QA server stopped.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$OutputDirectory,
    [string]$PythonExecutable='',
    [string]$TriageScript=(Join-Path $PSScriptRoot 'SteelmillTriage/steelmill_triage.py')
)
$ErrorActionPreference='Stop'
$out=(Resolve-Path -LiteralPath $OutputDirectory).Path
$runsPath=Join-Path $out 'runs.json'
if(-not (Test-Path -LiteralPath $runsPath)){throw 'No captured QA runs to analyze'}
$runs=@(Get-Content -LiteralPath $runsPath -Raw | ConvertFrom-Json)
$logs=@($runs | ForEach-Object {Join-Path $out $_.log} | Sort-Object -Unique)
if(-not $logs.Count){throw 'No captured QA logs to analyze'}
foreach($log in $logs){if(-not(Test-Path -LiteralPath $log)){throw "Missing captured log: $log"}}
if(-not $PythonExecutable){
    $bundled=Join-Path $env:USERPROFILE '.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
    if(Test-Path -LiteralPath $bundled){$PythonExecutable=$bundled}
    else {$PythonExecutable=(Get-Command python.exe -ErrorAction Stop).Source}
}
# Detailed multi-log report has no borrowed graph: each compact summary gets its own export.
$triageArgs=@('-B',$TriageScript,'analyze')+$logs+@('--mem',(Join-Path $out 'initial-memory.mem'),'--out',$out)
$rootSummary=Join-Path $out 'summary.json'
if(Test-Path -LiteralPath $rootSummary){Remove-Item -LiteralPath $rootSummary}
& $PythonExecutable @triageArgs
if($LASTEXITCODE -ne 0){throw "Steelmill Triage analyze failed ($LASTEXITCODE)"}
foreach($run in $runs) {
    # One compact contract object per run; never combine different scenarios into one verdict.
    $filename=if($runs.Count -eq 1){'summary.json'}else{"$($run.tag)-summary.json"}
    $summary=Join-Path $out $filename
    if(Test-Path -LiteralPath $summary){Remove-Item -LiteralPath $summary}
    $label="map=multiplayer/ia/steelmill;bots=$($run.bots);seconds=$($run.seconds);style=$($run.style_mode)"
    foreach($key in @('progress_requested','trap_recovery_requested','hotspot_all_nodes_requested','physics_diagnostics_requested','reachable_goals_requested','sink_escape_requested','fire_diagnostics','move_trace_requested','bot_pace_requested','time_mode','requested_speed')) {
        $value=$run.$key;if($null -eq $value){$value='unknown'}
        $label+=";$key=$value"
    }
    foreach($key in @('mode','hooks','z_offset_requested')){if($run.PSObject.Properties.Name -contains $key){$label+=";$key=$($run.$key)"}}
    $label+=';roster='+(@($run.roster | Sort-Object number | ForEach-Object {"$($_.number):$($_.skill):$($_.style)"}) -join ',')
    $summaryArgs=@('-B',$TriageScript,'summary',(Join-Path $out $run.log),'--mem',(Join-Path $out 'initial-memory.mem'),'--label',$label,'--out',$summary)
    $graph=Join-Path $out "$($run.tag)-nav-1.json"
    if(Test-Path -LiteralPath $graph){$summaryArgs+=@('--graph',$graph)}
    & $PythonExecutable @summaryArgs | Out-Null
    if($LASTEXITCODE -ne 0){
        if(Test-Path -LiteralPath $summary){Remove-Item -LiteralPath $summary}
        throw "Steelmill Triage summary failed ($LASTEXITCODE): $($run.tag)"
    }
    $data=Get-Content -LiteralPath $summary -Raw | ConvertFrom-Json
    if($data.schema -ne 'warsbots-run-summary/1' -or $data.label -ne $label){
        Remove-Item -LiteralPath $summary
        throw "Unexpected run summary contract: $($run.tag)"
    }
    Write-Host "TRIAGE summary=$summary schema=$($data.schema)"
}
