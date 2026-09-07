# Cleveland Historic Circuit — M5 environment dressing

**Title:** raceGPS: Cleveland Historic Circuit  
**Offline:** no Cesium, no network fetch, no CARLA.  
This document does **not** claim Unreal Editor, PIE, or a packaged build was run. No FPS or playtest screenshots.

## What was built

Camera-needed near-field and far-field geometry for Burke Lakefront (1997 10-turn layout):

| Layer | Source | Runtime |
|---|---|---|
| Lake Erie sheet (north of the circuit) | `water.json` | PMC polygon, `Water_Surface` |
| Infield grass | `track_dressing.json` | PMC, `Vegetation_Grass` |
| Runway 06L/24R vs taxiway G | coarse lat/lon AABBs | `Road_Asphalt` |
| Concrete / tire barriers | 9 m offset of `racing_line.json` | PMC boxes |
| Cones | denser at T1 + grid | PMC cones |
| Start/finish stripe | pose at s=0 | `Road_Marking` |
| 3 grid slots | s=0 minus 8 m spacing | poses only (pawn spawn stays on `ARaceGridManager`) |
| Pit visual | 1982 T1/T2 polyline | metadata, not XODR |
| 4 hangar boxes | south of G | `Building_Concrete` |
| Downtown skyline silhouette | `skyline.json` 45 volumes | extruded boxes, Glass/Concrete by height |
| Day lighting | `ADayNightCycle` ~14:00 | spawn if missing |

World conversion matches `URacingLineComponent::GeoToWorld`: **Z-up, 1uu=1cm, X=east, Y=north**.

JSON lives in `citypacks/cleveland/burke_gp_1997/`. Regenerate dressing with:

```
python3 scripts/build_cleveland_environment.py
```

## How GameMode spawns the actor

`AClevelandShowcaseGameMode::BeginPlay` already spawns `ARaceGridManager`, loads the citypack, and starts `URaceSessionManager`. This drop **adds** (does not replace) a spawn of `AClevelandEnvironmentActor` after `LoadCityPack()`.

See `apps/unreal-akron-beta/Source/raceGPSAkronBeta/CLEVELAND_ENV_WIREUP.md`.

`AClevelandEnvironmentActor::BeginPlay` then:

1. Spawns `ADayNightCycle` at 14:00 if none exists.
2. Resolves JSON via `ProjectDir()/citypacks/cleveland/burke_gp_1997` then Content/ variants (same order family as the racing line).
3. Builds procedural meshes. Materials come from existing `URaceGPSMaterialProvider` slots when that header is present.

Do **not** route race barriers through `AStreetFurnitureSpawner::SpawnBarrier` (city-intersection furniture). The dedicated actor is the race path.

`ABuildingMeshGenerator` is optional; skyline volumes are PMC boxes so the level does not depend on it.

## Non-goals

- No downtown storefronts, interiors, or cadastral lots.
- No Cesium / 3D Tiles / photogrammetry.
- No CARLA server.
- No 811-pond hydro dump from a 5 km citypack.
- No driveable pit-lane XODR (still metadata; visual polyline only).
- No new huge uassets.
- No claim that Unreal was executed.

## Assumptions (labelled in JSON)

Skyline lat/lon are Wikipedia/OSM centroid approximations. Barrier offset is a constant 9 m from the reconstructed centerline, not a catch-fence survey. T1 vortex (s 700–860) is left open. Lake south edge is `racing_line` max latitude plus ~220 m toward Lake Erie.


## V5 airport polish (track dressing)

Hangars expanded to ~10 real-ish Burke footprints south of taxiway G (T-rows, FBO, FSDO, maint). They **complement** the baked `Cleveland5_0KmWorld` HISM city (119k instances) and do **not** replace it. No terminals.

`airport_boxes[].mesh_path` + `hangar_mesh_candidates` are offline CARLA `LoadObject` paths. `Content/Carla/Static` currently has only `Car/`, `GenericMaterials/`, `Truck/` — **no hangar uassets** — so C++ instances boxes. No CARLA server.

Taxiway G centerline dashes + 06L/24R hold-shorts live in `track_dressing.json#taxiway_markings` (Road_Marking, cap 240). Cones denser on T1/grid/T4/T8/T9–T10 plus runway-edge dashes (cap 360). Extra tire stacks on T4/T8/T9/T10 only; T1 vortex stays open. Barriers cap 1100.

Regenerate **dressing only** (do not clobber skyline/water owned by the camera pass):

```
python scripts/build_cleveland_environment.py --dressing-only
```

Karla hangar extract (optional, does not rewrite skyline):

```
python scripts/karla_visual_kernel.py --buildings <osm.json> --racing-line citypacks/cleveland/burke_gp_1997/racing_line.json --out <tmp-skyline.json> --airport-out <hangars.json>
```
