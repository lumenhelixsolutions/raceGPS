# Akron Semantic Compiler

Compiles Akron, Ohio into a game-ready semantic driving package for the raceGPS Unreal beta.

## Pipeline

```
fetch_osm.py          → Download Akron OSM data
osm_to_xodr.py        → Convert OSM → OpenDRIVE (.xodr) via CARLA
road_graph.py         → Build semantic road graph (intersections, lanes, turns)
route_generator.py    → Generate Cruise Sprint routes
checkpoint_generator.py → Place checkpoint gates along routes
poi_classifier.py     → Classify landmarks and POIs
export_unreal_bundle.py → Combine into citypack + semantic manifest
```

## Install

```bash
cd tools/akron-semantic-compiler
python -m venv .venv
.venv\Scripts\activate  # Windows
pip install -r requirements.txt
```

## Optional: CARLA for OpenDRIVE

If you want `.xodr` output, install CARLA Python API:

```bash
pip install carla  # or pip install <path-to-carla>/PythonAPI/carla/dist/carla-*.whl
```

## Run

```bash
python compile_akron.py
```

Output lands in `citypacks/akron-oh-beta-001/`.
