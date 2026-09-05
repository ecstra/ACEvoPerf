# ACEvoPerf installer. Usage:
#   powershell -ExecutionPolicy Bypass -File install.ps1 [-GameDir "C:\path\to\Assetto Corsa EVO"]
param([string]$GameDir = "")
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path

if (-not $GameDir) {
    $candidates = @(
        "C:\InfinityX\Games\Assetto Corsa EVO",
        "C:\Program Files (x86)\Steam\steamapps\common\Assetto Corsa EVO",
        "C:\Program Files\Steam\steamapps\common\Assetto Corsa EVO"
    )
    foreach ($c in $candidates) { if (Test-Path "$c\AssettoCorsaEVO.exe") { $GameDir = $c; break } }
}
if (-not $GameDir -or -not (Test-Path "$GameDir\AssettoCorsaEVO.exe")) {
    throw "Game folder not found. Pass -GameDir 'C:\path\to\Assetto Corsa EVO'."
}
if (Get-Process -Name AssettoCorsaEVO -ErrorAction SilentlyContinue) { throw "Close Assetto Corsa EVO first." }

$dll  = Join-Path $GameDir "dstorage.dll"
$orig = Join-Path $GameDir "dstorage_orig.dll"

function Is-MicrosoftDStorage($path) {
    if (-not (Test-Path $path)) { return $false }
    $v = (Get-Item $path).VersionInfo
    return ($v.ProductName -eq "DirectStorage") -and ($v.CompanyName -like "*Microsoft*")
}

if (-not (Test-Path $orig)) {
    if (-not (Is-MicrosoftDStorage $dll)) { throw "$dll is not the original Microsoft DirectStorage runtime and no dstorage_orig.dll exists. Verify game files first." }
    Rename-Item $dll $orig
    Write-Host "Renamed original runtime -> dstorage_orig.dll ($((Get-Item $orig).VersionInfo.FileVersion))"
} elseif (Is-MicrosoftDStorage $dll) {
    # a game update restored the original dstorage.dll; keep the newer one as orig
    Remove-Item $orig -Force
    Rename-Item $dll $orig
    Write-Host "Game update detected: refreshed dstorage_orig.dll"
}

Copy-Item (Join-Path $here "dstorage.dll") $dll -Force
Write-Host "Installed proxy dstorage.dll"

$ini = Join-Path $GameDir "acevo_perf.ini"
if (-not (Test-Path $ini)) { Copy-Item (Join-Path $here "acevo_perf.ini") $ini; Write-Host "Installed acevo_perf.ini (defaults)" }
else { Write-Host "Kept existing acevo_perf.ini" }

Write-Host "Done. Start the game normally; acevo_perf.log appears next to the exe."
