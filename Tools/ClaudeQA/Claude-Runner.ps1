# Claude job runner. Polls ClaudeQA\inbox\*.json; runs ONLY these fixed job types:
#   import+build (bundle -> separate worktree ..\crymp-multibot; build on|off|both|none)
#   qa: list of { bots, seconds, tag }  (headless loopback server, port 64120, no client)
# Writes ClaudeQA\outbox\<id>.json. Never runs arbitrary commands, never touches the game root
# EXEs or the user's own server/client. Close this window (or create ClaudeQA\stop.flag) to stop.
$ErrorActionPreference = 'Continue'
$here = $PSScriptRoot
$inbox = Join-Path $here 'inbox'; $outbox = Join-Path $here 'outbox'
New-Item -ItemType Directory -Force $inbox, $outbox | Out-Null
$repo = Split-Path $here -Parent
Write-Host "Claude runner started $(Get-Date). Watching $inbox"
while (-not (Test-Path (Join-Path $here 'stop.flag'))) {
    $job = Get-ChildItem $inbox -Filter '*.json' | Sort-Object LastWriteTime | Select-Object -First 1
    if (-not $job) { Start-Sleep -Seconds 3; continue }
    $started = Get-Date
    try { $spec = Get-Content -LiteralPath $job.FullName -Raw | ConvertFrom-Json } catch { $spec = $null }
    Remove-Item -LiteralPath $job.FullName
    $id = if ($spec -and $spec.id) { [string]$spec.id } else { $job.BaseName }
    if ($id -notmatch '^[A-Za-z0-9_-]{1,40}$') { $id = 'bad-id' }
    $res = [ordered]@{ id = $id; started = $started.ToString('s'); steps = @() }
    Write-Host "== job $id"
    if ($spec) {
        $build = [string]$spec.build
        if ($build -in @('on','off','both')) {
            $psArgs = @('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $here 'Claude-Build-Multibot.ps1'))
            if ($build -eq 'on') { $psArgs += '-OnOnly' }
            & powershell @psArgs | Out-Null
            $done = Get-Content -LiteralPath (Join-Path $repo 'build\claude-multibot-build.done') -ErrorAction SilentlyContinue
            $res.steps += [ordered]@{ step = "build-$build"; result = [string]$done }
            if ($build -eq 'off') { $res.steps += [ordered]@{ step='note'; result='off-only not supported; ran on+off' } }
            if ($done -ne 'PASS') { $spec.qa = @() }
        }
        foreach ($q in @($spec.qa)) {
            if (-not $q) { continue }
            $bots = [Math]::Min(16, [Math]::Max(1, [int]$q.bots))
            $secs = [Math]::Min(900, [Math]::Max(30, [int]$q.seconds))
            $tag = [string]$q.tag; if ($tag -notmatch '^[A-Za-z0-9_-]{1,20}$') { $tag = 'qa' }
            & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $here 'Claude-QA-Bots.ps1') -Bots $bots -Seconds $secs -Tag $tag | Out-Null
            $qd = Get-Content -LiteralPath (Join-Path $repo "build\claude-qa\$tag.done") -ErrorAction SilentlyContinue
            $res.steps += [ordered]@{ step = "qa-$tag-$bots"; result = [string]$qd }
        }
    }
    $head = git -C (Join-Path (Split-Path $repo -Parent) 'crymp-multibot') log --oneline -1 2>$null
    $res.head = [string]$head
    $res.finished = (Get-Date).ToString('s')
    $res | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $outbox "$id.json") -Encoding UTF8
    Write-Host "== done $id : $($res.steps | ForEach-Object { $_.step + '=' + $_.result })"
}
Write-Host 'stop.flag found; runner exits.'
