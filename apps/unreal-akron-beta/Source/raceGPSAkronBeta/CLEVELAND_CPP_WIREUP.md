# Cleveland showcase C++ wire-up (UE 5.7 / raceGPSAkronBeta)

This drop is a **parallel grid/AI layer**. It does not replace `URaceSessionManager`
(Menu → Countdown → Racing → Paused → Finished). Opponents are **AChaosVehiclePawn**
possessed by `ARaceAIDriverController` (AAIController). Do **not** use `ATrafficVehicle`.

## UBT / Visual Studio

New sources live under:

- `Source/raceGPSAkronBeta/Public/*.h`
- `Source/raceGPSAkronBeta/Private/*.cpp`

UBT compiles every `.cpp` in the module directory tree automatically. You do **not**
list them in the `.Build.cs`. After pulling these files:

1. Close the editor if it has this module loaded.
2. Right-click the `.uproject` → Generate Visual Studio project files (or
   `UnrealBuildTool -projectfiles`).
3. Build `raceGPSAkronBeta` (Editor target). PCH + C++20 already match this module.

### Module dependencies

Existing `PublicDependencyModuleNames` already include ChaosVehicles, EnhancedInput,
UMG, Json, XmlParser, ProceduralMeshComponent, Niagara, HTTP, WebSockets.

**Add `AIModule`** if it is not already there (`ARaceAIDriverController` subclasses
`AAIController`). Engine module is implied.

No CARLA runtime dependency. No new third-party binaries.

## Selecting ClevelandShowcaseGameMode

`AClevelandShowcaseGameMode` extends the existing CruiseSprint / race **flow** by
*composing* `URaceSessionManager` (subobject) plus `ARaceGridManager`. It subclasses
`AGameModeBase` so it does not require the garage.

Pick it in any of these ways:

1. **World Settings** on the Cleveland map: GameMode Override → `ClevelandShowcaseGameMode`.
2. **DefaultEngine.ini** (map-specific or default):

   ```
   [/Script/EngineSettings.GameMapsSettings]
   GlobalDefaultGameMode=/Script/raceGPSAkronBeta.ClevelandShowcaseGameMode
   ```

3. Open the level with `?game=/Script/raceGPSAkronBeta.ClevelandShowcaseGameMode`.

Blueprint subclassing is optional. If your Chaos vehicle is a Blueprint child of
`AChaosVehiclePawn`, set `ARaceGridManager::VehicleClass` on a BP child of the
grid actor, or set it from the GameMode after spawn.

HUD: bind title via `GetHudTitleLine()` (`raceGPS` / `CLEVELAND HISTORIC CIRCUIT`),
standings via `GetStandings()` / `GetPlayerPlace()` (`n/3`), and per-car telemetry
UPROPERTY mirrors on `ARaceAIDriverController`.

## JSON / citypack load path

`AClevelandShowcaseGameMode::LoadCityPack()` and `URacingLineComponent` search, in order:

1. `ProjectContentDir()/citypacks/cleveland/burke_gp_1997/<file>`
2. `ProjectContentDir()/Dir/citypacks/cleveland/burke_gp_1997/<file>`  (Content/Dir)
3. `ProjectDir()/citypacks/cleveland/burke_gp_1997/<file>`
4. `ProjectContentDir()/<file>` as a last resort

Files:

- `racing_line.json` — samples with `lat`/`lon` (or `latitude`/`longitude`/`lng`),
  `s` (meters auto-promoted to cm if max s < 50 000), `curvature` (1/m).
- `checkpoints.json` — array `checkpoints` / `gates` / `points` → `TotalCheckpoints`.

World conversion is **Z-up, 1uu = 1cm**, X=east, Y=north (`URacingLineComponent::GeoToWorld`).
This matches GeoToWorld on this branch.

## Session gating (grid)

AI `Tick` zeros throttle/steer/brake and holds handbrake unless
`URaceSessionManager::GetCurrentState() == Racing`. Vehicles are spawned **before**
countdown completes. `StartSession` / `TickSession` / `StartRace` stay on the
existing manager (countdown default 3 s).

Restart = `AClevelandShowcaseGameMode::RestartShowcase()` → respawn grid + `StartSession`.

## Control math lockstep

`RaceAIControlMath` in `Public/ClevelandShowcaseTypes.h` **must stay in sync** with
`tests/test_race_ai_control.py`. That Python suite does not need Unreal.

## RaceSessionManager

No rewrite. See `RACESESSIONMANAGER_PATCH.md` for the one additive getter the AI
layer needs if it is not already public.
