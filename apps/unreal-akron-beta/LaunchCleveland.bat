@echo off
REM raceGPS Cleveland Historic Circuit — PIE / -game test run
REM Does not change GlobalDefaultGameMode (Akron stays CruiseSprint).
setlocal
set UE="C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe"
set PROJ="C:\projects\racegps\apps\unreal-akron-beta\raceGPSAkronBeta.uproject"
set MAP=/Game/Maps/Cleveland5_0KmWorld
set GAME=/Script/raceGPSAkronBeta.ClevelandShowcaseGameMode
if not exist %UE% (
  echo UnrealEditor not found at %UE%
  exit /b 1
)
echo Launching Cleveland showcase...
echo   map  %MAP%
echo   game %GAME%
echo Watch Output Log for [raceGPS] paint MID ... and [raceGPS Cleveland]
set EXTRA=
if /I "%~1"=="playtest" (
  set EXTRA=-ClevelandAutoLap -ClevelandSkipIntro -log -windowed -ResX=1600 -ResY=900
  echo   playtest auto-lap ON
)
start "raceGPS Cleveland" %UE% %PROJ% "%MAP%?game=%GAME%" -game %EXTRA%
