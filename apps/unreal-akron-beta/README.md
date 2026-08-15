# raceGPS Akron Beta — Unreal Engine 5

Commercial-grade driving slice for Akron, Ohio. Built on **Unreal Engine 5.7** with **Chaos Vehicles**, **ProceduralMeshComponent**, and **OpenDRIVE** semantic ingestion via a pure-Python compiler pipeline.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         raceGPS Akron Beta                                   │
│  ┌─────────────┐  ┌──────────────┐  ┌─────────────────────┐  ┌──────────┐  │
│  │ ChaosVehicle │  │ CruiseSprint │  │ AkronXodrImporter   │  │ RoadMesh │  │
│  │ Pawn        │  │ GameMode     │  │ (JSON / XODR loader)│  │ Generator│  │
│  └─────────────┘  └──────────────┘  └─────────────────────┘  └──────────┘  │
│         │                │                      │                    │       │
│  ┌──────▼──────┐  ┌─────▼─────┐  ┌─────────────▼────────┐  ┌──────▼─────┐  │
│  │ VehicleAudio│  │ RaceScoring│  │ RouteSplineActor    │  │ Checkpoint │  │
│  │ TuningData  │  │ ReplayMgr  │  │ (ribbon + waypoints)│  │ Gate       │  │
│  └─────────────┘  └───────────┘  └─────────────────────┘  └────────────┘  │
│         │                │                      │                    │       │
│  ┌──────▼──────┐  ┌─────▼─────┐  ┌─────────────▼────────┐  ┌──────▼─────┐  │
│  │ NeonHUD     │  │ Minimap   │  │ DayNightCycle       │  │ Traffic    │  │
│  │ (Canvas)    │  │ Compass   │  │ (sun + sky)         │  │ Spawner    │  │
│  └─────────────┘  └───────────┘  └─────────────────────┘  └────────────┘  │
│         │                │                      │                    │       │
│  ┌──────▼──────┐  ┌─────▼─────┐  ┌─────────────▼────────┐  ┌──────▼─────┐  │
│  │ Leaderboard │  │ Achieve-  │  │ TutorialSystem      │  │ Settings   │  │
│  │ System      │  │ mentSystem│  │ + TutorialWidget    │  │ System     │  │
│  └─────────────┘  └───────────┘  └─────────────────────┘  └────────────┘  │
│                                                                              │
│  ┌────────────────────────────────────────────────────────────────────────┐  │
│  │                    Python Semantic Compiler                             │  │
│  │  OSM → Road Graph → OpenDRIVE → Routes → Checkpoints → POIs → Manifest │  │
│  └────────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Quick Start

### Prerequisites

- Unreal Engine 5.7 (via Epic Games Launcher or source)
- Visual Studio 2022 with "Game development with C++" workload
- Windows SDK 10.0.22000+
- Python 3.10+ (for semantic compiler)
- Git (with LFS recommended for large assets)

### Build

```powershell
cd apps\unreal-akron-beta
.\Build.bat
```

The batch script:
1. Auto-detects your UE5 install path
2. Generates `.sln` project files
3. Builds the Development Editor target
4. Cooks content and stages the build
5. Copies `citypacks/` to the output directory

### Manual Build (if needed)

1. **Generate project files**:
   ```powershell
   # Right-click raceGPSAkronBeta.uproject → Generate Visual Studio project files
   # OR via command line:
   "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" `
     -projectfiles -project="D:\projects\racegps\apps\unreal-akron-beta\raceGPSAkronBeta.uproject" `
     -game -engine -progress
   ```

2. **Build in Visual Studio**:
   - Open `raceGPSAkronBeta.sln`
   - Configuration: **Development Editor**
   - Platform: **Win64**
   - Build the `raceGPSAkronBeta` project

3. **Open in UE5 Editor**:
   - Double-click `raceGPSAkronBeta.uproject`
   - Load `Content/Maps/AkronWorld`

4. **Cook & Package**:
   - File → Package Project → Windows (64-bit)
   - Output: `Build/Windows/`

---

## Project Structure

