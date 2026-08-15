# raceGPS EZ Windows Setup
# Easy one-script onboarding + preflight + setup + tests for local clone
# Run in PowerShell (pwsh or Windows PowerShell 5.1+)
# Recommended: Run as Administrator for best results (VS/UE installs, paths)
#
# What it does:
# - Beautiful onboarding + explanations
# - Preflight checks (OS, RAM, disk, tools)
# - Auto setup for Python tools, Node services, data validation
# - Guidance + automation for UE5 build
# - Built-in test runs (pytest, citypack validate, node tests)
# - Final verification + launch instructions
#
# Usage:
#   Right-click this file -> Run with PowerShell
#   or: pwsh -ExecutionPolicy Bypass -File scripts\racegps-ez-setup.ps1

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"
$Host.UI.RawUI.WindowTitle = "raceGPS EZ Setup"

# Colors
function Write-Ok($msg)    { Write-Host "✅ $msg" -ForegroundColor Green }
function Write-Warn($msg)  { Write-Host "⚠️  $msg" -ForegroundColor Yellow }
function Write-Err($msg)   { Write-Host "❌ $msg" -ForegroundColor Red }
function Write-Info($msg)  { Write-Host "ℹ️  $msg" -ForegroundColor Cyan }
function Write-Step($num, $msg) {
    Write-Host ""
    Write-Host "══════════════════════════════════════════════════════════════" -ForegroundColor DarkGray
    Write-Host " STEP $num: $msg" -ForegroundColor Magenta
    Write-Host "══════════════════════════════════════════════════════════════" -ForegroundColor DarkGray
}

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $Root) { $Root = "C:\projects\racegps" }  # fallback if run differently
$Root = Resolve-Path $Root | Select-Object -ExpandProperty Path
Set-Location $Root

Write-Host @"
  ____   __   ____  ____  ____  ____  ____ 
 |  _ \ /  \ / ___||  _ \|  __||  __||  _ \
 | |_) | () | |  _ | |_) | |__ | |__ | |_) |
 |  _ <  __/| |_| ||  _ <|  __||  __||  _ <
 |_| \_\|    \____||_| \_\|____||____||_| \_\
 
 raceGPS EZ Windows Setup (Local Clone)
 Real-world arcade racing on actual city streets
"@ -ForegroundColor Cyan

# ============================================================================
# ONBOARDING
# ============================================================================
Write-Host ""
Write-Host "Welcome to the easiest way to set up your local raceGPS clone!" -ForegroundColor White
Write-Host ""
Write-Host "This script will:"
Write-Host "  • Check your system (preflight)"
Write-Host "  • Install/setup Python tools & Node services"
Write-Host "  • Guide you through the Unreal Engine 5 build"
Write-Host "  • Run built-in tests automatically"
Write-Host "  • Give you simple next steps to play"
Write-Host ""
Write-Host "What you still need to do manually:"
Write-Host "  • Install Unreal Engine 5 + Visual Studio (if missing) - BIG downloads"
Write-Host "  • Open the UE Editor once and create/import the level (one-time)"
Write-Host ""
Write-Host "Estimated time: 30-90 minutes (mostly downloads + UE build)"
Write-Host "Disk space needed: ~15-25 GB free (UE is huge)"
Write-Host ""

$continue = Read-Host "Ready to begin? (y/n)"
if ($continue -ne "y" -and $continue -ne "Y") {
    Write-Host "Exiting. Run the script again anytime." -ForegroundColor Yellow
    exit 0
}

# ============================================================================
# PREFLIGHT CHECKS
# ============================================================================
Write-Step "1" "PREFLIGHT - Checking your system"

$checksPassed = $true

# OS
$os = Get-CimInstance Win32_OperatingSystem
$osVersion = [version]$os.Version
if ($osVersion.Major -ge 10) {
    Write-Ok "Windows 10/11 detected ($($os.Caption))"
} else {
    Write-Err "Windows 10 or newer required. You have: $($os.Caption)"
    $checksPassed = $false
}

