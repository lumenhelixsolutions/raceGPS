# Architecture

raceGPS is a **UE5.5 desktop application** with a Python semantic compiler pipeline that turns real-world OpenStreetMap data into playable driving cities.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         Data Pipeline                                       │
│  OpenStreetMap ──► Python Compiler ──► citypack/ ──► UE5 Runtime           │
│                                                                              │
│  OSM Overpass API    Pure Python          manifest.json                      │
│  (with cache fallback)                    akron.xodr (OpenDRIVE)            │
│                                           routes/*.json                      │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Runtime Architecture

### Module: `raceGPSAkronBeta`

A single UE5 C++ game module with 35+ classes across 5 subsystems.

#### Dependency Modules

```
raceGPSAkronBeta
├── ChaosVehicles          # Vehicle physics (AWheeledVehiclePawn)
├── ChaosVehiclesCore      # Low-level vehicle simulation
├── EnhancedInput          # Action/axis mapping
├── ProceduralMeshComponent # Async road mesh generation
├── UMG                    # Widget-based UI
├── Json, JsonUtilities    # Leaderboards, replays, settings
├── XmlParser              # OpenDRIVE XML parsing
└── Projects               # Version.h, build metadata
```

#### Subsystem Layers

```
Input Layer
│   EnhancedInput (Throttle, Steer, Brake, Handbrake, Camera, Reset, Pause)
│
▼
Vehicle Layer
│   ChaosVehiclePawn ──► VehicleTuningData (data asset)
│   │                    VehicleAudioComponent (engine, tire, brake, collision)
│   │                    Telemetry (speed, RPM, gear)
│   │
▼
Game Mode Layer
│   CruiseSprintGameMode ──► RaceScoringSystem
│   │                        RaceReplayManager
│   │                        LeaderboardSystem
│   │                        AchievementSystem
│   │                        TutorialSystem
│   │                        ConsoleCommands
│   │
▼
World Generation Layer
│   AkronXodrImporter ──► RoadMeshGenerator (async)
│   RouteSplineActor
│   CheckpointGate
│   DayNightCycle
│   TrafficSpawner + TrafficVehicle
│   GhostVehicle
│   │
▼
UI / HUD Layer
│   NeonHUD (Canvas, primary)
│   MinimapWidget (UMG)
│   CompassWidget (UMG)
│   LoadingScreenWidget (UMG)
│   PauseMenuWidget (UMG)
│   MainMenuWidget (UMG)
│   LeaderboardWidget (UMG)
│   SettingsWidget (UMG)
│   PostRaceStatsWidget (UMG)
│   TutorialWidget (UMG)
│   DeveloperConsole (Canvas overlay)
│   │
▼
Persistence Layer
│   raceGPSGameInstance ──► Saved/settings.json
│   LeaderboardSystem ──► Saved/leaderboards/*.json
│   AchievementSystem ──► Saved/achievements.json
│   RaceReplayManager ──► Saved/replays/*.json
```

---

## Data Flow: Race Lifecycle

```
StartPlay()
    │
    ├──► Load manifest.json + akron.xodr + routes/*.json
    │
    ├──► Spawn RoadMeshGenerator (async)
    │
    ├──► Initialize ReplayManager (begin recording)
    │
    ├──► Initialize LeaderboardSystem
    │
    ├──► Initialize AchievementSystem (load from disk)
    │
    ├──► Initialize TutorialSystem (configure 5 steps)
    │
    ├──► Initialize ConsoleCommands
    │
    └──► Show LoadingScreenWidget
              │
              ▼
        Loading complete → Countdown
              │
              ├──► 3-2-1-GO timer
              │
              ├──► Start Tutorial (if first race)
              │
              └──► Racing
                        │
                        ├──► Tick: ElapsedTime += DeltaTime
                        │
                        ├──► Tick: ReplayRecorder.CaptureFrame()
                        │
                        ├──► OnCheckpointReached → Advance checkpoint
                        │
                        ├──► OnVehicleCollision → Add penalty
                        │
                        └──► All checkpoints reached → FinishRace()
                                                        │
                                                        ├──► CalculateFinalScore()
                                                        ├──► UpdateBestTime() in GameInstance
                                                        ├──► AddLeaderboardEntry()
                                                        ├──► CheckAchievements()
                                                        ├──► SaveBestReplay() (if PB)
                                                        └──► Show PostRaceStatsWidget
```

---

## Python Semantic Compiler

The compiler is a **pure-Python** pipeline with zero external geospatial dependencies.

### Pipeline Flow

```
fetch_osm()          → Download OSM XML from Overpass API
                          │
                          ▼ (fallback to cached JSON on failure)
build_road_graph()   → Parse nodes, ways, relations → semantic road graph
                          │
                          ▼
generate_xodr()      → Write OpenDRIVE 1.4 XML (planView, lanes, links, geoReference)
                          │
                          ▼
generate_routes()    → Greedily follow connected roads from center-biased start
                          │
                          ▼
place_checkpoints()  → Distribute gates along route spline
                          │
                          ▼
classify_pois()      → Tag landmarks, stadiums, parks, bridges
                          │
                          ▼
export_manifest()    → Combine into game-ready bundle
```

### Projection

- **Input:** WGS84 (lat, lon) from OSM
- **Output:** Local meters in OpenDRIVE using **tmerc** centered on data bounds
- **Elevation:** Currently flat (0.0) — future work: SRTM / DEM integration

### Caching

- OSM data cached as `akron_road_graph.json` to avoid repeated Overpass API calls
- HTTP 504 / timeout gracefully falls back to cache
- Reproducible: fixed RNG seed (`random.Random(42)`)

---

## File Persistence

| Data | Format | Path | Written By |
|------|--------|------|------------|
| Settings | JSON | `Saved/settings.json` | SettingsSystem |
| Best Times | JSON (in Settings) | `Saved/settings.json` | raceGPSGameInstance |
| Achievements | JSON | `Saved/achievements.json` | AchievementSystem |
| Leaderboards | JSON per route | `Saved/leaderboards/<RouteId>.json` | LeaderboardSystem |
| Replays | JSON per route | `Saved/replays/<RouteId>.json` | RaceReplayManager |

---

## Performance Targets

| Metric | Target | Status |
|--------|--------|--------|
| Initial load | < 5s (SSD) | ✅ Road mesh async generation |
| Frame rate | 60 FPS @ 1080p | ✅ Target hardware |
| Traffic vehicles | 20-50 active | ✅ Radius culling |
| Road mesh batch | Frame-batched | ✅ No hitches |
| Replay recording | 30 FPS capture | ✅ JSON frame log |
