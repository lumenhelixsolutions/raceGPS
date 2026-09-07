# Cleveland vehicle paint fix (V6)

Midnight Club cars need CARLA `M_CarPaint_Master_New` to actually sample
flake and dirt textures. Those packages were **never imported** into this
project, so paint MIDs compile with missing TextureSample inputs and the
Hellcat / asphalt / silver looks read as flat unlit plastic.

## What the logs say

From `apps/unreal-akron-beta/Saved/Logs/raceGPSAkronBeta.log` (and backups):

```
Skipped package /Game/Carla/Static/GenericMaterials/00_MastersOpt/Textures/T_dirt_01
Skipped package /Game/Carla/Static/GenericMaterials/00_MastersOpt/Textures/T_flakes_d
Skipped package /Game/Carla/Static/GenericMaterials/00_MastersOpt/Textures/T_flakes_n
```

Master material: `/Game/Carla/Static/GenericMaterials/00_MastersOpt/M_CarPaint_Master_New`

## Inventory (Content/Carla/.../00_MastersOpt/Textures/)

| Asset | Status |
|---|---|
| T_Flat_Black_d | present |
| T_Flat_n | present |
| T_Flat_orm | present |
| T_Flat_White_d | present |
| T_Stripes | present |
| T_VerticalStripes_n | present |
| T_WindShield_Masks | present |
| T_WindShield_Masks_01 | present |
| **T_flakes_d** | **MISSING** (log) |
| **T_flakes_n** | **MISSING** (log) |
| **T_dirt_01** | **MISSING** (log) |
| T_dirt_* anything else | not referenced |

No other flake/dirt textures exist under `Content/Carla`. Copying `T_Flat_*`
uassets under a new name is unsafe (internal names). Stubs are PNGs + import.

## What we did

1. Generated **placeholder PNGs** (256², not photogrammetry):
   - `Content/Carla/Static/GenericMaterials/00_MastersOpt/Textures/Source/T_flakes_d.png`
     dark metallic albedo + sparse bright sparkles
   - `.../T_flakes_n.png`  mostly-flat normal (128,128,255) + tiny bumps
   - `.../T_dirt_01.png`   near-clean dirt mask (high value = little dirt)
2. Unreal import script:
   `Content/Python/import_carpaint_stub_textures.py`
   Destination names match the log paths exactly.
3. `AChaosVehiclePawn::ApplyVehicleLook` still writes vector params
   **`Base_color`** and **`Base_color_flakes`** (confirmed 2026-08-22 from
   `MI_DodgeCharger2024_BodyWork`). Also writes `BaseColor` / `Dirt Color`
   aliases. Hellcat / black / silver additionally push **ClearCoat / Metallic /
   Roughness / FlakesAmount** scalars when those names exist on the MID.

## Import (UnrealEditor was not running)

Python Editor Script Plugin enabled, then either:

- Editor: File → Execute Python Script → `import_carpaint_stub_textures.py`
- Cmd:

```
"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
  "C:\projects\racegps\apps\unreal-akron-beta\raceGPSAkronBeta.uproject" ^
  -unattended -nopause -nullrhi ^
  -ExecutePythonScript="Content/Python/import_carpaint_stub_textures.py"
```

Import **completed** 2026-08-29 10:44 PM ET via UnrealEditor-Cmd `-ExecutePythonScript` with an **absolute** script path
(relative `Content/Python/...` resolves under Engine/Binaries and fails).

After import you should have:

- `/Game/Carla/Static/GenericMaterials/00_MastersOpt/Textures/T_flakes_d`
- `/Game/Carla/Static/GenericMaterials/00_MastersOpt/Textures/T_flakes_n`
- `/Game/Carla/Static/GenericMaterials/00_MastersOpt/Textures/T_dirt_01`

`T_flakes_n` must stay **non-sRGB / TC_NORMALMAP**. `T_dirt_01` is a linear mask.

## Grid looks (unchanged names)

| Slot | Look | Tint | Params |
|---|---|---|---|
| 0 PLAYER | Hellcat | Go-Mango orange `(1.00, 0.28, 0.05)` | Base_color, Base_color_flakes, high ClearCoat |
| 1 AI | Charger Asphalt | near-black | same vector names |
| 2 AI | Charger Silver | silver | same + high Metallic |

CARLA remains **assets in the project**, not a running CARLA server.

## Remaining visual gaps

- Stubs are solid/noisy placeholders, not CARLA's original flake sheet.
  Swap the three uassets later if the real CARLA textures are recovered.
- If Output Log still shows `Texture Sample> Missing input texture` after
  import, the master has additional unnamed samples (windshield was already
  present). Dump `M_CarPaint_Master_New` again.
- Lincoln MKZ 2024 is materials-only and is not spawned.
