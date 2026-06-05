# raceGPS Akron Beta — Content Directory

This directory contains Unreal Engine assets for the raceGPS Akron Beta.

## Structure

```
Content/
├── Maps/
│   └── AkronWorld.umap          # Main World Partition level for Akron
├── Vehicles/
│   └── Sedan/                   # Placeholder sedan vehicle mesh & physics
├── Materials/
│   └── Roads/                   # Road surface materials
├── UI/
│   └── WBP_HUD.uasset           # UMG HUD widget (to be created in-editor)
└── Blueprints/
    └── BP_PlayerVehicle.uasset  # Chaos Vehicle pawn BP wrapper
```

## Asset Generation Pipeline

1. **CityPack Import**: The semantic compiler (`tools/akron-semantic-compiler/`) generates:
   - `akron.xodr` — OpenDRIVE road network
   - `manifest.json` — Routes, spawns, POIs, bounds
   - `routes/*.json` — Per-route spline waypoints + checkpoints

2. **Runtime Import**: The C++ `AkronXodrImporter` loads these JSON files at level start and:
   - Spawns road splines for the Chaos Vehicle to drive on
   - Places checkpoint gate actors
   - Sets player spawn location from manifest
   - Registers POIs for HUD minimap

3. **XODR Road Mesh**: Long-term, the XODR should be converted to static meshes via:
   - CARLA `Osm2Odr` → `.xodr` → custom mesh baker, or
   - Runtime procedural mesh generation from parsed road geometry

## World Partition Setup

- `AkronWorld.umap` uses **World Partition** for streaming
- Grid size: 128x128 cells
- Each cell ~500m × 500m
- Runtime streaming based on player vehicle position

## Placeholder Assets

Until final art passes, use:
- **Vehicle**: `/Game/Vehicles/Sedan/Sedan_SkelMesh` (Epic sample)
- **Road**: Procedural mesh or basic plane with asphalt material
- **Checkpoint**: Blueprint with BoxComponent overlap + Niagara particle ring
- **POI**: Simple static mesh billboard
