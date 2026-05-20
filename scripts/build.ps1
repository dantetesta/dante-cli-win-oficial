# Dante CLI — Windows build script
# Usage:
#   .\scripts\build.ps1                          # Release build with auto-detected Qt
#   .\scripts\build.ps1 -QtRoot "C:\Qt\6.8.0\msvc2022_64"
#   .\scripts\build.ps1 -Config Debug
#   .\scripts\build.ps1 -Package                 # also runs windeployqt + zips bin/

[CmdletBinding()]
param(
    [string]$QtRoot = "",
    [ValidateSet("Release","Debug","RelWithDebInfo")]
    [string]$Config = "Release",
    [string]$Generator = "Ninja",
    [string]$Arch = "x64",
    [switch]$Clean,
    [switch]$Package
)

$ErrorActionPreference = "Stop"
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$buildDir = Join-Path $repoRoot "build/$Config"
$installDir = Join-Path $repoRoot "dist/$Config"

Write-Host "Dante CLI — build start" -ForegroundColor Cyan
Write-Host "  Config:   $Config"
Write-Host "  Generator: $Generator"
Write-Host "  RepoRoot: $repoRoot"

# Auto-detect Qt if not provided
if (-not $QtRoot) {
    $candidates = @(
        "C:\Qt\6.8.1\msvc2022_64",
        "C:\Qt\6.8.0\msvc2022_64",
        "C:\Qt\6.7.3\msvc2022_64",
        "C:\Qt\6.6.3\msvc2022_64",
        "C:\Qt\6.6.0\msvc2022_64",
        "C:\Qt\6.5.3\msvc2022_64"
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { $QtRoot = $c; break }
    }
}
if (-not $QtRoot) {
    Write-Error "Qt 6.5+ not found. Install via Qt Online Installer or pass -QtRoot."
    exit 1
}
Write-Host "  Qt:       $QtRoot"

$qtBin = Join-Path $QtRoot "bin"
$env:PATH = "$qtBin;$env:PATH"
$env:CMAKE_PREFIX_PATH = $QtRoot

if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "Cleaning $buildDir" -ForegroundColor Yellow
    Remove-Item -Recurse -Force $buildDir
}

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

# Configure
Write-Host "`n[1/3] CMake configure" -ForegroundColor Green
cmake -S $repoRoot -B $buildDir `
    -G $Generator `
    -DCMAKE_BUILD_TYPE=$Config `
    -DCMAKE_INSTALL_PREFIX="$installDir" `
    -DCMAKE_PREFIX_PATH="$QtRoot"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# Build
Write-Host "`n[2/3] cmake --build" -ForegroundColor Green
cmake --build $buildDir --config $Config --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# windeployqt
Write-Host "`n[3/3] windeployqt" -ForegroundColor Green
$exeName = "Dante CLI.exe"
$exePath = Join-Path $buildDir "bin\$exeName"
if (-not (Test-Path $exePath)) {
    Write-Error "Built executable not found at $exePath"
    exit 2
}

$windeployqt = Join-Path $qtBin "windeployqt.exe"
if (Test-Path $windeployqt) {
    & $windeployqt --release `
        --no-translations `
        --no-system-d3d-compiler `
        --no-opengl-sw `
        --compiler-runtime `
        "$exePath"
} else {
    Write-Warning "windeployqt not found at $windeployqt"
}

if ($Package) {
    Write-Host "`nPackaging dist/$Config..." -ForegroundColor Cyan
    New-Item -ItemType Directory -Force -Path $installDir | Out-Null
    Copy-Item -Recurse -Force (Join-Path $buildDir "bin\*") $installDir
    $zipPath = Join-Path $repoRoot "dist\DanteCLI-$Config-x64.zip"
    if (Test-Path $zipPath) { Remove-Item $zipPath }
    Compress-Archive -Path "$installDir\*" -DestinationPath $zipPath
    Write-Host "  → $zipPath" -ForegroundColor Cyan
}

Write-Host "`nBuild OK — $exePath" -ForegroundColor Green
