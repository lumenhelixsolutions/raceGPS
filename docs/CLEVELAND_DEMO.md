# Cleveland Historic Circuit — demo

**Title:** raceGPS: Cleveland Historic Circuit  
**Engine on this branch:** Unreal Engine **5.7** (spec text that says 5.5 is stale — **do not downgrade**).  
**CARLA server:** not used. `carla_required: false`.  
**Cesium:** not required for this offline pack. `cesium_required: false`.  
**Garage:** skip.  
This document does **not** claim a packaged Unreal build was run.

## What you should see

- Menu product line: **raceGPS**
- Circuit line: **Cleveland Historic Circuit** (not a 1982 “500” layout, no title-sponsor string)
- Flow: **Menu → RACE → countdown → racing → finished**
- Grid: **1 player + 2 Chaos AI** (`ARaceGridManager`, `ARaceAIDriverController` possessing Chaos vehicle pawns)
- Distance: **one lap** (`TargetLaps = 1`)
- Track data: `citypacks/cleveland/burke_gp_1997/` (`manifest.json` → xodr, racing_line, checkpoints, metadata)

## Build / run (existing Akron-beta app)

Use the **existing** project docs, not a new engine install:

1. Open `apps/unreal-akron-beta/` (this branch).
2. Follow **`BUILD.md`** and/or run **`Build.bat`** as already documented for Akron-beta.
3. Editor: UE **5.7**. World Settings / `DefaultEngine.ini`: game mode `AClevelandShowcaseGameMode` (see `CLEVELAND_CPP_WIREUP.md` if present).
4. From the **menu**, choose **RACE** (not garage, not CARLA connect).
5. Confirm three grid slots spawn on the racing line, countdown, then one lap.
6. AI inputs stay gated until session state is Racing.

Offline: the citypack JSON/XODR is local. No Cesium tile fetch is required to exercise the spline/AI loop.

## Citypack path

`citypacks/cleveland/burke_gp_1997/`

| File | Role |
|---|---|
| `manifest.json` | id `cleveland_burke_gp_1997`, UE5, offline |
| `cleveland_burke_gp.xodr` | OpenDRIVE 1.4 closed loop, flat 174 m |
| `racing_line.json` | WGS84 samples ~8 m, curvature +left |
| `checkpoints.json` | S/F + turns + wrap |
| `metadata.json` | official 2.106 mi / measured haversine |

## Remaining M5–M9

Not done in this data pass (do not tick these as shipped):

- **M5** Driveable pit-lane XODR (1982 T1/T2 extended exit is metadata only).
- **M6** PIE / packaged smoke on a GPU editor (this pass did not run Unreal).
- **M7** HUD polish (standings, branding line locked, finished screen).
- **M8** Replay + telemetry export of the Cleveland lap.
- **M9** Multi-lap / weekend structure beyond the 1-lap showcase.

Pytest for the pack: `pytest tests/test_cleveland_circuit.py` from the showcase root (or repo root if this tree is merged).
