# raceGPS Windows Installer Builder
# Run this AFTER you have a successful payload from Build.bat (i.e. apps/unreal-akron-beta/Build/Windows contains the game files + raceGPS.exe)
# Requires NSIS 3.x installed (makensis in PATH or standard location)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path | Split-Path -Parent
$InstallerDir = Join-Path $Root "installer"
$NSISScript = Join-Path $InstallerDir "racegps-setup.nsi"

Write-Host "raceGPS Windows Installer Builder" -ForegroundColor Cyan
Write-Host "=================================="

# Verify payload
$PayloadDir = Join-Path $Root "apps\unreal-akron-beta\Build\Windows"
$CitypacksDir = Join-Path $Root "citypacks"

if (-not (Test-Path $PayloadDir)) {
    Write-Error "Payload not found at $PayloadDir. Run the UE5 build/packaging first (Build.bat or scripts\build.py)."
    exit 1
}

$Exe = Get-ChildItem $PayloadDir -Recurse -Filter "raceGPS*.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $Exe) {
    Write-Warning "No raceGPS*.exe found under payload yet (searched recursively). Make sure the full archive/cook stage completed in Build.bat."
}

# Deep payload inspection for layout (shortcuts in NSIS expect raceGPS.exe directly usable at $INSTDIR)
Write-Host "`n--- Payload layout inspection ---" -ForegroundColor Cyan
$topItems = Get-ChildItem $PayloadDir -ErrorAction SilentlyContinue | Select-Object -First 15
if ($topItems) {
    Write-Host "Top items directly under $PayloadDir :"
    $topItems | ForEach-Object { Write-Host "  $($_.Name) $(if($_.PSIsContainer){'(dir)'})" }
} else {
    Write-Host "(empty or inaccessible)"
}

$allExes = Get-ChildItem $PayloadDir -Recurse -Filter "*.exe" -ErrorAction SilentlyContinue
if ($allExes) {
    Write-Host "`nAll .exe files found (relative):"
    $allExes | ForEach-Object {
        $rel = $_.FullName.Substring($PayloadDir.Length).TrimStart('\','/')
        $mb = [math]::Round($_.Length/1MB,1)
        Write-Host "  $rel  (size: $mb MB)"
    }
} else {
    Write-Host "No .exe files found in payload at all."
}

$rootRaceExe = Join-Path $PayloadDir "raceGPS.exe"
$dummySize = 500KB   # anything this small is our placeholder (cmd.exe copy)
$isDummy = $false

if (Test-Path $rootRaceExe) {
    $rootSize = (Get-Item $rootRaceExe).Length
    if ($rootSize -lt $dummySize) {
        $isDummy = $true
        Write-Host "`n*** PLACEHOLDER / DEMO PAYLOAD DETECTED ***" -ForegroundColor Red -BackgroundColor Yellow
        Write-Host "The raceGPS.exe at the payload root is only $([math]::Round($rootSize/1KB)) KB." -ForegroundColor Red
        Write-Host "This is the temporary stand-in we created (copy of cmd.exe) so the installer UI could be built and tested immediately." -ForegroundColor Red
        Write-Host "The installed 'game' will just be a command prompt. No real raceGPS experience." -ForegroundColor Red
        Write-Host "To see the actual game, first complete a full UE5 package (see below), then re-run this script." -ForegroundColor Yellow
    } else {
        Write-Host "`n✓ raceGPS.exe present directly in payload root (good for NSIS shortcuts)." -ForegroundColor Green
    }
} else {
    Write-Warning "raceGPS.exe NOT present at payload root ($rootRaceExe). NSIS shortcuts + LaunchGame assume it will be at `$INSTDIR\raceGPS.exe after the recursive File copy."
    # Auto-normalize if we can find a suitable built exe
    $candidate = Get-ChildItem $PayloadDir -Recurse -Filter "*.exe" -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match "race|akron|game" -and $_.Length -gt 5MB } |
        Sort-Object Length -Descending |
        Select-Object -First 1
    if ($candidate) {
        try {
            Copy-Item $candidate.FullName $rootRaceExe -Force -ErrorAction Stop
            Write-Host "  Auto-normalized: copied $($candidate.Name) -> raceGPS.exe at payload root." -ForegroundColor Green
            $Exe = Get-Item $rootRaceExe  # update for later checks
        } catch {
            Write-Host "  Auto-copy failed. Manual command:" -ForegroundColor Yellow
            Write-Host "  Copy-Item `"$($candidate.FullName)`" `"$rootRaceExe`" -Force"
        }
    } else {
        Write-Host "  No suitable large candidate exe found to auto-normalize. You may need to rename/copy your main game exe to raceGPS.exe in the payload root before the installer File copy."
    }
}

