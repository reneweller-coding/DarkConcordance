<#
.SYNOPSIS
    Packs DARK CONCORDANCE for handing out: dist\DarkConcordance-<version>-windows-x64.zip and, through WSL, the
    Linux archive.

.DESCRIPTION
    .\Deploy\build_release.ps1 [-NoBuild] [-NoLinux]

    Builds the Release (build.ps1), stages the game as DarkConcordance.exe with its assets (without assets\raw, the
    sources the textures were made from), the player's readme, the manual (Handbuch.pdf, Manual.pdf), the licence
    and the credits in work\release and zips them into dist. The Linux archive comes from Deploy/build_linux.sh in WSL (GCC 12, glibc 2.35 and newer, the
    Steam Deck included; dav1d travels along in lib/); without WSL it is skipped.

    -NoBuild   pack what bin\msvc holds
    -NoLinux   only the Windows archive

.EXAMPLE
    .\Deploy\build_release.ps1
#>
param(
    [switch]$NoBuild,
    [switch]$NoLinux
)
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$version = (Select-String -Path (Join-Path $root "CMakeLists.txt") -Pattern 'project\( SpaceOpera VERSION ([0-9.]+)').Matches[0].Groups[1].Value
Write-Host "DARK CONCORDANCE $version"

if (-not $NoBuild) {
    & (Join-Path $root "build.ps1")
    if ($LASTEXITCODE -ne 0) { throw "build failed" }
}

# Windows: the exe (static runtime, dav1d inside) and the assets beside it.
$name = "DarkConcordance-$version-windows-x64"
$stage = Join-Path $root "work\release\$name"
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force $stage | Out-Null
Copy-Item (Join-Path $root "bin\msvc\SpaceOpera.exe") (Join-Path $stage "DarkConcordance.exe")
& robocopy (Join-Path $root "assets") (Join-Path $stage "assets") /E /XD (Join-Path $root "assets\raw") /NFL /NDL /NJH /NJS /NP | Out-Null
if ($LASTEXITCODE -ge 8) { throw "copying the assets failed" }
Copy-Item (Join-Path $root "docs\README-player.txt") (Join-Path $stage "README.txt")
foreach ($doc in "LICENSE", "docs\CREDITS.txt") { Copy-Item (Join-Path $root $doc) $stage }
# The manual in both languages (tools/manual/make_manual.py), where it is made.
foreach ($pair in @(@("DarkConcordance-Handbuch.pdf", "Handbuch.pdf"), @("DarkConcordance-Manual.pdf", "Manual.pdf"))) {
    $pdf = Join-Path $root "docs\manual\$($pair[0])"
    if (Test-Path $pdf) { Copy-Item $pdf (Join-Path $stage $pair[1]) }
}
$dist = Join-Path $root "dist"
New-Item -ItemType Directory -Force $dist | Out-Null
$zip = Join-Path $dist "$name.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
# Windows' own tar (bsdtar, Windows 10 and later; another tar may come first on the PATH) zips faster than
# Compress-Archive and copes with large archives.
& (Join-Path $env:SystemRoot "System32\tar.exe") -a -c -f $zip -C (Join-Path $root "work\release") $name
if ($LASTEXITCODE -ne 0) { throw "zip failed" }
Write-Host ("{0}  {1:N0} MB" -f $zip, ((Get-Item $zip).Length / 1MB))

# Linux through WSL: the source is reached as /mnt/<drive>/...
if (-not $NoLinux) {
    if (-not (Get-Command wsl.exe -ErrorAction SilentlyContinue)) {
        Write-Host "no WSL: no Linux archive"
    } else {
        $drive = $root.Substring(0, 1).ToLower()
        $wslRoot = "/mnt/$drive" + ($root.Substring(2) -replace '\\', '/')
        & wsl.exe -- bash "$wslRoot/Deploy/build_linux.sh"
        if ($LASTEXITCODE -ne 0) { throw "Linux build failed" }
        $tgz = Join-Path $dist "DarkConcordance-$version-linux-x86_64.tar.gz"
        Write-Host ("{0}  {1:N0} MB" -f $tgz, ((Get-Item $tgz).Length / 1MB))
    }
}
