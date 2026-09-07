# STACK — raceGPS (Akron Beta + Cleveland Showcase)

One-pager for agents. Details: [`AGENTIC_HANDOFF_CLEVELAND.md`](AGENTIC_HANDOFF_CLEVELAND.md).

| | |
|--|--|
| **Product** | Arcade real-world city racing (Midnight Club–class feel) |
| **Engine** | Unreal Engine **5.7** |
| **Primary app** | `apps/unreal-akron-beta` → target `raceGPSAkronBeta` / `raceGPSAkronBetaEditor` |
| **Default GameMode** | `CruiseSprintGameMode` (**must stay** GlobalDefault) |
| **Cleveland map** | `/Game/Maps/Cleveland5_0KmWorld` |
| **Cleveland GM** | `ClevelandShowcaseGameMode` via `LaunchCleveland.bat` |
| **Physics** | Chaos Vehicles (+ planned Arcade Control / R-Tune / ArcadeVS / KinetiForge) |
| **Hero vehicle** | CARLA `BP_DodgeCharger2024` + `RaceGPSVehicleWheels` Front/Rear CDOs |
| **City data** | OSM → Python semantic / citypack → UE HISM (T10 ~120k) + **Karla** skyline |
| **Cesium** | Spike **paused** (ion token) |
| **Lang / tooling** | C++ (gameplay), Python (`scripts/`, `Content/Python/`), pytest |
| **Repo** | https://github.com/lumenhelixsolutions/raceGPS.git |
| **Cleveland branch** | `feature/cleveland-showcase-demo` |
| **Playtest report** | `apps/unreal-akron-beta/Temp/cleveland_playtest_lap.txt` |

**Build:** `raceGPSAkronBetaEditor` Win64 Development. **Run Cleveland:** `apps/unreal-akron-beta\LaunchCleveland.bat`.
