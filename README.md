# raceGPS 🏎️🌍

> **Real-world arcade racing.** Drive actual streets. Race your city. No fictional tracks — just you, your car, and the real world rendered in Unreal Engine 5.

[![Unreal Engine 5.5](https://img.shields.io/badge/Unreal%20Engine-5.5-blue.svg)](https://www.unrealengine.com/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Windows](https://img.shields.io/badge/Platform-Windows-lightgrey.svg)]()

---

## What is raceGPS?

raceGPS is an **open-source desktop arcade racing game** built on top of real-world map data. Instead of fictional tracks, you race on actual city streets rendered in 3D from OpenStreetMap and OpenDRIVE semantic road networks.

### Key Features (v0.1.0-beta)

- 🗺️ **Real-world Akron, Ohio** — 1,370 real roads, 1,214 junctions, 2 generated race routes
- 🏎️ **Arcade physics** — Chaos Vehicles tuned for drift-friendly, fun handling
- 🎮 **Cruise Sprint mode** — Checkpoint-to-checkpoint racing with route ribbons
- 👻 **Ghost replay** — Race against your own best time
- 🏆 **Leaderboards & medals** — Gold, Silver, Bronze thresholds per route
- 🎯 **10 achievements** — Persistent unlocks with JSON save data
- 📖 **Interactive tutorial** — 5-step onboarding for new players
- 🌙 **Day/night cycle** — Dynamic lighting and time of day
- 🚦 **Traffic system** — AI vehicles spawning dynamically around the player
- 🛠️ **Developer console** — In-game cheats and debug commands

---

## Quick Start

### Download & Play (Recommended)

1. Download the latest release from the [Releases](https://github.com/lumenhelixsolutions/raceGPS/releases) page
2. Extract the `.zip`
3. Run `raceGPS.exe`
4. Select a route, pick your vehicle, and race

### Build from Source

See [`apps/unreal-akron-beta/BUILD.md`](apps/unreal-akron-beta/BUILD.md) for full build instructions.

**Prerequisites:**
- Unreal Engine 5.5
- Visual Studio 2022 with C++ game dev workload
- Python 3.10+ (for semantic compiler)

```powershell
cd apps\unreal-akron-beta
.\Build.bat
```

---

## Controls

| Action | Keyboard | Gamepad |
|--------|----------|---------|
| Throttle | W | RT |
| Brake / Reverse | S | LT |
| Steer Left | A | Left Stick Left |
| Steer Right | D | Left Stick Right |
| Handbrake | Space | A (Face Bottom) |
| Reset Vehicle | R | — |
| Toggle Camera | C | — |
| Pause | P / Esc | Menu |
| Developer Console | `~` (Tilde) | — |

---

## System Requirements

| Tier | Minimum | Recommended |
|------|---------|-------------|
| **OS** | Windows 10 64-bit | Windows 11 64-bit |
| **CPU** | Quad-core 2.5 GHz | 6-core 3.5 GHz |
| **RAM** | 8 GB | 16 GB |
| **GPU** | GTX 1060 / RX 580 | RTX 3060 / RX 6700 XT |
| **Storage** | 5 GB SSD | 5 GB NVMe SSD |
| **DirectX** | Version 12 | Version 12 |

---

## Project Structure

```
raceGPS
├── apps/unreal-akron-beta/     # UE5.5 C++ project (35+ classes)
│   ├── Source/                  # C++ gameplay, world, UI, systems
│   ├── Config/                  # Engine / Game / Input INIs
│   ├── Content/                 # Maps, materials, blueprints (Editor)
│   ├── citypacks/               # Generated city data
│   └── Build.bat                # Automated build script
├── tools/akron-semantic-compiler/  # Pure-Python OpenDRIVE generator
│   ├── compile_akron.py         # Main pipeline orchestrator
│   ├── osm_to_xodr.py           # OpenDRIVE XML writer
│   ├── route_generator.py       # Cruise sprint route builder
│   └── ...
├── docs/                        # Architecture & design docs
├── reference/web-renderer/      # Archived web renderer reference code
└── README.md                    # This file
```

---

## Data Pipeline

```
OpenStreetMap ──► Python Semantic Compiler ──► citypack/
                                                    │
                                              akron.xodr
                                              manifest.json
                                              routes/*.json
                                                    │
                                              UE5 Runtime
```

The **Akron Semantic Compiler** fetches OSM data, generates a valid OpenDRIVE 1.4 road network, builds cruise sprint routes, places checkpoints, classifies POIs, and exports a game-ready bundle.

---

## Development

```bash
# Type-check / lint (Python tools)
cd tools/akron-semantic-compiler
py -m compileall .

# Build UE5 project
cd apps/unreal-akron-beta
.\Build.bat
```

For architecture details, see [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).
For gameplay design, see [`docs/GAMEPLAY_DESIGN.md`](docs/GAMEPLAY_DESIGN.md).

---

## License

MIT — Free for personal and commercial use.

Map data © [OpenStreetMap](https://www.openstreetmap.org/copyright) contributors.
