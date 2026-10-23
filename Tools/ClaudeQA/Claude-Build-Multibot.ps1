# Claude QA helper: BUILD step only.
# - Sources: warsbots/feature/multibot fetched from GitHub (local bundle only as offline fallback)
#   into a separate worktree OUTSIDE OneDrive: %USERPROFILE%\WarsBotsBuild\crymp-multibot
#   (OneDrive syncing thousands of .obj/.ninja_* files made every "incremental" build a full one).
# - -Expect <sha prefix> refuses to build anything else.
# - Reuse: if build\multibot-on\claude-build.json records the same commit and configuration and the
#   EXEs still match their recorded SHA256, nothing is compiled ("REUSE").
# - Before each build a ninja dry run with -d explain records why targets would rebuild
#   (build\claude-multibot-explain-<on|off>.log).
param([switch]$OnOnly, [string]$Expect = '', [switch]$Force)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$root = if ($env:WARSBOTS_BUILD_ROOT) { $env:WARSBOTS_BUILD_ROOT } else { Join-Path $env:USERPROFILE 'WarsBotsBuild' }
$wt = Join-Path $root 'crymp-multibot'
$log = Join-Path $repo 'build\claude-multibot-build.log'
$done = Join-Path $repo 'build\claude-multibot-build.done'
$compileLog = Join-Path $repo 'build\claude-multibot-compile.log'
New-Item -ItemType Directory -Force (Split-Path $log), $root | Out-Null
Remove-Item -LiteralPath $done, $compileLog -ErrorAction SilentlyContinue
Start-Transcript -LiteralPath $log -Force | Out-Null
$result = 'FAIL'
function Hash($path) { (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash }
try {
    Push-Location $repo
    git fetch origin '+refs/heads/warsbots/feature/multibot:refs/remotes/claude/multibot'
    if ($LASTEXITCODE -ne 0) {
        Write-Host 'GitHub fetch failed; using the local bundle.'
        git fetch (Join-Path $PSScriptRoot 'warsbots-multibot.bundle') '+refs/heads/warsbots/feature/multibot:refs/remotes/claude/multibot'
        if ($LASTEXITCODE -ne 0) { throw 'fetch failed (GitHub and bundle)' }
    }
    $target = (git rev-parse refs/remotes/claude/multibot).Trim()
    Write-Host "TARGET: $target"
    if ($Expect -and -not $target.StartsWith($Expect)) { throw "TARGET $target is not the expected $Expect" }
    if (-not (Test-Path -LiteralPath (Join-Path $wt '.git'))) {
        git worktree prune
        # Detached at the exact commit: the branch itself may be checked out by another worktree.
        git worktree add --detach $wt $target
        if ($LASTEXITCODE -ne 0) { throw 'worktree add failed' }
    } else {
        if (git -C $wt status --porcelain --untracked-files=no) { throw "worktree $wt has local changes; not updating" }
        git -C $wt checkout --detach $target
        if ($LASTEXITCODE -ne 0) { throw 'worktree update failed' }
    }
    Pop-Location
    $head = (git -C $wt rev-parse HEAD).Trim()
    Write-Host "HEAD: $(git -C $wt log --oneline -1)"
    if ($head -ne $target) { throw "worktree HEAD $head != target $target" }

    $configs = @(@{ name = 'on'; dir = 'build/multibot-on'; args = @('-Experimental') })
    if (-not $OnOnly) { $configs += @{ name = 'off'; dir = 'build/multibot-off'; args = @() } }
    if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
        $vs = & (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe') -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vs) { & (Join-Path $vs 'Common7/Tools/Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null }
    }
    $jobs = [Math]::Max(2, [int]$env:NUMBER_OF_PROCESSORS)
    $build = Join-Path $wt 'Tools\Build-WarsBots.ps1'
    foreach ($c in $configs) {
        $dir = Join-Path $wt $c.dir
        $manifestPath = Join-Path $dir 'claude-build.json'
        $exes = @('CryMP-Server64.exe', 'CryMP-Client64.exe') | ForEach-Object { Join-Path $dir $_ }
        $config = "Release Ninja MSVC x64 WARSBOTS=$($c.name.ToUpper())"
        if (-not $Force -and (Test-Path $manifestPath) -and -not ($exes | Where-Object { -not (Test-Path $_) })) {
            $m = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
            if ($m.sha -eq $head -and $m.config -eq $config -and $m.server -eq (Hash $exes[0]) -and $m.client -eq (Hash $exes[1])) {
                Write-Host "BUILD $($c.name.ToUpper()): REUSE $head (EXE hashes match the manifest)"
                continue
            }
        }
        Write-Host "=== BUILD $($c.name.ToUpper()) ==="
        # Why will ninja rebuild? Dry run with -d explain BEFORE building (needs the VS environment,
        # which Build-WarsBots.ps1 sets up in this session; ninja writes explain to stderr, so cmd
        # redirects it to a file instead of PowerShell turning it into a terminating error).
        $explainLog = Join-Path $repo "build\claude-multibot-explain-$($c.name).log"
        if ((Test-Path (Join-Path $dir 'build.ninja')) -and (Get-Command ninja -ErrorAction SilentlyContinue)) {
            cmd /c "ninja -C `"$dir`" -n -d explain > `"$explainLog`" 2>&1"
            $reasons = Select-String -LiteralPath $explainLog -Pattern 'ninja explain:' | Select-Object -First 6
            $count = (Select-String -LiteralPath $explainLog -Pattern '^\[\d+/(\d+)\]' | Select-Object -Last 1)
            Write-Host "Planned: $(if ($count) { $count.Line } else { 'nothing' })"
            $reasons | ForEach-Object { Write-Host "  $($_.Line)" }
        }
        $buildArgs = @{ Jobs = $jobs; BuildDirectory = $c.dir }
        if ($c.args -contains '-Experimental') { $buildArgs.Experimental = $true }
        & $build @buildArgs *>&1 | ForEach-Object { $l = "$_"; Write-Host $l; Add-Content -LiteralPath $compileLog -Value $l }
        [ordered]@{ sha = $head; config = $config; server = (Hash $exes[0]); client = (Hash $exes[1]); built = (Get-Date).ToString('s') } |
            ConvertTo-Json | Set-Content -LiteralPath $manifestPath -Encoding UTF8
        Write-Host "BUILD $($c.name.ToUpper()): PASS $head"
    }
    $result = 'PASS'
} catch {
    Write-Host "ERROR: $_"
} finally {
    Stop-Transcript | Out-Null
    Set-Content -LiteralPath $done -Value $result
}