```
apps/unreal-akron-beta/
├── Config/                          # Engine / Game / Input INIs
│   ├── DefaultEngine.ini
│   ├── DefaultGame.ini
│   └── DefaultInput.ini
├── Content/                         # Assets (maps, meshes, materials, UI)
│   ├── Maps/AkronWorld.umap
│   ├── Vehicles/
│   ├── Materials/
│   └── UI/
├── Source/raceGPSAkronBeta/
│   ├── raceGPSAkronBeta.Build.cs
│   ├── raceGPSAkronBeta.Target.cs
│   ├── raceGPSAkronBetaEditor.Target.cs
│   ├── Private/
│   │   ├── raceGPSAkronBeta.cpp           # Module entry
│   │   ├── raceGPSGameInstance.cpp        # Game instance (settings + progress)
│   │   ├── ChaosVehiclePawn.cpp           # Player vehicle + input + audio
│   │   ├── VehicleAudioComponent.cpp      # Engine pitch, tire screech, collision
│   │   ├── CruiseSprintGameMode.cpp       # Race lifecycle state machine
│   │   ├── RaceScoringSystem.cpp          # Time, penalties, bonuses, medals
│   │   ├── RaceReplayManager.cpp          # Orchestrates record + playback
│   │   ├── RaceReplayRecorder.cpp         # JSON frame capture
│   │   ├── RaceReplayPlayer.cpp           # Interpolated playback
│   │   ├── LeaderboardSystem.cpp          # Per-route JSON leaderboards
│   │   ├── AchievementSystem.cpp          # 10 achievements + JSON persistence
│   │   ├── TutorialSystem.cpp             # 5-step interactive tutorial
│   │   ├── ConsoleCommands.cpp            # Exec cheats + debug commands
│   │   ├── SettingsSystem.cpp             # Video/audio/control settings
│   │   ├── AkronXodrImporter.cpp          # OpenDRIVE / JSON importer
│   │   ├── RoadMeshGenerator.cpp          # Async procedural road meshes
│   │   ├── RouteSplineActor.cpp           # Route ribbon visualization
│   │   ├── CheckpointGate.cpp             # Overlap-based checkpoint gates
│   │   ├── DayNightCycle.cpp              # Rotating sun + HSV sky
│   │   ├── GhostVehicle.cpp               # Waypoint-following ghost car
│   │   ├── TrafficSpawner.cpp             # Radius-culled traffic spawning
│   │   ├── TrafficVehicle.cpp             # Simple road-following AI
│   │   ├── NeonHUD.cpp                    # Primary cyberpunk Canvas HUD
│   │   ├── AkronHUD.cpp                   # Legacy Canvas HUD
│   │   ├── DeveloperConsole.cpp           # Tilde-activated debug overlay
│   │   ├── MinimapWidget.cpp              # UMG minimap
│   │   ├── CompassWidget.cpp              # UMG compass
│   │   ├── MainMenuWidget.cpp             # UMG main menu
│   │   ├── PauseMenuWidget.cpp            # UMG pause menu
│   │   ├── LoadingScreenWidget.cpp        # UMG loading screen
│   │   ├── LeaderboardWidget.cpp          # UMG leaderboard display
│   │   ├── SettingsWidget.cpp             # UMG settings panel
│   │   ├── PostRaceStatsWidget.cpp        # UMG post-race breakdown
│   │   └── TutorialWidget.cpp             # UMG tutorial overlay
│   └── Public/
│       ├── Version.h                      # Compile-time version macros
│       ├── VehicleTuningData.h            # Data asset for vehicle tuning
│       ├── LeaderboardEntry.h             # Leaderboard entry struct
│       └── (headers matching .cpp files)
├── citypacks/
│   └── akron-oh-beta-001/
│       ├── manifest.json
│       ├── akron.xodr
│       └── routes/
├── Build.bat
├── raceGPSAkronBeta.uproject
└── README.md
```

**71 source files total** (35 `.cpp` + 36 `.h`)

---

## Systems Overview

### Vehicle
- **ChaosVehiclePawn** — Arcade-tuned Chaos Vehicle with throttle, steer, brake, handbrake, reset, camera switch
- **VehicleTuningData** — Data asset for mass, max speed, acceleration, grip, drift factor
- **VehicleAudioComponent** — Engine pitch by RPM, tire screech by slip, brake squeal, collision thud
- **Telemetry** — Speed, RPM, gear exposed for HUD

### Game Flow (Cruise Sprint)
```
Loading → Countdown → Racing → Finished
   │           │          │          │
   │      Tutorial     Timer     Post-Race
   │      (1st time)   Running   Stats + Medal
   │                            + Leaderboard
```
- **Loading** — Imports city data, spawns road meshes async, initializes replay recording
- **Countdown** — 3-2-1-GO timer; tutorial starts on first race
- **Racing** — Timer runs, checkpoints tracked, collisions penalized, clean driving bonused
- **Finished** — Final score calculated, leaderboard entry added, achievements checked, best replay saved, post-race stats shown

