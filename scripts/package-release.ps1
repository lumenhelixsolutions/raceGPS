#Requires -Version 5.1
# package-release.ps1
# Package raceGPS for distribution (Windows standalone + tools)

$ErrorActionPreference = "Stop"
$Version = (git describe --tags --always 2>$null) || "dev"
$OutputDir = "dist"
$ProjectRoot = Split-Path -Parent $PSScriptRoot

Write-Host "raceGPS Release Packager v$Version" -ForegroundColor Cyan
Write-Host "Project: $ProjectRoot" -ForegroundColor Gray

# Create output directory
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

# Package 1: Python Tools
$ToolsZip = "$OutputDir/racegps-tools-$Version.zip"
Write-Host "`n[1/3] Packaging Python tools..." -ForegroundColor Green
Compress-Archive -Path "$ProjectRoot/tools/*" -DestinationPath $ToolsZip -Force
Write-Host "      -> $ToolsZip" -ForegroundColor Gray

# Package 2: UE5 Game (if built)
$GameDir = "$ProjectRoot/apps/unreal-akron-beta/Build/Windows"
if (Test-Path $GameDir) {
    $GameZip = "$OutputDir/racegps-game-$Version.zip"
    Write-Host "`n[2/3] Packaging UE5 game..." -ForegroundColor Green
    Compress-Archive -Path "$GameDir/*" -DestinationPath $GameZip -Force
    Write-Host "      -> $GameZip" -ForegroundColor Gray
} else {
    Write-Host "`n[2/3] UE5 game build not found. Skipping." -ForegroundColor Yellow
    Write-Host "      Run apps/unreal-akron-beta/Build.bat first." -ForegroundColor Gray
}

# Package 3: Full source bundle
$SourceZip = "$OutputDir/racegps-source-$Version.zip"
Write-Host "`n[3/3] Packaging source bundle..." -ForegroundColor Green
$Exclude = @(".git", "node_modules", "__pycache__", ".pytest_cache", "Build", "Binaries", "Intermediate", "Saved")
$Items = Get-ChildItem -Path $ProjectRoot -Exclude $Exclude | Select-Object -ExpandProperty FullName
Compress-Archive -Path $Items -DestinationPath $SourceZip -Force
Write-Host "      -> $SourceZip" -ForegroundColor Gray

Write-Host "`nDone. Release artifacts in $OutputDir" -ForegroundColor Cyan
Get-ChildItem -Path $OutputDir | ForEach-Object { Write-Host "  $($_.Name) [$([math]::Round($_.Length/1MB,1)) MB]" }
