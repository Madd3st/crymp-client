# Claude job runner v2. Polls ClaudeQA\inbox\*.json and hands each job to a FRESH Claude-Job.ps1
# process, so job logic can be updated without restarting this window.
# Close this window, or create ClaudeQA\stop.flag, to stop it.
$here = $PSScriptRoot
$inbox = Join-Path $here 'inbox'; $outbox = Join-Path $here 'outbox'
New-Item -ItemType Directory -Force $inbox, $outbox | Out-Null
Remove-Item -LiteralPath (Join-Path $here 'stop.flag') -ErrorAction SilentlyContinue
$consoleLog = Join-Path $here 'runner-console.log'
function Say($text) { $line = "$(Get-Date -Format T) $text"; Write-Host $line; Add-Content -LiteralPath $consoleLog -Value $line }
Say "Claude runner v2 started. Watching $inbox (this window shows every step; also in runner-console.log)"
while (-not (Test-Path (Join-Path $here 'stop.flag'))) {
    $job = Get-ChildItem $inbox -Filter '*.json' | Sort-Object LastWriteTime | Select-Object -First 1
    if (-not $job) { Start-Sleep -Seconds 3; continue }
    $work = Join-Path $outbox ('running-' + $job.Name)
    Move-Item -LiteralPath $job.FullName -Destination $work -Force
    Say "== job $($job.Name)"
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $here 'Claude-Job.ps1') -JobFile $work
    Remove-Item -LiteralPath $work -ErrorAction SilentlyContinue
    Say "== finished $($job.Name)"
}
Say 'stop.flag found; runner exits.'