### Scoring Formula
```
Base Time      = raw elapsed time
Collision Penalty     = +2.0s per collision
Missed Checkpoint Penalty = +5.0s per missed checkpoint
Clean Driving Bonus   = -1.0s (if 0 collisions)
Final Time     = Base + Penalties - Bonus
Medal          = Gold ≤ 120s | Silver ≤ 150s | Bronze ≤ 200s
```

### Replay / Ghost
- **RaceReplayRecorder** — Captures position, rotation, velocity at 30 FPS to JSON
- **RaceReplayPlayer** — Interpolates between recorded frames for smooth playback
- **RaceReplayManager** — Orchestrates recording during race, saves best replay, loads + plays on ghost vehicle
- **GhostVehicle** — Waypoint-following pawn that plays back the best replay

### Leaderboard
- Per-route JSON persistence in `Saved/leaderboards/<RouteId>.json`
- AI seed entries for Gold/Silver/Bronze thresholds
- Player rank calculation
- Date, vehicle used, and collision count stored per entry

### Achievements
| ID | Title | Description | Target |
|----|-------|-------------|--------|
| first_race | First Steps | Complete your first race | 1 |
| gold_medalist | Gold Rush | Earn 3 gold medals | 3 |
| clean_driver | Clean Driver | Finish with zero collisions | 1 |
| speed_demon | Speed Demon | Reach 200 km/h | 1 |
| checkpoint_master | Checkpoint Master | Hit every checkpoint | 1 |
| explorer | Explorer | Drive 50km total | 1 |
| night_racer | Night Racer | Complete a race at night | 1 |
| traffic_dodger | Traffic Dodger | Avoid 100 traffic vehicles | 100 |
| replay_watcher | Ghost Buster | Beat your own ghost | 1 |
| cartographer | Cartographer | Compile a new city | 1 |

### Tutorial
5-step interactive onboarding:
1. **Getting Moving** — Throttle (W/S)
2. **Steering** — Steer (A/D)
3. **Drifting** — Handbrake (Space)
4. **Checkpoints** — Drive through gates (auto-advance after 5s)
5. **Good Luck!** — Complete the route (auto-advance after 3s)

### Traffic
- **TrafficSpawner** — Spawns TrafficVehicle pawns within a radius of the player, culls when out of range
- **TrafficVehicle** — Simple road-following AI with basic lane adherence

### Day/Night Cycle
- Rotating directional light (sun)
- HSV-adjusted sky atmosphere
- Configurable time-of-day speed

### Developer Console
Tilde (`~`) activated overlay showing FPS, RAM usage, vehicle position, and velocity.

### Console Commands (Cheats & Debug)
| Command | Category | Description |
|---------|----------|-------------|
| `CheatGodMode true/false` | Cheat | Disable collision damage |
| `CheatSetSpeed <km/h>` | Cheat | Instantly set vehicle speed |
| `CheatTeleportCheckpoint <index>` | Cheat | Teleport to checkpoint |
| `CheatFinishRace` | Cheat | Immediately finish the race |
| `CheatSetTime <seconds>` | Cheat | Set race timer |
| `DebugShowRoadGraph` | Debug | Visualize road graph |
| `DebugHideRoadGraph` | Debug | Hide road graph debug |
| `DebugSpawnVehicle <name>` | Debug | Spawn a vehicle by class name |
| `DebugSetTimeOfDay <hour>` | Debug | Set time of day (0-24) |
| `DebugClearTraffic` | Debug | Remove all traffic vehicles |
| `DebugReloadCity` | Debug | Reload city data and restart |

---

## Input Mapping

| Action | Keyboard | Gamepad |
|--------|----------|---------|
| Throttle | W / S (reverse) | RT / LT |
| Steer | A / D | Left stick X |
| Brake | S | LT |
| Handbrake | Space | A (Face Bottom) |
| Reset Vehicle | R | — |
| Toggle Camera | C | — |
| Pause | P / Esc | Menu |
| Developer Console | `~` (Tilde) | — |

---

## Data Contract

The game loads three data sources at runtime:

1. **`manifest.json`** — City metadata, spawn points, POIs, route list
2. **`akron.xodr`** — OpenDRIVE road network (XML, pure-Python generated, no CARLA)
3. **`routes/*.json`** — Per-route waypoint splines + checkpoint gates

All coordinates in the JSON are **WGS84 (lat, lon)**. The `AkronXodrImporter` converts them to Unreal world space using the origin from `manifest.json`.

