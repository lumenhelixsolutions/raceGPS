# Akron Semantic Compiler

Compiles Akron, Ohio into a game-ready semantic driving package for the raceGPS Unreal beta.

**Pure Python. No CARLA dependency.**

## Pipeline

```
┌─────────────┐   ┌─────────────┐   ┌─────────────┐   ┌─────────────┐
│  fetch_osm  │──►│ build_road  │──►│ generate_   │──►│   export    │
│  (or cache) │   │    graph    │   │    xodr     │   │   manifest  │
└─────────────┘   └─────────────┘   └─────────────┘   └─────────────┘
       │                 │                 │                 │
       ▼                 ▼                 ▼                 ▼
  akron_road_      road_graph      akron.xodr       manifest.json
  graph.json       (in-memory)     (OpenDRIVE)      routes/*.json
```

### 7-Step Pipeline (`compile_akron.py`)

| Step | Script | Output | Description |
|------|--------|--------|-------------|
| 1 | `fetch_osm.py` (cached) | `akron_road_graph.json` | Downloads OSM data for Akron bounds |
| 2 | `osm_to_xodr.py` | `akron.xodr` | Generates OpenDRIVE 1.4 XML from road graph |
| 3 | `road_graph.py` | Road graph (in-memory) | Parses OSM → semantic roads + intersections |
| 4 | `route_generator.py` | `routes/*.json` | Generates connected cruise sprint routes |
| 5 | `checkpoint_generator.py` | Embedded in routes | Places checkpoint gates along routes |
| 6 | `poi_classifier.py` | Embedded in manifest | Classifies landmarks and POIs |
| 7 | `export_unreal_bundle.py` | `manifest.json` | Combines everything into game-ready bundle |

### Key Design Decisions

- **Pure Python** — No CARLA, no OSMnx, no external geospatial libraries. Only standard library + `xml.etree`.
- **WGS84 → Local Meters** — Uses tmerc projection centered on data bounds for OpenDRIVE coordinates.
- **Graceful Degradation** — If OSM Overpass returns HTTP 504, falls back to cached `akron_road_graph.json`.
- **Reproducible** — Fixed RNG seed (`random.Random(42)`) for consistent route generation.

---

## Install

```bash
cd tools/akron-semantic-compiler
python -m venv .venv
.venv\Scripts\activate  # Windows
# source .venv/bin/activate  # macOS/Linux
pip install -r requirements.txt
```

> **Windows Note:** If `python` is not found, use `py` instead:
> ```powershell
> py -m venv .venv
> .venv\Scripts\activate
> py compile_akron.py
> ```

---

## Run

```bash
python compile_akron.py
```

Or on Windows:
```powershell
py compile_akron.py
```

Output lands in `citypacks/akron-oh-beta-001/`:

```
citypacks/akron-oh-beta-001/
├── manifest.json                      # City metadata, spawn points, POIs, route list
├── akron.xodr                         # OpenDRIVE road network (XML)
└── routes/
    ├── akron_cruise_sprint_001.json   # Route 1: ~4.1km, 5 checkpoints
    └── akron_cruise_sprint_002.json   # Route 2: ~2.8km, 5 checkpoints
```

### Caching Behavior

- OSM data is cached as `akron_road_graph.json` to avoid repeated Overpass API calls.
- If the fetch step fails (timeout, HTTP 504), the pipeline falls back to the cached file.
- Delete the cache to force a fresh OSM download.

---

## Output File Reference

### `manifest.json`
```json
{
  "city": "Akron, OH",
  "bounds": { "west": -81.65, "south": 40.95, "east": -81.35, "north": 41.20 },
  "origin": { "lat": 41.08, "lon": -81.52 },
  "road_count": 1370,
  "junction_count": 1214,
  "spawn_points": [...],
  "pois": [...],
  "routes": ["akron_cruise_sprint_001", "akron_cruise_sprint_002"]
}
```

### `akron.xodr`
Valid OpenDRIVE 1.4 XML with:
- `<road>` elements with `<planView>` geometry (line segments)
- `<lanes>` with center + right driving lanes
- `<link>` predecessor/successor junction references
- `<geoReference>` with tmerc projection

### `routes/*.json`
Per-route JSON containing:
- `route_id`, `distance_meters`, `num_checkpoints`
- `start` / `finish` coordinates
- `waypoints[]` — spline control points
- `checkpoints[]` — gate locations and rotations

---

## Extending to New Cities

To compile a different city:

1. Update `AKRON_BOUNDS` in `compile_akron.py` to your city's bounding box
2. Update the cache filename from `akron_road_graph.json`
3. Update output directory from `akron-oh-beta-001/`
4. Run `compile_akron.py`

The compiler is city-agnostic — it only needs a bounding box and an output path.
