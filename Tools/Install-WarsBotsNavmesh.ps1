# Install only manifest-verified Detour data; canonical LF text is accepted from CRLF checkouts.
[CmdletBinding()] param([string]$Source = (Join-Path $PSScriptRoot 'RecastBake/meshes'))
$ErrorActionPreference='Stop'
function Hash-Bytes([byte[]]$Bytes) {
 $sha=[Security.Cryptography.SHA256]::Create()
 try { return ([BitConverter]::ToString($sha.ComputeHash($Bytes))).Replace('-','').ToLowerInvariant() } finally {$sha.Dispose()}
}
$staged=@()
foreach($manifestFile in @(Get-ChildItem -LiteralPath $Source -Filter '*.manifest.json' -File)) {
 $manifest=Get-Content -LiteralPath $manifestFile.FullName -Raw|ConvertFrom-Json
 foreach($entry in $manifest.files.PSObject.Properties) {
  $f=$entry.Value
  if([IO.Path]::GetFileName($f.name) -ne $f.name){throw 'Unsafe manifest filename'}
  $path=Join-Path $Source $f.name
  [byte[]]$bytes=[IO.File]::ReadAllBytes($path)
  if((Hash-Bytes $bytes) -ne $f.sha256 -and $f.name -match '\.(links|spots)\.txt$') {
   $bytes=[Text.Encoding]::UTF8.GetBytes([Text.Encoding]::UTF8.GetString($bytes).Replace("`r`n","`n"))
  }
  if($bytes.Length -ne $f.bytes -or (Hash-Bytes $bytes) -ne $f.sha256){throw "Navmesh manifest mismatch: $path"}
  $staged+=@{name=$f.name;bytes=$bytes;sha256=$f.sha256}
 }
 $staged+=@{name=$manifestFile.Name;bytes=[IO.File]::ReadAllBytes($manifestFile.FullName);sha256=(Get-FileHash -LiteralPath $manifestFile.FullName).Hash.ToLowerInvariant()}
}
if(-not $staged.Count){throw 'No navmesh manifests; installation refused'}
$target=Join-Path $env:LOCALAPPDATA 'WarsBots/Navmesh'
New-Item -ItemType Directory -Force -Path $target|Out-Null
foreach($item in $staged){
 $dest=Join-Path $target $item.name
 [IO.File]::WriteAllBytes($dest,$item.bytes)
 if((Get-FileHash -LiteralPath $dest).Hash.ToLowerInvariant() -ne $item.sha256){throw "Installed hash mismatch: $dest"}
 Write-Output ("{0} {1} bytes {2}" -f $item.name,$item.bytes.Length,$item.sha256)
}
Write-Output "Installed verified data to $target"
