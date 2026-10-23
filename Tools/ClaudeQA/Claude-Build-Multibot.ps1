# Claude QA helper: imports warsbots-multibot.bundle into a SEPARATE worktree and builds ON + OFF.
# Never touches the main checkout's working tree, the game root EXEs or running game processes.
param([switch]$OnOnly)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$wt = Join-Path (Split-Path $repo -Parent) 'crymp-multibot'
$log = Join-Path $repo 'build\claude-multibot-build.log'
$done = Join-Path $repo 'build\claude-multibot-build.done'
New-Item -ItemType Directory -Force (Split-Path $log) | Out-Null
Remove-Item -LiteralPath $done -ErrorAction SilentlyContinue
Start-Transcript -LiteralPath $log -Force | Out-Null
$result = 'FAIL'
try {
    Push-Location $repo
    $bundle = Join-Path $PSScriptRoot 'warsbots-multibot.bundle'
    git bundle verify $bundle
    if ($LASTEXITCODE -ne 0) { throw 'bundle verify failed' }
    git fetch $bundle '+refs/heads/warsbots/feature/multibot:refs/remotes/claude/multibot' '+refs/heads/warsbots/feature/ia-participants:refs/remotes/claude/ia-participants'
    if ($LASTEXITCODE -ne 0) { throw 'fetch failed' }
    if (-not (Test-Path -LiteralPath $wt)) {
        git worktree add -B warsbots/feature/multibot $wt refs/remotes/claude/multibot
        if ($LASTEXITCODE -ne 0) { throw 'worktree add failed' }
    } else {
        Push-Location $wt
        if (git status --porcelain --untracked-files=no) { throw "worktree $wt has local changes; not updating" }
        git checkout -B warsbots/feature/multibot refs/remotes/claude/multibot
        if ($LASTEXITCODE -ne 0) { throw 'worktree update failed' }
        Pop-Location
    }
    Pop-Location
    Write-Host "HEAD: $(git -C $wt log --oneline -1)"
    $jobs = [Math]::Max(2, [int]$env:NUMBER_OF_PROCESSORS)
    $build = Join-Path $wt 'Tools\Build-WarsBots.ps1'
    Write-Host '=== BUILD ON ==='
    & $build -Experimental -Jobs $jobs -BuildDirectory 'build/multibot-on'
    Write-Host 'BUILD ON: PASS'
    if (-not $OnOnly) {
    Write-Host '=== BUILD OFF ==='
    & $build -Jobs $jobs -BuildDirectory 'build/multibot-off'
    Write-Host 'BUILD OFF: PASS'
    }
    Get-ChildItem -Path (Join-Path $wt 'build') -Recurse -Filter 'CryMP-*64.exe' | ForEach-Object { Write-Host "EXE $($_.FullName) $((Get-FileHash $_.FullName).Hash)" }
    $result = 'PASS'
} catch {
    Write-Host "ERROR: $_"
} finally {
    Stop-Transcript | Out-Null
    Set-Content -LiteralPath $done -Value $result
}