# RAM (recommend 16GB+)
$ramGB = [math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory / 1GB)
if ($ramGB -ge 16) {
    Write-Ok "RAM: $ramGB GB (recommended)"
} elseif ($ramGB -ge 8) {
    Write-Warn "RAM: $ramGB GB (minimum is 8GB, 16GB+ recommended for UE builds)"
} else {
    Write-Err "RAM: $ramGB GB - too low (need at least 8GB)"
    $checksPassed = $false
}

# Disk space on C: (need ~20GB free)
$disk = Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='C:'"
$freeGB = [math]::Round($disk.FreeSpace / 1GB)
if ($freeGB -ge 20) {
    Write-Ok "C: drive free space: $freeGB GB"
} else {
    Write-Warn "C: drive free space: $freeGB GB (20GB+ recommended for UE + builds)"
}

# Git
if (Get-Command git -ErrorAction SilentlyContinue) {
    $gitVer = git --version
    Write-Ok "Git: $gitVer"
} else {
    Write-Err "Git not found. Install from https://git-scm.com/"
    $checksPassed = $false
}

# Python
$pythonOk = $false
try {
    $pyVer = py --version 2>&1
    if ($pyVer -match "Python 3\.(1[0-9]|[1-9][0-9])") {
        Write-Ok "Python: $pyVer (via py launcher)"
        $pythonOk = $true
    } else {
        Write-Warn "Python found but version may be old: $pyVer (need 3.10+)"
    }
} catch {
    Write-Err "Python 'py' launcher not found. Install Python 3.10+ from python.org (check 'Add python to PATH' and 'py launcher')"
    $checksPassed = $false
}

# Node
if (Get-Command node -ErrorAction SilentlyContinue) {
    $nodeVer = node --version
    $major = [int]($nodeVer -replace 'v','' -split '\.')[0]
    if ($major -ge 20) {
        Write-Ok "Node.js: $nodeVer (>=20 good)"
    } else {
        Write-Warn "Node.js: $nodeVer (20+ recommended)"
    }
} else {
    Write-Err "Node.js not found. Install from https://nodejs.org/ (LTS 20+)"
    $checksPassed = $false
}

# Visual Studio 2022 (check for MSBuild or common paths)
$vsFound = $false
$vsPaths = @(
    "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe",
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
)
foreach ($p in $vsPaths) {
    if (Test-Path $p) {
        Write-Ok "Visual Studio 2022 found (MSBuild at $p)"
        $vsFound = $true
        break
    }
}
if (-not $vsFound) {
    Write-Warn "Visual Studio 2022 with 'Game development with C++' workload not detected."
    Write-Warn "  You will need it for the UE build. Download from visualstudio.microsoft.com"
}

# Unreal Engine 5.7 (check common install locations)
$ueFound = $false
$uePaths = @(
    "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat",
    "C:\Program Files (x86)\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat"
)
foreach ($p in $uePaths) {
    if (Test-Path $p) {
        Write-Ok "Unreal Engine 5.7 found at $p"
        $ueFound = $true
        break
    }
}
if (-not $ueFound) {
    Write-Warn "Unreal Engine 5.7 not found in standard locations."
    Write-Warn "  Install via Epic Games Launcher (https://www.unrealengine.com/). This is the biggest prerequisite."
}

# DirectX (basic check via dxdiag or assume on Win10+)
Write-Info "DirectX 12 is required (usually present on Windows 10/11). Will be verified at first launch."

