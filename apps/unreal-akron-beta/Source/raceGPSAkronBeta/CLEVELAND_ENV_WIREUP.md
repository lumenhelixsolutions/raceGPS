# Cleveland M5 environment wire-up

Additive dressing. **Do not replace** the grid/AI/session path in `AClevelandShowcaseGameMode`.

## BeginPlay spawn of `AClevelandEnvironmentActor`

This drop patches `ClevelandShowcaseGameMode.cpp` so `BeginPlay` still:

1. Spawns `ARaceGridManager`
2. `BindSession` + `LoadCityPack` + `BindHud`
3. **NEW:** `World->SpawnActor<AClevelandEnvironmentActor>()`
4. `SpawnGrid` + `SessionManager->StartSession`

If you already merged the previous GameMode and prefer not to take the full replacement file, add this after `LoadCityPack()`:

```cpp
#include "ClevelandEnvironmentActor.h"

EnvironmentActor = World->SpawnActor<AClevelandEnvironmentActor>(
    AClevelandEnvironmentActor::StaticClass());
```

You can also drop `AClevelandEnvironmentActor` into the Cleveland map; `BeginPlay` on the actor loads JSON and builds meshes even without the GameMode pointer. The GameMode spawn is the showcase default so a blank map still dresses.

## Files to copy into the UE module

- `Public/ClevelandEnvironmentActor.h`
- `Private/ClevelandEnvironmentActor.cpp`
- Optional: patched `Public/ClevelandShowcaseGameMode.h` + `Private/ClevelandShowcaseGameMode.cpp` (full replacement still contains grid + AI + session)

UBT picks up new `.cpp` files in the module tree. Existing `ProceduralMeshComponent` module dependency is already listed. No Cesium plugin. No new uassets.

## Materials / lighting

- `URaceGPSMaterialProvider` slots: `Water_Surface`, `Vegetation_Grass`, `Road_Asphalt`, `Road_Marking`, `Building_Concrete`, `Building_Glass`.
- `ADayNightCycle` spawned at **14:00** if the world has none. If the setter name on your branch differs from `SetTimeOfDayHours`, keep the spawn and set time in the level.
- `ABuildingMeshGenerator` is optional (skyline is PMC).
- Do **not** use `AStreetFurnitureSpawner` for race barriers.

## JSON search paths

Same family as `URacingLineComponent`:

1. `ProjectDir()/citypacks/cleveland/burke_gp_1997/<file>`
2. `ProjectContentDir()/citypacks/cleveland/burke_gp_1997/<file>`
3. `ProjectContentDir()/Dir/citypacks/cleveland/burke_gp_1997/<file>`

Ship the citypack JSON next to the existing `racing_line.json`.
