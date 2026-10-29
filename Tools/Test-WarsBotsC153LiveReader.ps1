# No game: replay the actual parser and actual new reader with a two-fragment archived log.
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$file=Join-Path $repo 'Tools/Invoke-WarsBotsC153Batch.ps1'
$source=[IO.File]::ReadAllText($file)
$tokens=$null;$errors=$null;$ast=[System.Management.Automation.Language.Parser]::ParseFile($file,[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'AST failed'}
$reader=$ast.EndBlock.Statements|Where-Object {$_ -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $_.Name -eq 'Read-LiveLog'}
. ([scriptblock]::Create($reader.Extent.Text))
$from=$source.IndexOf('  $pickupActors=@(');$to=$source.IndexOf('  $elapsed=',$from)
if($from -lt 0 -or $to -le $from){throw 'Actual parser not found'}
$parse=[scriptblock]::Create($source.Substring($from,$to-$from))
$script:fixtureReadPath=Join-Path $repo 'build/c153-batch-5ef9ee22/C153-5ef9ee22.bots.log'
$full=[IO.File]::ReadAllText($script:fixtureReadPath)
$script:pieces=@($full.Substring(0,$full.Length-100),$full.Substring($full.Length-100))
$out=Join-Path $repo 'build/c153-live-reader-control';New-Item -ItemType Directory -Force $out|Out-Null
$side=$script:pieces;$rosterAt=[DateTime]::UtcNow;$rosterSignature='';$oldFailed=$false
try{. $parse}catch{$oldFailed=$_.Exception.Message -match 'Substring|startIndex'}
if(-not $oldFailed){throw 'Expected fragmented old-parser failure was not reproduced'}
function Get-Content {
 param([string]$LiteralPath,[switch]$Raw,[string]$Encoding)
 if($LiteralPath -ne $script:fixtureReadPath -or -not $Raw){throw 'Unexpected fixture read'}
 return $script:pieces
}
try{
 $side=Read-LiveLog $script:fixtureReadPath
 if($side -isnot [string] -or $side -cne $full){throw 'Reader did not preserve one exact scalar'}
 . $parse
 if($currentIds.Count -ne 8 -or $currentPickups.Count -ne 7){throw 'Archive parser result changed'}
 $script:pieces=@();[string]$edgeValue=Read-LiveLog $script:fixtureReadPath
 if($edgeValue.Length -ne 0){throw 'Empty live file control failed'}
 $script:pieces=@($full);[string]$edgeValue=Read-LiveLog $script:fixtureReadPath
 if($edgeValue -cne $full){throw 'Single fragment control failed'}
 @{old_fragmented_parser='RED same Substring error';new_actual_reader='GREEN exact scalar';empty_and_single_fragment='PASS';chunks=2;current_roster=8;archived_pickup_actors=7;game_started=$false}|ConvertTo-Json|Set-Content (Join-Path $out 'result.json') -Encoding UTF8
 Write-Output 'Actual chunked input reader/parser RED old / GREEN new; no game'
}finally{Remove-Item -LiteralPath Function:/Get-Content}