if (-not $checksPassed) {
    Write-Err "Some critical preflight checks failed. Please fix the issues above and re-run this script."
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Ok "Preflight passed (or warnings noted). Continuing..."

# ============================================================================
# ONBOARDING + SETUP
# ============================================================================
Write-Step "2" "ONBOARDING - Setting up your environment"

Write-Host "This will:"
Write-Host "  • Set up Python virtual environments for the city compilers and servers"
Write-Host "  • Install Node.js dependencies for cityhub and backend"
Write-Host "  • Validate the bundled Akron city data"
Write-Host "  • Run automatic tests (Python + Node)"
Write-Host "  • Guide you through the UE5 build (the big manual part)"
Write-Host ""
Read-Host "Press Enter to start the automated setup (or Ctrl+C to cancel)"

# Python venvs for tools
Write-Info "Setting up Python environments (this may take a few minutes)..."

$pythonTools = @(
    @{ Name = "akron-semantic-compiler"; Dir = "tools\akron-semantic-compiler" },
    @{ Name = "universal-city-compiler"; Dir = "tools\universal-city-compiler" },
    @{ Name = "semantic-map-server";   Dir = "apps\semantic-map-server" }
)

foreach ($tool in $pythonTools) {
    $fullDir = Join-Path $Root $tool.Dir
    if (Test-Path $fullDir) {
        Push-Location $fullDir
        if (-not (Test-Path ".venv")) {
            Write-Info "Creating venv for $($tool.Name)..."
            py -m venv .venv
        }
        & ".\.venv\Scripts\Activate.ps1"
        if (Test-Path "requirements.txt") {
            Write-Info "Installing requirements for $($tool.Name)..."
            pip install -r requirements.txt --quiet
        }
        deactivate
        Pop-Location
        Write-Ok "$($tool.Name) environment ready"
    }
}

# Node setup
Write-Info "Setting up Node.js workspaces..."

# Root monorepo
if (Test-Path "package.json") {
    Write-Info "Installing root monorepo dependencies..."
    npm install --silent 2>$null | Out-Null
    Write-Ok "Root dependencies installed"
}

# cityhub
if (Test-Path "cityhub\package.json") {
    Push-Location "cityhub"
    if (-not (Test-Path "node_modules")) {
        Write-Info "Installing cityhub dependencies..."
        npm install --silent 2>$null | Out-Null
    }
    Pop-Location
    Write-Ok "cityhub ready"
}

# apps/backend
if (Test-Path "apps\backend\package.json") {
    Push-Location "apps\backend"
    if (-not (Test-Path "node_modules")) {
        Write-Info "Installing backend dependencies..."
        npm install --silent 2>$null | Out-Null
    }
    Pop-Location
    Write-Ok "backend ready"
}

# ============================================================================
# DATA + BUILT-IN TESTS
# ============================================================================
Write-Step "3" "DATA VALIDATION + BUILT-IN TEST RUNS"

# Validate citypack (built-in test)
Write-Info "Running built-in city data validation..."
try {
    $validateResult = py tools/validate-citypack.py citypacks/akron-oh-beta-001 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Ok "Akron citypack validation passed"
    } else {
        Write-Warn "Citypack validation had warnings (data may still work): $validateResult"
    }
} catch {
    Write-Warn "Could not run citypack validator (continuing anyway)"
}

# Python tests (built-in)
Write-Info "Running Python test suite (selected tests)..."
$testFiles = @(
    "tests/test_levelspec.py",
    "tests/test_import_pipeline.py",
    "tests/test_gameplay.py"
)
foreach ($tf in $testFiles) {
    if (Test-Path $tf) {
        Write-Info "  Running $tf ..."
        py -m pytest $tf -q --tb=no 2>$null
        if ($LASTEXITCODE -eq 0) {
            Write-Ok "  $tf passed"
        } else {
            Write-Warn "  $tf had issues (non-fatal for basic setup)"
        }
    }
}

# Node tests for cityhub
if (Test-Path "cityhub\package.json") {
    Write-Info "Running cityhub tests..."
    Push-Location "cityhub"
    npm test -- --silent 2>$null | Out-Null
    if ($LASTEXITCODE -eq 0) {
        Write-Ok "cityhub tests passed"
    } else {
        Write-Warn "cityhub tests had issues (check manually later)"
    }
    Pop-Location
}

Write-Ok "Built-in tests completed (some warnings are normal in beta)"

# ============================================================================
# UE5 BUILD ONBOARDING
# ============================================================================
Write-Step "4" "UE5 BUILD - The big one (guided)"

Write-Host ""
Write-Host "The Unreal Engine part is the most manual step." -ForegroundColor Yellow
Write-Host "We will now:"
Write-Host "  1. Try to run the automated Build.bat"
Write-Host "  2. Tell you exactly what to do in the UE Editor (one-time only)"
Write-Host ""

