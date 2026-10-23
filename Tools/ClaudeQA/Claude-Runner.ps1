# Claude job runner v2. Polls ClaudeQA\inbox\*.json and hands each job to a FRESH Claude-Job.ps1
# process, so job logic can be updated without restarting this window.
# Close this window, or create ClaudeQA\stop.flag, to stop it.
$here = $PSScriptRoot
$inbox = Join-Path $here 'inbox'; $outbox = Join-Path $here 'outbox'
New-Item -ItemType Directory -Force $inbox, $outbox | Out-Null
Remove-Item -LiteralPath (Join-Path $here 'stop.flag') -ErrorAction SilentlyContinue
Write-Host "Claude runner v2 started $(Get-Date). Watching $inbox"
while (-not (Test-Path (Join-Path $here 'stop.flag'))) {
    $job = Get-ChildItem $inbox -Filter '*.json' | Sort-Object LastWriteTime | Select-Object -First 1
    if (-not $job) { Start-Sleep -Seconds 3; continue }
    $work = Join-Path $outbox ('running-' + $job.Name)
    Move-Item -LiteralPath $job.FullName -Destination $work -Force
    Write-Host "== $(Get-Date -Format T) job $($job.Name)"
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $here 'Claude-Job.ps1') -JobFile $work
    Remove-Item -LiteralPath $work -ErrorAction SilentlyContinue
    Write-Host "== $(Get-Date -Format T) finished $($job.Name)"
}
Write-Host 'stop.flag found; runner exits.'
