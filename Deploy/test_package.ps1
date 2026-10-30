<#
.SYNOPSIS
    Starts a Windows package once: unpacked into work\package_test, muted, Velm for a few seconds.

.DESCRIPTION
    .\Deploy\test_package.ps1 dist\DarkConcordance-<version>-windows-x64.zip

    It must end by itself, draw its picture (not black) and miss no file of its own (raylib's "Failed to open" in its
    log; the people's models ask for a few textures they never had, those are left out). Exit 0 when all is well.
#>
param([Parameter(Mandatory = $true)][string]$Zip)
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$dir = Join-Path $root "work\package_test"
if (Test-Path $dir) { Remove-Item -Recurse -Force $dir }
New-Item -ItemType Directory -Force $dir | Out-Null
& (Join-Path $env:SystemRoot "System32\tar.exe") -x -f $Zip -C $dir
if ($LASTEXITCODE -ne 0) { Write-Host "unpacking failed"; exit 1 }
$game = Get-ChildItem $dir -Recurse -Filter DarkConcordance.exe | Select-Object -First 1
$shot = Join-Path $dir "test.png"
$p = Start-Process -FilePath $game.FullName -WorkingDirectory $game.DirectoryName -PassThru `
    -ArgumentList "--level", "velm.txt", "--hero", "corvin", "--autoplay", "--shot", $shot, "--warm", "8", "--mute"
$p.PriorityClass = "BelowNormal"
if (-not $p.WaitForExit(300000)) { $p.Kill(); Write-Host "it did not end"; exit 1 }
if ($p.ExitCode -ne 0) { Write-Host ("it ended with {0:X}" -f $p.ExitCode); exit 1 }
if (-not (Test-Path $shot) -or (Get-Item $shot).Length -lt 300KB) { Write-Host "no picture (or a black one)"; exit 1 }
$log = Join-Path $game.DirectoryName "spaceopera.log"
$missing = @(Select-String -Path $log -Pattern "Failed to open" | Where-Object { $_.Line -notmatch "quaternius" })
if ($missing.Count -gt 0) { $missing | Select-Object -First 5 | ForEach-Object { Write-Host $_.Line }; exit 1 }
Write-Host "the Windows package starts: $shot"
exit 0
