# Changelog

## [0.1.0-beta] — 2026-06-04

### Major Pivot
- Project moved from browser-based Three.js/Babylon web renderer to packaged UE5 desktop application
- Deleted deprecated web packages (`apps/web-client/`, `packages/map-adapters/`)
- Preserved reference code in `reference/web-renderer/`

### UE5 Systems Shipped
- **Vehicle:** ChaosVehiclePawn with tuning data asset, VehicleAudioComponent (engine pitch, tire screech, brake squeal, collision), telemetry
- **Game Flow:** CruiseSprintGameMode state machine (Loading → Countdown → Racing → Finished), scoring, replay, leaderboard integration
- **World:** AkronXodrImporter, RoadMeshGenerator (async procedural mesh), RouteSplineActor, CheckpointGate, DayNightCycle
- **AI & Traffic:** GhostVehicle waypoint follower, TrafficVehicle, TrafficSpawner with radius culling
- **Replay:** RaceReplayRecorder (JSON frame capture), RaceReplayPlayer (interpolated playback), RaceReplayManager
- **Leaderboards:** Per-route JSON persistence with AI seed entries and medal thresholds
- **Settings:** Persistent video/audio/control settings with UMG widget base
- **HUD/UMG:** NeonHUD (cyberpunk Canvas aesthetic), MinimapWidget, CompassWidget, DeveloperConsole, MainMenuWidget, PauseMenuWidget, LoadingScreenWidget, LeaderboardWidget, SettingsWidget
- **New Systems:** TutorialSystem + TutorialWidget, AchievementSystem, PostRaceStatsWidget, ConsoleCommands
- **Build:** Build.bat, Version.h, config INIs

### Python Semantic Compiler
- Pure-Python OpenDRIVE 1.4 generation (no CARLA dependency)
- Road graph builder, route generator, POI classifier, checkpoint placer, bundle exporter
- 2 cruise sprint routes generated for Akron, OH (4.1km and 2.8km, 5 checkpoints each)
- Caching with graceful HTTP degradation

### Data
- Akron, OH: 1,370 roads, 1,214 junctions, POIs, spawn points

---

## 0.1.0

- Initial full GitHub repo stack for raceGPS.
