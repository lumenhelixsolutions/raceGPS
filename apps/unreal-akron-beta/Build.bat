@echo off
REM raceGPS Akron Beta — Windows Build Script
REM Requires: Unreal Engine 5.5+, Visual Studio 2022, Windows SDK

setlocal enabledelayedexpansion

set PROJECT_NAME=raceGPSAkronBeta
set PROJECT_DIR=%~dp0
set UPROJECT=%PROJECT_DIR%%PROJECT_NAME%.uproject
set BUILD_CONFIG=Development
set TARGET_PLATFORM=Win64
set OUTPUT_DIR=%PROJECT_DIR%Build\Windows

echo ==========================================
echo raceGPS Akron Beta Build
echo ==========================================

REM Pre-build: Level spec staleness check
set LEVEL_SPEC=%PROJECT_DIR%..\..\generated\AkronWorld_LevelSpec.json
set UMAP=%PROJECT_DIR%Content\Maps\AkronWorld.umap
set PLACEHOLDER=%PROJECT_DIR%Content\Maps\AkronWorld.umap.placeholder

if not exist "%UMAP%" (
    if exist "%PLACEHOLDER%" (
        echo WARN: AkronWorld.umap is a placeholder. Level has not been created in the Editor yet.
    ) else (
        echo WARN: AkronWorld.umap not found.
    )
    echo        See Level Setup Guide: apps/unreal-akron-beta/README.md#level-setup-guide
    echo.
) else if exist "%LEVEL_SPEC%" (
    for /f "usebackq delims=" %%a in (`powershell -NoProfile -Command "(Get-Item '%UMAP%').LastWriteTime -gt (Get-Item '%LEVEL_SPEC%').LastWriteTime"`) do (
        if "%%a"=="False" (
            echo WARN: AkronWorld.umap is older than the generated level spec.
            echo        Re-import the level spec in UE5 Editor before packaging.
            echo        See Level Setup Guide: apps/unreal-akron-beta/README.md#level-setup-guide
            echo.
        )
    )
)

REM Find UE5 engine
if exist "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" (
    set UE5_BUILD="C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat"
) else if exist "C:\Program Files\Epic Games\UE_5.5\Engine\Build\BatchFiles\Build.bat" (
    set UE5_BUILD="C:\Program Files\Epic Games\UE_5.5\Engine\Build\BatchFiles\Build.bat"
) else if exist "C:\Program Files\Epic Games\UE_5.4\Engine\Build\BatchFiles\Build.bat" (
    set UE5_BUILD="C:\Program Files\Epic Games\UE_5.4\Engine\Build\BatchFiles\Build.bat"
) else (
    echo ERROR: Unreal Engine 5 not found.
    echo Please install UE5.5+ via Epic Games Launcher or set UE5_BUILD manually.
    exit /b 1
)

REM Step 1: Generate project files
echo [1/4] Generating Visual Studio project files...
if not exist "%PROJECT_DIR%%PROJECT_NAME%.sln" (
    call "%UE5_BUILD%\..\..\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" -projectfiles -project="%UPROJECT%" -game -engine -progress
    if errorlevel 1 (
        echo ERROR: Project file generation failed.
        exit /b 1
    )
)

REM Step 2: Build editor (optional, for cooking)
echo [2/4] Building Editor binaries...
call %UE5_BUILD% %PROJECT_NAME%Editor %TARGET_PLATFORM% %BUILD_CONFIG% "%UPROJECT%" -waitmutex
if errorlevel 1 (
    echo ERROR: Editor build failed.
    exit /b 1
)

REM Step 3: Cook content
echo [3/4] Cooking content for Windows...
set UE5_RUNUAT="C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
if not exist %UE5_RUNUAT% (
    set UE5_RUNUAT="C:\Program Files\Epic Games\UE_5.5\Engine\Build\BatchFiles\RunUAT.bat"
)
if not exist %UE5_RUNUAT% (
    set UE5_RUNUAT="C:\Program Files\Epic Games\UE_5.4\Engine\Build\BatchFiles\RunUAT.bat"
)

call %UE5_RUNUAT% BuildCookRun -project="%UPROJECT%" -noP4 -platform=%TARGET_PLATFORM% -clientconfig=%BUILD_CONFIG% -serverconfig=%BUILD_CONFIG% -cook -allmaps -stage -pak -archive -archivedirectory="%OUTPUT_DIR%"
if errorlevel 1 (
    echo ERROR: Cook/Stage failed.
    exit /b 1
)

REM Step 4: Copy citypack data
echo [4/4] Copying citypack data...
if not exist "%OUTPUT_DIR%\Windows\%PROJECT_NAME%\citypacks" mkdir "%OUTPUT_DIR%\Windows\%PROJECT_NAME%\citypacks"
xcopy /E /I /Y "%PROJECT_DIR%..\..\citypacks\*" "%OUTPUT_DIR%\Windows\%PROJECT_NAME%\citypacks\"

echo ==========================================
echo Build complete: %OUTPUT_DIR%
echo ==========================================

endlocal
