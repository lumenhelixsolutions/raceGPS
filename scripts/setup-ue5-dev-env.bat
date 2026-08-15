@echo off
REM setup-ue5-dev-env.bat
REM Auto-elevating launcher for the PowerShell setup script.
REM Double-click this file — it will request Administrator permissions automatically.

echo ==========================================
echo raceGPS UE5.7 Dev Environment Setup
echo ==========================================
echo.

:: Check if already running as admin
net session >nul 2>&1
if %errorLevel% == 0 (
    echo Running as Administrator. Launching PowerShell script...
    powershell -ExecutionPolicy Bypass -File "D:\projects\scripts\setup-ue5-dev-env.ps1"
) else (
    echo Not running as Administrator. Requesting elevation...
    echo Click YES on the UAC prompt.
    echo.
    powershell -Command "Start-Process -FilePath 'cmd.exe' -ArgumentList '/c """"D:\projects\scripts\setup-ue5-dev-env.bat""""' -Verb RunAs"
)

pause
