# raceGPS: Real World Driving — Master Handoff v3

**One Earth. Many cities. Many routes. One player identity. One global career.**

raceGPS is a **UE5.5 desktop arcade racing game** where players drive actual city streets rendered from OpenStreetMap and OpenDRIVE semantic data. The current release is the **Akron Beta** — a complete, playable slice featuring 1,370 real roads, 1,214 junctions, and 2 generated cruise sprint routes.

---

## Locked Modules (Shipped in v0.1.0-beta)

- ✅ **Chaos Vehicle Physics** — Arcade-tuned with drift-friendly handling
- ✅ **Cruise Sprint Game Mode** — Loading → Countdown → Racing → Finished
- ✅ **Race Scoring System** — Time, collision penalties, clean-driving bonus, medals
- ✅ **Ghost Replay System** — JSON frame capture + interpolated playback
- ✅ **Per-Route Leaderboards** — JSON persistence with AI seed entries
- ✅ **Achievement System** — 10 achievements with progress tracking
- ✅ **Interactive Tutorial** — 5-step onboarding for new players
- ✅ **NeonHUD** — Cyberpunk Canvas aesthetic with speedometer, RPM, timer
- ✅ **Day/Night Cycle** — Dynamic sun rotation and sky atmosphere
- ✅ **Traffic System** — Radius-culled AI vehicles
- ✅ **Developer Console** — In-game FPS/RAM overlay + cheat commands
- ✅ **OpenDRIVE Import** — Pure-Python semantic compiler pipeline
- ✅ **Procedural Road Meshes** — Async generation from XODR geometry
- ✅ **Settings Persistence** — Video, audio, control settings saved to JSON
- ✅ **Post-Race Stats** — Time breakdown, penalties, medals, rank

---

## Architecture at a Glance

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  OpenStreetMap ──► Python Semantic Compiler ──► citypack/ ──► UE5 Runtime │
│                                                                              │
│  OSM Overpass        Pure Python (no CARLA)     manifest.json              │
│  (cached fallback)                              akron.xodr (OpenDRIVE)      │
│                                                 routes/*.json                │
└─────────────────────────────────────────────────────────────────────────────┘
```

The UE5 runtime is a single C++ module (`raceGPSAkronBeta`) with 35+ classes across gameplay, vehicle, world generation, UI/HUD, and persistence layers.

---

## MVP Experience (v0.1.0-beta)

1. Player launches `raceGPS.exe`
2. Main menu loads with route selection
3. Player selects a cruise sprint route
4. Game loads city data and generates road meshes asynchronously
5. Countdown begins (tutorial starts if first race)
6. Player races through real-world checkpoints
7. Finish triggers post-race stats, leaderboard update, and achievement checks
8. Player can restart, view leaderboard, or return to menu

---

## Viral Hook

**"Drive your actual city."**

Type a location. Compile it. Race it.

The semantic compiler can theoretically target any city with OSM road data. Akron is the proof of concept.

---

## Safety Boundary

Crime-like gameplay is symbolic: tokens, timers, pings, heat, extraction, capture radius. No real-world crime instruction, police feeds, CCTV, or surveillance integration.

---

## Technical Notes

- **Platform:** Windows 10/11 x64
- **Engine:** Unreal Engine 5.5
- **Physics:** Chaos Vehicles (not PhysX)
- **Rendering:** ProceduralMeshComponent for roads, UMG for UI, Canvas for HUD
- **Data:** OpenDRIVE 1.4 XML, custom JSON manifest and routes
- **Compiler:** Python 3.10+, pure standard library + xml.etree
- **Persistence:** JSON files in `Saved/` directory (no database)
- **Multiplayer:** Not yet shipped. LAN foundation in v0.4.0 roadmap.

---

## Next Milestones

See `ROADMAP.md` for the full timeline. Immediate next steps:

1. **v0.2.0 — Visual Polish:** Cesium 3D Tiles, Niagara FX, materials, UMG Blueprint binding
2. **v0.3.0 — Content:** More routes, vehicle selection, custom keybindings
3. **v0.4.0 — Multiplayer:** LAN hosting, join/discovery, synchronized starts
4. **v1.0 — Release:** Additional cities, online leaderboards, Steamworks, installer
