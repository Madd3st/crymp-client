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
$triageArgs=@('-B',$TriageScript,'analyze')+$logs+@('--mem',(Join-Path $out 'initial-memory.mem'),'--out',$out)
$graph=Get-ChildItem -LiteralPath $out -Filter '*-nav-1.json' | Sort-Object Name | Select-Object -First 1
if($graph){$triageArgs+=@('--graph',$graph.FullName)}
# Never leave an old summary looking current after a failed rerun.
$summary=Join-Path $out 'summary.json'
if(Test-Path -LiteralPath $summary){Remove-Item -LiteralPath $summary}
& $PythonExecutable @triageArgs
if($LASTEXITCODE -ne 0){throw "Steelmill Triage failed ($LASTEXITCODE)"}
$report=Join-Path $out 'triage_report.json'
$data=Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
if($data.schema -ne 'steelmill-triage/1' -or @($data.runs).Count -lt $logs.Count){throw 'Unexpected/incomplete native Triage report'}
# Preserve native schema; replace adapter only when Claude publishes summary contract.
Copy-Item -LiteralPath $report -Destination $summary
Write-Host "TRIAGE summary=$summary schema=$($data.schema)"
