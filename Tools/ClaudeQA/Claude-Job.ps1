# One Claude job. Fixed job types only (no arbitrary commands):
#   build: on|both|none      import ClaudeQA\warsbots-multibot.bundle into ..\crymp-multibot, build
#   qa:    [{bots,seconds,tag}]  headless loopback IA server on port 64120, no client
#   push:  ["multibot","ia-participants"]  plain (non-force) push to the user's fork feature branches
# Game directory comes from ClaudeQA\game-dir.txt (local, not in Git).
param([Parameter(Mandatory)][string]$JobFile)
$ErrorActionPreference = 'Continue'
$here = $PSScriptRoot
$repo = Split-Path $here -Parent
$wt = Join-Path (Split-Path $repo -Parent) 'crymp-multibot'
$outbox = Join-Path $here 'outbox'
try { $spec = Get-Content -LiteralPath $JobFile -Raw | ConvertFrom-Json } catch { $spec = $null }
$id = if ($spec -and $spec.id) { [string]$spec.id } else { 'bad-json' }
if ($id -notmatch '^[A-Za-z0-9_-]{1,40}$') { $id = 'bad-id' }
$res = [ordered]@{ id = $id; started = (Get-Date).ToString('s'); steps = @() }
function Step($name, $result) { $script:res.steps += [ordered]@{ step = $name; result = [string]$result }; Write-Host "   $name -> $result" }
if ($spec) {
    $build = [string]$spec.build
    if ($build -in @('on','both')) {
        $psArgs = @('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $here 'Claude-Build-Multibot.ps1'))
        if ($build -eq 'on') { $psArgs += '-OnOnly' }
        if ($spec.expect) { $psArgs += @('-Expect', [string]$spec.expect) }
        & powershell @psArgs | Out-Null
        $done = Get-Content -LiteralPath (Join-Path $repo 'build\claude-multibot-build.done') -ErrorAction SilentlyContinue
        Step "build-$build" $done
        if ($done -ne 'PASS') { $spec.qa = @(); $spec.push = @() }
    }
    foreach ($q in @($spec.qa)) {
        if (-not $q) { continue }
        $bots = [Math]::Min(16, [Math]::Max(1, [int]$q.bots))
        $secs = [Math]::Min(900, [Math]::Max(30, [int]$q.seconds))
        $tag = [string]$q.tag; if ($tag -notmatch '^[A-Za-z0-9_-]{1,20}$') { $tag = 'qa' }
        & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $here 'Claude-QA-Bots.ps1') -Bots $bots -Seconds $secs -Tag $tag | Out-Null
        Step "qa-$tag-$bots" (Get-Content -LiteralPath (Join-Path $repo "build\claude-qa\$tag.done") -ErrorAction SilentlyContinue)
    }
    foreach ($p in @($spec.push)) {
        if ($p -eq 'multibot') {
            $out = git -C $wt push origin 'HEAD:refs/heads/warsbots/feature/multibot' 2>&1 | Out-String
            Step 'push-multibot' "exit=$LASTEXITCODE $($out.Trim())"
        } elseif ($p -eq 'ia-participants') {
            $out = git -C $repo push origin 'refs/remotes/claude/ia-participants:refs/heads/warsbots/feature/ia-participants' 2>&1 | Out-String
            Step 'push-ia-participants' "exit=$LASTEXITCODE $($out.Trim())"
        }
    }
}
$res.head = [string](git -C $wt log --oneline -1 2>$null)
$res.remote = [string](git -C $wt ls-remote origin 'refs/heads/warsbots/feature/*' 2>&1 | Out-String)
$res.finished = (Get-Date).ToString('s')
$res | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $outbox "$id.json") -Encoding UTF8
