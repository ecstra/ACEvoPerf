# Builds dist\dstorage.dll (ACEvoPerf proxy) with the installed MSVC + Windows SDK.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$vsRoot = "C:\Program Files\Microsoft Visual Studio\2022"
$vc = Get-ChildItem "$vsRoot\*\VC\Tools\MSVC" -ErrorAction Stop | Select-Object -First 1
$msvc = Get-ChildItem $vc.FullName | Sort-Object Name -Descending | Select-Object -First 1
$kits = "C:\Program Files (x86)\Windows Kits\10"
$sdkVer = (Get-ChildItem "$kits\Include" | Where-Object { Test-Path "$($_.FullName)\um\d3d12.h" } | Sort-Object Name -Descending | Select-Object -First 1).Name
Write-Host "MSVC: $($msvc.FullName)"
Write-Host "SDK : $sdkVer"
$env:INCLUDE = "$($msvc.FullName)\include;$kits\Include\$sdkVer\ucrt;$kits\Include\$sdkVer\um;$kits\Include\$sdkVer\shared;$kits\Include\$sdkVer\winrt"
$env:LIB = "$($msvc.FullName)\lib\x64;$kits\Lib\$sdkVer\ucrt\x64;$kits\Lib\$sdkVer\um\x64"
$env:PATH = "$($msvc.FullName)\bin\Hostx64\x64;$kits\bin\$sdkVer\x64;$env:PATH"
New-Item -ItemType Directory -Force "$root\dist" | Out-Null
New-Item -ItemType Directory -Force "$root\build" | Out-Null
Push-Location "$root\build"
try {
    & rc.exe /nologo /fo "$root\build\version.res" "$root\src\version.rc"
    if ($LASTEXITCODE -ne 0) { throw "rc.exe failed with $LASTEXITCODE" }
    $sources = Get-ChildItem "$root\src" -Recurse -Filter *.cpp | ForEach-Object { $_.FullName }
    & cl.exe /nologo /O2 /W4 /MT /EHsc /std:c++17 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX `
        /I"$root\include" /I"$root\third_party\directstorage" /Fo"$root\build\\" $sources `
        /link /DLL /MACHINE:X64 /DEF:"$root\src\exports.def" /OUT:"$root\dist\dstorage.dll" `
        /IMPLIB:"$root\build\dstorage_proxy.lib" /PDB:"$root\build\dstorage_proxy.pdb" /DEBUG:FULL /OPT:REF /OPT:ICF `
        "$root\build\version.res" kernel32.lib user32.lib
    if ($LASTEXITCODE -ne 0) { throw "cl.exe failed with $LASTEXITCODE" }
} finally { Pop-Location }
# the zip payload also needs the original Microsoft runtime under the name the proxy forwards to
Copy-Item "$root\third_party\directstorage\bin\x64\dstorage.dll" "$root\dist\dstorage_orig.dll" -Force
Write-Host "Built: $root\dist\dstorage.dll (+ dstorage_orig.dll)"