if ($isDummy) {
    Write-Host ""
    Write-Host "=== HOW TO GET THE REAL GAME ===" -ForegroundColor Cyan
    Write-Host "1. Make sure you have the full UE5 cooked/staged payload:"
    Write-Host "   cd apps\unreal-akron-beta"
    Write-Host "   .\Build.bat"
    Write-Host "   (or manual: open .uproject in UE Editor, create AkronWorld level as per BUILD.md / README.md, then Package Project or run BuildCookRun)"
    Write-Host "2. Confirm real content: the .exe in Build\Windows should be tens or hundreds of MB, plus Content, .pak files, etc."
    Write-Host "3. Re-run this builder:  pwsh -File scripts\build-windows-installer.ps1"
    Write-Host "4. Run the new installer. It will now bundle the actual game."
    Write-Host ""
}

if (-not (Test-Path $CitypacksDir)) {
    Write-Error "citypacks directory missing at $CitypacksDir"
    exit 1
}

Write-Host "`nPayload looks present." -ForegroundColor Green
Write-Host "Citypacks present." -ForegroundColor Green

# Find makensis
$makensis = $null
$possible = @(
    "C:\Program Files (x86)\NSIS\makensis.exe",
    "C:\Program Files\NSIS\makensis.exe",
    (Get-Command makensis.exe -ErrorAction SilentlyContinue).Source
)

foreach ($p in $possible) {
    if ($p -and (Test-Path $p)) {
        $makensis = $p
        break
    }
}

if (-not $makensis) {
    Write-Error "makensis.exe not found. Install NSIS 3.x from https://nsis.sourceforge.io/Download and ensure it's in PATH or standard location."
    exit 1
}

Write-Host "Using makensis: $makensis" -ForegroundColor Green

# Compile
Write-Host "Compiling installer..." -ForegroundColor Yellow
Push-Location $InstallerDir
& $makensis /V2 $NSISScript
$compileExit = $LASTEXITCODE
Pop-Location

if ($compileExit -eq 0) {
    $outExe = Join-Path $InstallerDir "raceGPS-v0.2.0-Win64-Setup.exe"
    if (Test-Path $outExe) {
        Write-Host "SUCCESS! Installer created: $outExe" -ForegroundColor Green
        Write-Host "Next: Test on a clean VM (or current machine) following installer\WINDOWS-INSTALLER-VALIDATION-CHECKLIST.md" -ForegroundColor Green
    } else {
        # Fallback: sometimes lands next to caller cwd
        $fallback = Join-Path $Root "raceGPS-v0.2.0-Win64-Setup.exe"
        if (Test-Path $fallback) {
            Move-Item $fallback $outExe -Force
            Write-Host "SUCCESS! Installer created: $outExe (was moved from root)" -ForegroundColor Green
        } else {
            Write-Host "Compile appeared successful (exit 0) but output exe not found in expected locations. Check the makensis output above for the written filename." -ForegroundColor Yellow
        }
    }
} else {
    Write-Error "NSIS compile failed. Check output above for errors (common: missing includes, payload paths, or plugin issues)."
}