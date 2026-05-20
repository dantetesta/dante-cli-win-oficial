# Dante CLI — installer build script
# Builds the app, runs windeployqt, then packages with Inno Setup (preferred)
# or NSIS as fallback.
#
# Usage:
#   .\scripts\make-installer.ps1                 # Inno Setup if available, else NSIS
#   .\scripts\make-installer.ps1 -Engine NSIS

[CmdletBinding()]
param(
    [ValidateSet("Auto","Inno","NSIS")]
    [string]$Engine = "Auto",
    [string]$QtRoot = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$distDir  = Join-Path $repoRoot "dist\Release"
$outDir   = Join-Path $repoRoot "dist\installer"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

Write-Host "1/3 — building app in Release mode" -ForegroundColor Cyan
& "$PSScriptRoot\build.ps1" -Config Release -QtRoot $QtRoot -Package
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# Decide engine
if ($Engine -eq "Auto") {
    $inno = Get-Command "ISCC.exe" -ErrorAction SilentlyContinue
    if (-not $inno) {
        $innoPaths = @(
            "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
            "C:\Program Files\Inno Setup 6\ISCC.exe"
        )
        foreach ($p in $innoPaths) {
            if (Test-Path $p) { $inno = $p; break }
        }
    } else {
        $inno = $inno.Source
    }
    if ($inno) {
        $Engine = "Inno"
    } else {
        $Engine = "NSIS"
    }
}

Write-Host "2/3 — packaging with $Engine" -ForegroundColor Cyan

if ($Engine -eq "Inno") {
    $iscc = if ($inno) { $inno } else { (Get-Command ISCC.exe).Source }
    & $iscc /Qp /O"$outDir" (Join-Path $repoRoot "installer\inno\dante-cli.iss")
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} else {
    $nsis = Get-Command "makensis.exe" -ErrorAction SilentlyContinue
    if (-not $nsis) {
        $nsisPaths = @(
            "C:\Program Files (x86)\NSIS\makensis.exe",
            "C:\Program Files\NSIS\makensis.exe"
        )
        foreach ($p in $nsisPaths) {
            if (Test-Path $p) { $nsis = $p; break }
        }
    } else {
        $nsis = $nsis.Source
    }
    if (-not $nsis) {
        Write-Error "Neither Inno Setup nor NSIS found. Install one of them."
        exit 3
    }
    & $nsis /DDIST_DIR="$distDir" /DOUT_DIR="$outDir" `
        (Join-Path $repoRoot "installer\nsis\dante-cli.nsi")
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Host "`n3/3 — done" -ForegroundColor Green
Get-ChildItem $outDir | ForEach-Object {
    Write-Host "  → $($_.FullName)" -ForegroundColor Cyan
}