$doUE = Read-Host "Run UE build guidance now? (y/n) [recommended]"
if ($doUE -ne "y" -and $doUE -ne "Y") {
    Write-Warn "Skipping UE steps. You can run them later with: cd apps\unreal-akron-beta ; .\Build.bat"
} else {
    # Try automated build first
    Write-Info "Attempting automated UE build (this can take 10-30+ minutes)..."
    if (Test-Path "apps\unreal-akron-beta\Build.bat") {
        Push-Location "apps\unreal-akron-beta"
        try {
            & .\Build.bat
        } catch {
            Write-Warn "Build.bat encountered issues (common on first run - see below)."
        }
        Pop-Location
    }

    Write-Host ""
    Write-Host "=== MANUAL UE EDITOR STEPS (DO THESE ONCE) ===" -ForegroundColor Magenta
    Write-Host ""
    Write-Host "1. Double-click: apps\unreal-akron-beta\raceGPSAkronBeta.uproject"
    Write-Host "   (This opens Unreal Editor - it may ask to build or regenerate files)"
    Write-Host ""
    Write-Host "2. In the editor:"
    Write-Host "   - File → New Level → Empty Open World"
    Write-Host "   - Save As → Content/Maps/AkronWorld (this replaces the placeholder)"
    Write-Host "   - Window → World Settings:"
    Write-Host "       GameMode Override = CruiseSprintGameMode"
    Write-Host "       Game Instance     = raceGPSGameInstance"
    Write-Host "   - Place a PlayerStart actor near (0,0,0)"
    Write-Host "   - Save the level"
    Write-Host ""
    Write-Host "3. Open Python console: Window → Developer Tools → Python"
    Write-Host "   Paste and run this (exact path):"
    Write-Host '   exec(open(r"C:\projects\racegps\tools\ue5-import-level-spec.py").read())'
    Write-Host ""
    Write-Host "4. Build → Build Lighting Only (Production)"
    Write-Host "5. Save level again"
    Write-Host ""
    Write-Host "6. (Optional but recommended) Re-run Build.bat after the above"
    Write-Host ""
    Write-Host "After that, you can Play In Editor (PIE) and race!"
    Write-Host ""
    Read-Host "Press Enter after you have completed (or will complete) the UE Editor steps above"
}

# ============================================================================
# FINAL VERIFICATION + ONBOARDING COMPLETE
# ============================================================================
Write-Step "5" "FINAL CHECKS & YOU'RE READY"

Write-Host ""
Write-Ok "raceGPS EZ setup complete!"

Write-Host ""
Write-Host "Quick verification you can run anytime:"
Write-Host "  • cityhub:     cd cityhub ; node server.js   (then visit http://localhost:7778/api/cities)"
Write-Host "  • data server: cd apps\semantic-map-server ; python main.py"
Write-Host "  • UE game:     Double-click apps\unreal-akron-beta\raceGPSAkronBeta.uproject → Play"
Write-Host ""

Write-Host "Next steps (recommended order):"
Write-Host "  1. Finish the UE Editor level steps above if you haven't"
Write-Host "  2. Launch the editor and try a race (use ~ console for cheats like CheatFinishRace)"
Write-Host "  3. (Optional) Run full test suite: py -m pytest tests/ -q"
Write-Host "  4. Explore docs/ for architecture, controls, etc."
Write-Host ""

Write-Host "Common next commands:"
Write-Host "  cd apps\unreal-akron-beta ; .\Build.bat"
Write-Host "  cd cityhub ; node server.js"
Write-Host ""

Write-Host "Thank you for setting up raceGPS! Drive safe on real streets." -ForegroundColor Green
Write-Host ""
Read-Host "Press Enter to finish (script will close)"

# Optional: open a couple helpful things
Start-Process "explorer.exe" "$Root\apps\unreal-akron-beta"
if (Test-Path "$Root\docs\GAMEPLAY_DESIGN.md") {
    Start-Process "$Root\docs\GAMEPLAY_DESIGN.md"
}