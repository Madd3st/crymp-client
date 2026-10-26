# Record only the exact inbox version actually processed, including manual desktop turns.
[CmdletBinding()] param([Parameter(Mandatory)][string]$Repo,[Parameter(Mandatory)][int]$Item,[Parameter(Mandatory)][string]$SourceRef)
$ErrorActionPreference='Stop'
$resolved=(git -C $Repo rev-parse $SourceRef).Trim();if($LASTEXITCODE -or $resolved -notmatch '^[0-9a-f]{40}$'){throw 'Immutable processed research ref required'}
$text=(git -C $Repo show ($resolved+':Docs/Research/Coordination/MAILBOX/TO_CODEX.md'))-join "`n";if($LASTEXITCODE){throw 'Inbox source missing'}
$pattern='(?ms)^### C-0*'+$Item+'[^\r\n]*\r?\n.*?(?=^### C-|\z)';$m=[regex]::Match($text,$pattern);if(-not $m.Success){throw 'Item missing in processed source'}
$h=[Security.Cryptography.SHA256]::Create();try{$hash=([BitConverter]::ToString($h.ComputeHash([Text.Encoding]::UTF8.GetBytes($m.Value)))).Replace('-','')}finally{$h.Dispose()}
$file=Join-Path $env:LOCALAPPDATA 'WarsBots/Mailbox/wake-codex-state.json'
$state=if(Test-Path $file){Get-Content $file -Raw|ConvertFrom-Json}else{[pscustomobject]@{sections=[pscustomobject]@{}}}
$state.sections|Add-Member -NotePropertyName ([string]$Item) -NotePropertyValue $hash -Force
$state|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $file -Encoding UTF8
[pscustomobject]@{item=$Item;processed_ref=$resolved;section_sha256=$hash;note='Only processed ref acknowledged; later amended SHA/text remains new'}|ConvertTo-Json
