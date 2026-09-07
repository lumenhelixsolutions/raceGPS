# Import CARLA car-paint stub textures. Use an ABSOLUTE python path.
$Ue = "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Proj = "C:\projects\racegps\apps\unreal-akron-beta\raceGPSAkronBeta.uproject"
$Py = "C:/projects/racegps/apps/unreal-akron-beta/Content/Python/import_carpaint_stub_textures.py"
if (-not (Test-Path $Ue)) { Write-Error "UnrealEditor-Cmd missing: $Ue"; exit 1 }
& $Ue $Proj -unattended -nopause -nullrhi -ExecutePythonScript="$Py"
exit $LASTEXITCODE
