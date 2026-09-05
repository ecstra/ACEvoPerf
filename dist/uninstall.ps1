# ACEvoPerf uninstaller: restores the original dstorage.dll and removes mod files.
param([string]$GameDir = "")
$ErrorActionPreference = "Stop"
if (-not $GameDir) {
    foreach ($c in @("C:\InfinityX\Games\Assetto Corsa EVO",
                     "C:\Program Files (x86)\Steam\steamapps\common\Assetto Corsa EVO",
                     "C:\Program Files\Steam\steamapps\common\Assetto Corsa EVO")) {
        if (Test-Path "$c\AssettoCorsaEVO.exe") { $GameDir = $c; break }
    }
}
if (-not $GameDir -or -not (Test-Path "$GameDir\AssettoCorsaEVO.exe")) { throw "Game folder not found. Pass -GameDir." }
if (Get-Process -Name AssettoCorsaEVO -ErrorAction SilentlyContinue) { throw "Close Assetto Corsa EVO first." }

$dll  = Join-Path $GameDir "dstorage.dll"
$orig = Join-Path $GameDir "dstorage_orig.dll"
if (Test-Path $orig) {
    Remove-Item $dll -Force -ErrorAction SilentlyContinue
    Rename-Item $orig $dll
    Write-Host "Restored original dstorage.dll"
} else {
    Write-Host "dstorage_orig.dll not found; nothing to restore (verify game files if dstorage.dll is missing)."
}
foreach ($f in @("acevo_perf.ini", "acevo_perf.log")) {
    $p = Join-Path $GameDir $f
    if (Test-Path $p) { Remove-Item $p -Force; Write-Host "Removed $f" }
}
Write-Host "Done."
