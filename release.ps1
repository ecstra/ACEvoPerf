# Builds the mod and zips the drag and drop payload into release\ACEvoPerf-<version>.zip
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
& "$root\build.ps1"

$version = (Get-Item "$root\dist\dstorage.dll").VersionInfo.FileVersion

# The changelog's newest heading has to be this version with its date, not the unreleased section.
$heading = Select-String -Path "$root\CHANGELOG.md" -Pattern '^## ' | Select-Object -First 1
$shortVersion = $version -replace '(\.0)+$', ''
if (-not $heading -or $heading.Line -match 'unreleased' -or $heading.Line -notmatch "^## $([regex]::Escape($shortVersion)) \(\d{4}-\d{2}-\d{2}\)") {
    throw "CHANGELOG.md's newest heading is '$($heading.Line)', it should be '## $shortVersion (date)' for this release"
}

# A developer switch left on in the shipped ini would write logs or CSVs into every player's game folder.
$section = ""
foreach ($line in Get-Content "$root\dist\acevo_perf.ini") {
    if ($line -match '^\[(.+)\]') { $section = $Matches[1]; continue }
    if ($section -ne "developer" -or $line -notmatch '^(\w+)\s*=\s*([^;\s]*)') { continue }
    if ($Matches[1] -eq "sample_us") { continue }
    if ($Matches[2] -ne "0") { throw "dist\acevo_perf.ini ships [developer] $($Matches[1])=$($Matches[2]), it has to be 0" }
}

New-Item -ItemType Directory -Force "$root\release" | Out-Null
$zip = "$root\release\ACEvoPerf-$version.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }

$payload = @("dstorage.dll", "dstorage_orig.dll", "acevo_dstoragecore.dll", "acevo_perf.ini", "README.txt") | ForEach-Object { Join-Path "$root\dist" $_ }
foreach ($f in $payload) { if (-not (Test-Path $f)) { throw "missing payload file $f" } }
Compress-Archive -Path $payload -DestinationPath $zip -CompressionLevel Optimal
Write-Host "Release: $zip"
Get-ChildItem $root\dist | Select-Object Name, Length | Format-Table -AutoSize
