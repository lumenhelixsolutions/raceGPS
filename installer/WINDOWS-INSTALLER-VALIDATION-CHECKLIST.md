# raceGPS Windows Installer Validation Checklist

Target installer
- `installer\raceGPS-v0.2.0-Win64-Setup.exe` (relative to racegps repo root)

Required build payload before compiling NSIS
- `apps/unreal-akron-beta/Build/Windows\` (the full cooked/staged output from Build.bat, containing the .exe)
- `citypacks\` (akron-oh-beta-001 and templates)

## A. Before building the installer

1. Confirm packaged game payload exists (user: run Build.bat or equivalent until complete)
   - Verify `apps/unreal-akron-beta/Build/Windows\` exists
   - Verify it contains the packaged game output + `raceGPS*.exe` (search recursively)
2. Confirm citypacks exist
   - Verify `citypacks\akron-oh-beta-001\` (has .xodr + json manifests)
3. Confirm NSIS prerequisites on build machine
   - NSIS 3.x installed (https://nsis.sourceforge.io/Download)
   - makensis in PATH or at standard `C:\Program Files (x86)\NSIS\makensis.exe`
4. Compile (recommended):
   - From repo root: `pwsh -File scripts\build-windows-installer.ps1`
   - Or manually: `cd installer; makensis /V2 racegps-setup.nsi`
5. Expected output name
   - `installer\raceGPS-v0.2.0-Win64-Setup.exe`

## B. Install test on a clean Windows machine or VM

1. Copy installer to the target machine
2. Run as Administrator
3. On preflight page verify:
   - OS check renders
   - RAM check renders
   - Disk check renders
   - DirectX line renders
   - VC++ runtime line renders
4. Continue install
5. Verify installed files under:
   - `C:\Program Files\raceGPS\`
6. Confirm these exist after install:
   - `C:\Program Files\raceGPS\raceGPS.exe`
   - `C:\Program Files\raceGPS\citypacks\`
   - `C:\Program Files\raceGPS\uninst.exe`
7. Verify shortcuts
   - Desktop shortcut launches
   - Start menu shortcut launches
   - Uninstall shortcut exists

## C. First-launch validation

1. Launch `raceGPS.exe`
2. Verify app starts without missing-runtime error
3. Verify onboarding appears
4. Verify onboarding can reach city selection
5. Verify bundled Akron citypack is visible/selectable
6. Verify save/config path is created
   - `%LOCALAPPDATA%\raceGPS\Saved\Config\PlayerSettings.json`
7. Verify app reaches main menu

## D. Failure points to watch

1. Installer compiles but includes no game payload
   - Cause: `apps/unreal-akron-beta/Build/Windows\` missing/empty or wrong layout from UE cook+archive. Re-run Build.bat fully. The build script warns if no raceGPS*.exe found recursively.
2. Shortcut created but launch fails
   - Cause: `raceGPS.exe` (or raceGPSAkronBeta.exe) not present directly under $INSTDIR after the File /r copy. Check actual layout in your payload; may need a small launcher or adjust NSIS SetOutPath + File for the binaries subdir.
3. Installer completes but citypack missing
   - Cause: repo `citypacks\` not copied (NSIS has explicit copy from root citypacks)
4. First launch fails on VC++ runtime
   - Cause: redistributable install/download failed (NSISdl section). Manual install of vc_redist.x64.exe as fallback.
5. Preflight page compile error in NSIS
   - Cause: missing `nsDialogs.nsh` (included with NSIS 3). Or plugin issues (nsProcess mentioned in comments but not strictly required by current script).

## E. Pass / fail summary

Pass if all are true:
- build script or makensis succeeds and produces the .exe
- installed `C:\Program Files\raceGPS\` contains raceGPS.exe (or equivalent) + citypacks\
- desktop + start menu shortcuts created and point correctly
- bundled akron citypack visible
- first launch runs preflight/hardware check + onboarding (no missing runtime popup)
- reaches main menu and can select/start a race with the included citypack

Fail if any are false.
