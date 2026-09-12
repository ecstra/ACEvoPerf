# Builds the mod and zips the drag and drop payload into release\ACEvoPerf-<version>.zip
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
& "$root\build.ps1"

$version = (Get-Item "$root\dist\dstorage.dll").VersionInfo.FileVersion
New-Item -ItemType Directory -Force "$root\release" | Out-Null
$zip = "$root\release\ACEvoPerf-$version.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }

$payload = @("dstorage.dll", "dstorage_orig.dll", "acevo_dstoragecore.dll", "acevo_perf.ini", "README.txt") | ForEach-Object { Join-Path "$root\dist" $_ }
foreach ($f in $payload) { if (-not (Test-Path $f)) { throw "missing payload file $f" } }
Compress-Archive -Path $payload -DestinationPath $zip -CompressionLevel Optimal
Write-Host "Release: $zip"
Get-ChildItem $root\dist | Select-Object Name, Length | Format-Table -AutoSize
