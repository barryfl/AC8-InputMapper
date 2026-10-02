# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Fred Barry
$ErrorActionPreference='Stop'
$project=$PSScriptRoot
$name='AC8-InputMapper-v0.2.0-experimental'
$folder=Join-Path $project "dist\$name"
$zip="$folder.zip"
if((Test-Path -LiteralPath $folder) -or (Test-Path -LiteralPath $zip)) {throw 'Release output already exists; select a clean output directory.'}
New-Item -ItemType Directory -Path (Join-Path $folder 'tools') -Force | Out-Null
$items=@{
 'build\compatibility\dinput8.dll'='dinput8.dll';
 'AC8InputMapper.example.ini'='AC8InputMapper.ini';
 'build\controller-list\List-Controllers.exe'='tools\List-Controllers.exe';
 'List-Controllers.cmd'='tools\List-Controllers.cmd';
 'README.md'='README.md'; 'SETUP.txt'='SETUP.txt'; 'MULTI-DEVICE.txt'='MULTI-DEVICE.txt'; 'LICENSE'='LICENSE'
}
foreach($src in $items.Keys) {Copy-Item -LiteralPath (Join-Path $project $src) -Destination (Join-Path $folder $items[$src])}
$ini=Get-Content -LiteralPath (Join-Path $folder 'AC8InputMapper.ini') -Raw
if($ini -notmatch 'Enabled=0' -or $ini -notmatch 'REPLACE-WITH-STICK-INSTANCE-GUID' -or $ini -notmatch 'REPLACE-WITH-THROTTLE-INSTANCE-GUID') {throw 'Public INI must be blank and disabled'}
$files=Get-ChildItem -LiteralPath $folder -Recurse -File | Sort-Object FullName
$manifest=foreach($file in $files) {
 (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLower()+'  '+$file.FullName.Substring($folder.Length+1).Replace('\','/')
}
$manifest | Set-Content -LiteralPath (Join-Path $folder 'SHA256SUMS.txt') -Encoding ASCII
Compress-Archive -LiteralPath $folder -DestinationPath $zip -CompressionLevel Optimal
Write-Host "Release candidate: $zip"
Get-FileHash -LiteralPath $zip -Algorithm SHA256

