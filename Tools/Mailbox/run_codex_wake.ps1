param([Parameter(Mandatory)][string]$Repo,[Parameter(Mandatory)][string]$ThreadId,[Parameter(Mandatory)][string]$CodexCmd,[Parameter(Mandatory)][string]$PromptFile)
$ErrorActionPreference='Continue';Set-Location -LiteralPath $Repo
$dir=Join-Path $env:LOCALAPPDATA 'WarsBots/Mailbox';$log=Join-Path $dir ('codex-'+(Get-Date -Format yyyyMMdd-HHmmss)+'.jsonl')
$prompt=[IO.File]::ReadAllText($PromptFile)
$prompt | & $CodexCmd --approve-for-me exec -C $Repo resume --json $ThreadId - *> $log
@{exit=$LASTEXITCODE;thread=$ThreadId;log=$log;finished=[DateTime]::UtcNow.ToString('o')}|ConvertTo-Json|Set-Content (Join-Path $dir 'codex-last-completion.json') -Encoding UTF8
