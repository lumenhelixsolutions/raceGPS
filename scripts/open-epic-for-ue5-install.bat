@echo off
chcp 65001 >nul
echo ========================================
echo   raceGPS UE 5.5 Installer Helper
echo ========================================
echo.
echo Epic Games Launcher will open now.
echo.
echo STEP 1: Click the UNREAL ENGINE tab at the top
echo STEP 2: Click LIBRARY on the left sidebar
echo STEP 3: Click the + button next to ENGINE VERSIONS
echo STEP 4: Select 5.5.x (latest)
echo STEP 5: Click INSTALL
echo STEP 6: Choose install location: D:\Epic Games\UE_5.5
echo STEP 7: Wait for download (~15-20 GB)
echo.
echo Press any key to open Epic Launcher...
pause >nul
start "" "D:\Epic Games\Launcher\Portal\Binaries\Win64\EpicGamesLauncher.exe"
echo.
echo Epic Launcher opened. Follow the steps above.
echo When UE 5.5 is installed, run:
echo   scripts\setup-ue5-dev-env.bat
echo.
pause