---

## Development Notes

- **Vehicle physics:** Chaos Vehicles (not PhysX). Substepping enabled at 60Hz.
- **World streaming:** Not yet using World Partition. Road meshes load via async procedural generation at level start.
- **Road rendering:** `RoadMeshGenerator` creates quad-strip procedural meshes from XODR geometry with normals, UVs, and tangents. Frame-batched to avoid hitches.
- **XODR parser:** `AkronXodrImporter` reads OpenDRIVE XML directly. Pure-Python generation in `tools/akron-semantic-compiler/osm_to_xodr.py`.
- **Versioning:** Compile-time version in `Source/raceGPSAkronBeta/Public/Version.h`
- **Settings persistence:** Saved to `Saved/settings.json`
- **Achievement persistence:** Saved to `Saved/achievements.json`
- **Leaderboard persistence:** Saved to `Saved/leaderboards/<RouteId>.json`
- **Best times:** Stored in `raceGPSGameInstance` and serialized with game settings

---

## Level Setup Guide

`AkronWorld.umap` is a placeholder binary that must be finalized inside the Unreal Editor. The pipeline below makes this trivial and reproducible.

### Prerequisites

- Unreal Engine 5.7 Editor (with **Python Editor Script Plugin** enabled)
- Generated level spec (`generated/AkronWorld_LevelSpec.json`)

### Step 1: Generate the Level Spec

From the project root:

```powershell
cd D:\projects\racegps
python tools/generate-level-spec.py
```

This reads the semantic manifest, routes, spawn points, and POIs and produces `generated/AkronWorld_LevelSpec.json` with all actor locations in UE5 world space.

### Step 2: Open the Level in UE5 Editor

1. Launch the project:
   ```powershell
   .\apps\unreal-akron-beta\raceGPSAkronBeta.uproject
   ```
2. In the Editor, open **Content/Maps/AkronWorld**
3. If the map is empty (first time), add:
   - A **DirectionalLight** (for the sun)
   - A **SkyAtmosphere**
   - A **PlayerStart** (the import script will replace its position)

### Step 3: Import Actors via Python

Enable the **Python Editor Script Plugin** (Edit → Plugins → Scripting).

Open the **Python console** (Window → Developer Tools → Python):

```python
exec(open(r"D:\projects\racegps\tools\ue5-import-level-spec.py").read())
```

The script will:
- Place `PlayerStart` actors at each spawn point
- Create `SplineComponent` actors for every route
- Place checkpoint gate actors (`BP_CheckpointGate` if available, otherwise plain `Actor`)
- Rotate the directional light for the configured time-of-day
- Add reflection captures near landmark POIs
- Spawn traffic volume triggers along route paths

If you prefer to preview the commands outside the Editor:

```powershell
python tools/ue5-import-level-spec.py
```

This prints the full `unreal.EditorLevelLibrary` command sequence without executing it.

### Step 4: Build Lighting & Save

1. **Build Lighting** — Build → Build Lighting Only (Production quality recommended for packaging)
2. **Save the map** — File → Save Current → `Content/Maps/AkronWorld.umap`
3. Delete `Content/Maps/AkronWorld.umap.placeholder` (optional)

### Step 5: Package the Build

```powershell
cd apps\unreal-akron-beta
.\Build.bat
```

The batch script will warn you if the map has not been saved since the last level-spec generation.

---

## Adding a New City

The semantic compiler is city-agnostic. To add a new city:

1. **Define bounds** in `tools/akron-semantic-compiler/compile_akron.py` (copy and rename the script):
   ```python
   NEW_CITY_BOUNDS = {
       "west": -122.5,
       "south": 37.7,
       "east": -122.3,
       "north": 37.9,
   }
   ```

2. **Run the compiler**:
   ```powershell
   cd tools/akron-semantic-compiler
   python compile_akron.py
   ```
   Output lands in `citypacks/<city-id>/`.

3. **Generate the level spec**:
   ```powershell
   cd ../..
   python tools/generate-level-spec.py
   ```
   *Note:* Update `generate-level-spec.py` to point to the new `citypacks/<city-id>/` directory and output a matching `generated/<City>World_LevelSpec.json`.

4. **Create a new UE5 map** (`Content/Maps/<City>World.umap`) and run the UE5 import script against the new spec.

5. **Add the citypack to `Build.bat`** so it is copied during packaging.

For full compiler documentation, see `tools/akron-semantic-compiler/README.md`.

---

## License

MIT — Copyright LumenHelix Solutions
