# Cleveland test run (visual sprint)

Unreal Engine **5.7** is at `C:\Program Files\Epic Games\UE_5.7`.

## Launch (does not change Akron default GameMode)

Double-click `apps/unreal-akron-beta/LaunchCleveland.bat` or:

```
"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe" ^
  "C:\projects\racegps\apps\unreal-akron-beta\raceGPSAkronBeta.uproject" ^
  "/Game/Maps/Cleveland5_0KmWorld?game=/Script/raceGPSAkronBeta.ClevelandShowcaseGameMode" ^
  -game
```

First boot may compile shaders. No CARLA server.

## What you should see

- Title path: raceGPS / CLEVELAND HISTORIC CIRCUIT
- Grid: orange **Hellcat** (player), black Charger, silver Charger
- Output Log: `[raceGPS] paint MID ... Base_color` (CARLA clearcoat param)
- Lake Erie north, downtown silhouette south, 15:00 sun, volumetric clouds
- 3-2-1 via existing `URaceSessionManager` countdown

If cars are unpainted, the log still names the MID; we then bind only that param.

## Paint parameters (uasset dump, not a guess)

`MI_DodgeCharger2024_BodyWork` instances `M_CarPaint_Master_New` and overrides:

- `Base_color`
- `Base_color_flakes`

Master also exposes `BaseColor`, `Base Color`, `Dirt Color`, `Dirt Rim Color`. `ApplyVehicleLook` writes all of them.

## Karla

`scripts/karla_visual_kernel.py` builds `skyline.json` from OSM / citypack buildings. Offline, no Cesium, no CARLA server. That is the start of automatic skyline materialization — not a second race engine.
