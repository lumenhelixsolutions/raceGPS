# raceGPS Citypack Interface Contract

**Status:** Frozen interface contract (story S3). This document describes the documented
inputs, outputs, and data shapes of the raceGPS citypack pipeline that **must not silently
change** while multiple agents modify the toolchain.

**Scope:**

- Producer: `tools/universal-city-compiler/` (pure-Python OSM → citypack compiler)
- Companion generator: `tools/generate-level-spec.py` (citypack → UE5 level spec)
- Reference artifact: `citypacks/akron-oh-beta-001/` (the only shipped citypack)
- Consumers:
  - UE5 runtime: `apps/unreal-akron-beta/Source/raceGPSAkronBeta/` (notably
    `Private/AkronXodrImporter.cpp`, `Private/CruiseSprintGameMode.cpp`,
    `Private/MainMenuWidget.cpp`, `Private/StreetFurnitureSpawner.cpp`,
    `Private/BuildingMeshGenerator.cpp`, `Private/PreflightSystem.cpp`)
  - Editor-side: `tools/ue5-import-level-spec.py`

Every field named in §5 ("Fields the UE5 importer depends on") is **load-bearing** and
falls under the change policy in §6.

---

## 1. Inputs

### 1.1 CLI / orchestrator inputs

Entry points: `tools/universal-city-compiler/cli.py` → `city_compiler.compile_city(...)`.

| Input | Type | Default | Source |
|---|---|---|---|
| `city` (positional) | string — city name (`"Cleveland, OH"`) **or** `"lat,lon"` (`"41.5,-81.7"`) | required | `cli.py:21`, dispatch logic `city_compiler.py:46-59` |
| `--radius` | float (km) | `5.0` | `cli.py:22` |
| `--detail` | `"minimal" \| "standard" \| "full"` | `"standard"` | `cli.py:23-24` |
| `--routes` | int (routes per mode) | `4` | `cli.py:25` |
| `--seed` | int (route RNG reproducibility) | `42` | `cli.py:26` |
| `--output` | path (resolved against project root if relative) | `citypacks` | `cli.py:27`, `cli.py:34-35` |
| `--batch` | path to text file, one city per line | none | `cli.py:29-30`, `cli.py:38-48` |

`compile_city()` return shape (`city_compiler.py:38, 162`):

```python
{"success": True, "manifest": {...}, "citypack_dir": "..."}   # success
{"success": False, "error": "..."}                            # failure
```

### 1.2 Geocode inputs

`geocode.geocode_city(city_name)` (`geocode.py:13`) queries Nominatim
(`https://nominatim.openstreetmap.org/search`) and returns:

```python
{"name", "display_name", "lat", "lon",
 "bounds": {"south", "west", "north", "east"},   # floats, degrees
 "osm_type", "osm_id", "place_id"}
```

Bounds post-processing:
- `expand_bounds(bounds, padding_km)` — used for named cities with
  `padding_km = radius_km * 0.3` (`city_compiler.py:57`).
- `bounds_from_center(lat, lon, radius_km)` — used for `"lat,lon"` queries
  (`city_compiler.py:49`). Conversion constants: `111.0 km/deg lat`,
  `111.32 km/deg lon` (`geocode.py:82-83, 96-97`).

### 1.3 OSM fetch parameters

`fetch_overpass.fetch_and_cache(bounds, cache_path, detail)` (`fetch_overpass.py:106`)
POSTs Overpass QL to `https://overpass-api.de/api/interpreter` and caches raw XML at
`<citypack_dir>/<city_id>_raw.osm` (`city_compiler.py:69`). If the cache file exists it is
reused silently (`fetch_overpass.py:108-110`) — stale caches are a known reproducibility
hazard.

Detail levels (`fetch_overpass.py:13-82`):

| `detail` | Timeout | OSM content |
|---|---|---|
| `minimal` | 120 s | drivable `highway=*` ways + nodes only |
| `standard` | 180 s | + `building=*` ways/relations + `amenity/tourism/shop/historic/leisure` nodes |
| `full` | 300 s | + `natural=water|wood|scrub|heath|grassland`, `waterway=river|stream|canal`, `relation[water]`, `landuse=forest|grass|meadow|farmland|residential|commercial|industrial` |

The drivable-highway filter is fixed
(`fetch_overpass.py:16`): `motorway|trunk|primary|secondary|tertiary|residential|
unclassified|service|living_street|pedestrian`.

### 1.4 Derived IDs and other config knobs

- `city_id = f"{city_name}_{radius_km}km"` (`city_compiler.py:61`) for compiler-generated
  packs, e.g. `cleveland_oh_5.0km`. **Note:** the shipped Akron pack uses the hand-styled
  ID `akron-oh-beta-001` and does **not** follow this pattern (see §7).
- Checkpoint spacing: `place_checkpoints(route, spacing_meters=350.0)` as called in
  `city_compiler.py:92` (function default is `300.0`, `route_engine.py:134`);
  gate radius default `18.0` m (`route_engine.py:134`).
- Road width map and speed-zone table are hard-coded — see §2.6 and §2.8.

---

## 2. Citypack output format

A citypack is a directory `citypacks/<city_id>/` containing a manifest plus the files it
references. The Akron reference pack (`citypacks/akron-oh-beta-001/`) contains:

```
akron_semantic_manifest.json   (709 B)   manifest — the contract root
akron.xodr                     (3.8 MB)  OpenDRIVE road network
akron_road_graph.json          (33 MB)   semantic road graph (JSON fallback)
akron_routes.json              (25 KB)   route definitions
akron_spawn_points.json        (342 B)   spawn points
akron_pois.json                (259 KB)  classified POIs
akron_gameplay_layer.json      (649 B)   speed zones / gameplay zones
```

> ⚠ `akron_semantic_manifest.json` also references `akron_buildings.json`
> (`building_count: 25022`), but that file is **absent** from the shipped pack. See §7.

### 2.1 Manifest — `<city_id>_semantic_manifest.json`

There are **two live dialects** of the manifest. Both are in production use and both are
consumed (e.g. `tools/generate-level-spec.py:101-110` branches on the presence of `files`).

**Dialect A — "legacy Akron" flat-file-reference manifest** (the shipped pack;
`citypacks/akron-oh-beta-001/akron_semantic_manifest.json`, verbatim):

```json
{
  "city_id": "akron-oh-beta-001",
  "display_name": "Akron, Ohio",
  "version": "0.2.0",
  "game_version_min": "0.1.0",
  "game_version_max": "0.2.0",
  "origin": { "lat": 41.06999999999999, "lon": -81.525 },
  "bounds": { "west": -81.62, "south": 40.98, "east": -81.43, "north": 41.16 },
  "opendrive_file": "akron.xodr",
  "routes": "akron_routes.json",
  "road_graph": "akron_road_graph.json",
  "spawn_points": "akron_spawn_points.json",
  "pois": "akron_pois.json",
  "buildings": "akron_buildings.json",
  "gameplay_layer": "akron_gameplay_layer.json",
  "route_count": 2,
  "poi_count": 590,
  "spawn_point_count": 2,
  "building_count": 25022
}
```

Field reference (Dialect A):

| Field | Type | Meaning |
|---|---|---|
| `city_id` | string | Unique pack ID; also the directory name |
| `display_name` | string | Human-readable name. **Read by UE5** (`MainMenuWidget.cpp:126,322`) |
| `version` | string (semver) | Pack version. **Read by UE5** for compatibility checks (`CruiseSprintGameMode.cpp:283`, `MainMenuWidget.cpp:125`) |
| `game_version_min` / `game_version_max` | string (semver) | Declared compatible game range. Currently **not read by any consumer** (the game instead compares `version` major.minor against `RACEGPS_VERSION_STRING` = `"0.1.0-beta"`, `Version.h:6`) |
| `origin.lat` / `origin.lon` | number (WGS84 deg) | World origin for geo→world projection. **Read by UE5** (`StreetFurnitureSpawner.cpp:76-80`) and by `generate-level-spec.py:112-114` |
| `bounds.west/south/east/north` | number (WGS84 deg) | Fetch/validity bounding box |
| `opendrive_file` | string (filename) | XODR file relative to the pack dir |
| `routes`, `road_graph`, `spawn_points`, `pois`, `buildings`, `gameplay_layer` | string (filename) | Relative filenames of the data files |
| `route_count`, `poi_count`, `spawn_point_count`, `building_count` | integer | Stats; informational only (no consumer reads them) |

A JSON Schema for Dialect A lives at `docs/schemas/citypack-manifest.schema.json` and is
validated against the real Akron manifest.

**Dialect B — "universal compiler v2" manifest** (what `export_bundle.export_bundle()`
writes today, `export_bundle.py:73-92`):

```python
{
  "city_id": str,
  "name": str,                       # NOTE: "name", not "display_name"
  "version": "2.0.0",                # hard-coded, export_bundle.py:76
  "bounds": {"south","north","west","east"},
  "origin": {"lat","lon"},
  "road_count": int, "intersection_count": int,
  "route_count": int, "spawn_point_count": int,
  "poi_count": int, "building_count": int,
  "water_count": int, "vegetation_zone_count": int,
  "has_heightmap": bool, "biome": str | None,
  "files": {                          # file references live under "files", not flat
    "routes", "pois", "buildings", "spawn_points", "road_graph",
    # optional: "water", "vegetation", "heightmap", "biome"
  },
  "generated_by": "universal-city-compiler-v2",
  "generated_at": str | None,         # ISO-8601; set by city_compiler.py:132
  # city_compiler.py:133-136 additionally injects:
  "query": str, "display_name": str, "radius_km": float, "detail": str,
}
```

Dialect B optional data files (`export_bundle.py:57-71`): `<city_id>_water.json`,
`<city_id>_vegetation.json`, `<city_id>_heightmap.json`, `<city_id>_biome.json`.

> The two dialects disagree on `display_name` vs `name`, flat refs vs `files`, and
> `version` semantics (`0.2.0` pack-level vs hard-coded `2.0.0`). Unification is a
> hardening item (§7); until then **both shapes are contractual**.

### 2.2 Road graph — `<city_id>_road_graph.json`

Real shape (first road, `citypacks/akron-oh-beta-001/akron_road_graph.json`):

```json
{
  "roads": [
    {
      "id": "21378582",
      "name": "Franz Drive",
      "highway": "residential",
      "points": [ { "lat": 41.1267631, "lon": -81.6004505 }, ... ],
      "width": 7,
      "one_way": false
    }
  ],
  "intersections": [
    { "node_id": "229937243", "lat": 41.1267631, "lon": -81.6004505,
      "road_ids": ["21378582", "21436340", "1273220199", "1273220200"] }
  ],
  "node_count": 383737,
  "road_count": 35828,
  "intersection_count": 44644
}
```

Road object fields: `id` (string, OSM way id), `name` (string, may be `""`),
`highway` (OSM highway class string), `points` (array of `{lat, lon}` numbers, ≥ 2),
`width` (number, meters), `one_way` (boolean).

The current compiler (`road_network.py:67-77, 100-107`) emits a **superset** per road —
additionally `lane_count` (int), `max_speed` (int km/h), `surface` (string) — and at top
level additionally `bounds` and `origin`, but **not** `node_count`. Intersections match
the real shape (`road_network.py:83-88`).

Width defaults are hard-coded (`road_network.py:54-59`): motorway 14, trunk 12,
primary 10, secondary 9, tertiary 8, residential/unclassified 7, service 5,
living_street/pedestrian 6 (meters).

### 2.3 Routes — `<city_id>_routes.json`

Top level is a **bare JSON array** of route objects
(`citypacks/akron-oh-beta-001/akron_routes.json`):

```json
[
  {
    "route_id": "akron_cruise_sprint_001",
    "mode": "cruise_sprint",
    "name": "Akron Sprint 1",
    "difficulty": "beta",
    "distance_meters": 6152,
    "start":  { "lat": 41.0813843, "lon": -81.5198804 },
    "finish": { "lat": 41.085338,  "lon": -81.502178 },
    "points": [ { "lat": 41.0813843, "lon": -81.5198804 }, ... ],
    "checkpoints": [
      { "id": "cp_001", "lat": 41.083573, "lon": -81.517476,
        "heading": 25.7, "radius_meters": 18, "type": "gate" }
    ],
    "scoring": { "time_bonus": true, "clean_driving_bonus": true,
                 "missed_checkpoint_penalty": 10 }
  }
]
```

Compiler-emitted fields (`route_engine.py:122-131`): `route_id` (pattern
`{city_id}_{mode}_{NNN}`), `mode` ∈ `cruise_sprint | time_trial | circuit | drift_run`,
`name`, `difficulty` ∈ `easy | medium | hard | extreme` (the real pack uses `"beta"` — an
out-of-enum value), `distance_meters` (int), `start`, `finish`, `points`, plus
`checkpoints` injected by `city_compiler.py:92` with shape
`{id, lat, lon, heading, radius_meters, type, distance_from_start_m}`
(`route_engine.py:152-160`). The real pack's checkpoints lack `distance_from_start_m` and
use simple `cp_NNN` ids; the real pack adds a `scoring` object the compiler never emits.
All of these divergences are §7 items.

### 2.4 Spawn points — `<city_id>_spawn_points.json`

Bare array (`citypacks/akron-oh-beta-001/akron_spawn_points.json`, verbatim):

```json
[
  { "id": "spawn_akron_cruise_sprint_001", "lat": 41.0813843,
    "lon": -81.5198804, "heading": 25.7, "route_id": "akron_cruise_sprint_001" }
]
```

Fields: `id` (string, pattern `spawn_{route_id}`), `lat`/`lon` (numbers), `heading`
(number, compass degrees), `route_id` (string, references `routes[].route_id`).
Compiler emits the identical shape (`export_bundle.py:31-37`).

### 2.5 POIs — `<city_id>_pois.json`

Bare array (`citypacks/akron-oh-beta-001/akron_pois.json`):

```json
[
  { "id": "9709703135", "lat": 41.0853351, "lon": -81.5364408,
    "type": "shop", "name": "Carnival Gal Clothing",
    "tags": { "shop": "clothes", "addr:city": "Akron", "...": "..." } }
]
```

Fields: `id` (string, OSM node/way id), `lat`/`lon` (numbers; way POIs use centroid),
`type` (classified enum — observed in the real pack: `education, food, health, landmark,
recreation, service, shop`; the classifier can also emit `entertainment, finance,
government, accommodation`, `poi_engine.py:58-96`), `name` (string, may be `""`),
`tags` (object, raw OSM tags, values always strings). The compiler additionally emits
`subtype` (`poi_engine.py:32, 50`), which the real pack lacks.

### 2.6 Gameplay layer — `akron_gameplay_layer.json`

Object with four zone arrays (`citypacks/akron-oh-beta-001/akron_gameplay_layer.json`,
verbatim excerpt):

```json
{
  "speed_zones": [
    { "type": "highway", "highways": ["motorway", "trunk"], "speed_kmh": 90 },
    { "type": "arterial", "highways": ["primary", "secondary"], "speed_kmh": 65 },
    { "type": "local", "highways": ["tertiary", "residential", "unclassified"], "speed_kmh": 45 },
    { "type": "service", "highways": ["service"], "speed_kmh": 25 }
  ],
  "pickup_zones": [],
  "objective_zones": [],
  "challenge_zones": []
}
```

This file is **not emitted by the compiler at all** — it exists only in the Akron pack.

### 2.7 Buildings — `<city_id>_buildings.json` (expected, not shipped)

Compiler output shape (`building_extractor.py:44-54`, wrapped by
`export_bundle.py:42` as `{ "buildings": [...] }`):

```json
{ "buildings": [
    { "id": "...", "lat": 0.0, "lon": 0.0,
      "footprint": [ { "lat": 0.0, "lon": 0.0 }, ... ],
      "building_type": "residential", "name": "",
      "levels": 3, "height_meters": 8.0, "material": "concrete" } ] }
```

⚠ This **does not match** what UE5's `BuildingMeshGenerator.cpp:64-93` reads — it expects
`buildings[].{id, type, name, height, area_m2, footprint[].{x, y}}` (world-space x/y,
not lat/lon). See §7.

### 2.8 XODR expectations — `<opendrive_file>`

`citypacks/akron-oh-beta-001/akron.xodr` header (verbatim):

```xml
<OpenDRIVE>
  <header revMajor="1" revMinor="4" name="akron_oh_beta" version="1" date="2026-06-04"
          north="41.171881" south="40.971881" east="-81.419748" west="-81.619748">
    <geoReference>+proj=tmerc +lat_0=41.07188122706074 +lon_0=-81.51974830739853 +k=1 +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs</geoReference>
  </header>
  <road id="21379683" name="Unnamed" length="115.750993" junction="-1" rule="RHT">
    <planView>
      <geometry s="0.000000" x="-2355.815311" y="-2695.984168" hdg="-2.052079" length="18.147230">
        <line />
      </geometry> ...
```

Contractual expectations (derived from `AkronXodrImporter.cpp`):

- Root `<OpenDRIVE>` with `<header revMajor="1" revMinor="4">`.
- `header/geoReference` **must** be a PROJ string containing `+lat_0=` and `+lon_0=`
  tokens — the importer parses the world origin out of it with string search
  (`AkronXodrImporter.cpp:79-94`); if absent it silently defaults to
  Akron's `41.08, -81.52` (`:69-70`).
- Each `<road>` needs attribute `id`, a `<planView>` with `<geometry>` elements carrying
  `x`, `y` (meters, origin-relative, X=east/Y=north), and `length`.
- Lane width is read from `lanes/laneSection/right/lane/width@a` and **doubled**
  (`AkronXodrImporter.cpp:123-127`); presence of a `<left>` lane section sets
  `NumLanes = 2` (`:131-132`).
- Axis mapping is fixed in code: UE world = `FVector(X, 0, -Y)` (`XodrToWorld`,
  `AkronXodrImporter.cpp:34-42`).
- The XODR generator is **not** part of `tools/universal-city-compiler/` (nothing in the
  compiler writes `.xodr`); the compiler is not currently the producer of this file.

---

## 3. Level spec format — `generated/*_LevelSpec.json`

Produced by `tools/generate-level-spec.py` from a citypack; consumed by
`tools/ue5-import-level-spec.py` (editor automation) and checked for existence by
`PreflightSystem.cpp:248`. Real example: `generated/AkronWorld_LevelSpec.json`.

Top-level keys (observed): `level_name, city_id, origin, world_bounds, spawn_points,
routes, lighting, day_night_cycle, traffic_spawn_volumes, poi_markers, terrain,
water_bodies, vegetation_zones, biome, metadata`.

Summary of shapes (all coordinates already projected to UE world space by
`generate-level-spec.py:25-31`, which mirrors the C++ `GeoToWorld` constants
`111320·cos(lat)` m/deg-lon and `110540` m/deg-lat):

- `level_name` (string), `city_id` (string, must match manifest), `origin {lat, lon}`.
- `world_bounds`: flat `{min_x, min_y, min_z, max_x, max_y, max_z}` numbers.
- `spawn_points[]`: `{id, location:{x,y,z}, rotation:{pitch,yaw,roll}, route_id}`.
- `routes[]`: `{route_id, mode, name, distance_meters, spline_points:[{x,y,z}],
  checkpoints:[{id, location, rotation, radius_meters, type}]}`.
- `lighting`: `{time_of_day, sun_rotation:{pitch,yaw,roll}, sky_atmosphere,
  exposure:{method, compensation}}`; `day_night_cycle`: `{enabled,
  cycle_duration_minutes, start_time}`.
- `traffic_spawn_volumes[]`: `{id, bounds:{min:{x,y,z}, max:{x,y,z}}, density,
  vehicle_types:[string]}`.
- `poi_markers[]`: `{id, name, type, location:{x,y,z}}` (≤ 50 landmarks + ≤ 50 others).
- `terrain`, `water_bodies`, `vegetation_zones`, `biome`: pass-through of optional
  Dialect-B citypack files; `null` when absent.
- `metadata`: `{generated_by: "generate-level-spec.py", spec_version: "2.0.0"}`.

Editor importer dependencies (`ue5-import-level-spec.py`): `spawn_points[].{id, location,
rotation}` (:112-117), `routes[].{route_id, spline_points, checkpoints[].{id, location,
rotation}}` (:122-149), `lighting.{time_of_day, sun_rotation}` (:154-166),
`poi_markers[].{id, type, location}` with `type == "landmark"` filter (:172-182),
`traffic_spawn_volumes[].{id, bounds.min/max.{x,y,z}}` (:187-199).

---

## 4. Stability guarantees

The following are **frozen** for any change that doesn't go through the §6 process:

1. **File layout & naming.** The manifest is located at
   `<citypack_dir>/<city_id>_semantic_manifest.json` (glob `*_semantic_manifest.json`,
   `generate-level-spec.py:92`). All file references in the manifest are filenames
   relative to the pack directory. Preflight additionally hard-codes
   `akron_semantic_manifest.json`, `akron_routes.json`, and
   `generated/AkronWorld_LevelSpec.json` (`PreflightSystem.cpp:247-249`).
2. **Manifest core fields (both dialects):** `city_id`, `version`, `origin.{lat,lon}`,
   `bounds`, and the file references (flat in Dialect A, under `files` in Dialect B).
3. **Coordinate conventions.**
   - All JSON `lat`/`lon` are WGS84 degrees, numbers.
   - Geo→world projection constants are duplicated in C++ and Python and must stay in
     lock-step: `MetersPerDegreeLon = 111320·cos(origin_lat)`,
     `MetersPerDegreeLat = 110540` (`AkronXodrImporter.cpp:13-22`,
     `generate-level-spec.py:15-22`). **Do not "fix" either side independently.**
   - UE world mapping: `X = east`, `Z = -north`, `Y = up = 0` at ground level.
   - `heading` is compass degrees, mapped directly to UE yaw.
4. **Routes / spawns / POIs stay bare top-level JSON arrays** of the shapes in
   §2.3–§2.5 (consumers index into them positionally and by the documented keys).
5. **XODR header contract** (§2.8): `geoReference` with `+lat_0=`/`+lon_0=`,
   `planView/geometry[@x,@y]`, `laneSection/right/lane/width@a`.

---

## 5. Fields the UE5 importer depends on (verified by reading the C++)

| Consumer | File it opens | Fields read |
|---|---|---|
| `CruiseSprintGameMode.cpp:266-296` | manifest | `version` (semver major.minor compat check vs game version; mismatch = warning only) |
| `MainMenuWidget.cpp:125-126, 321-322` | manifest | `version`, `display_name` |
| `StreetFurnitureSpawner.cpp:76-80` | manifest | `origin.lat`, `origin.lon` |
| `StreetFurnitureSpawner.cpp:59, 98-99` | road graph JSON | `intersections[].{lat, lon}` |
| `AkronXodrImporter.cpp:44-162` | XODR | `header/geoReference` (`+lat_0=`, `+lon_0=`), `road@id`, `planView/geometry@{x,y}`, `laneSection/right/lane/width@a`, presence of `laneSection/left` |
| `AkronXodrImporter.cpp:164-227` (fallback when XODR missing; path hard-coded to `akron_road_graph.json`, :50) | road graph JSON | `roads[].{id, width, points[].{lat,lon}}`, and `roads[].oneway` (⚠ real data uses `one_way` — falls back to `false`, §7) |
| `AkronXodrImporter.cpp:229-259` | manifest | `bounds.lat_min`, `bounds.lon_min` (⚠ **absent** in the real manifest — origin silently becomes `0,0`, §7) |
| `AkronXodrImporter.cpp:261-317` | one JSON file **per route** in `RouteDir` (`<pack>/routes/`, `CruiseSprintGameMode.h:81`) | per-file object `{distance_m, points[].{lat,lon}, checkpoints[].{lat,lon}}` (⚠ no such directory exists; real routes are one array file using `distance_meters`, §7) |
| `AkronXodrImporter.cpp:319-351` | manifest (passed as path) | `spawn_points[].{id, lat, lon, heading}` embedded **in the manifest object** (⚠ real manifest stores a filename string, §7) |
| `AkronXodrImporter.cpp:353-386` | manifest (passed as path) | `pois[].{id, name, type, lat, lon}` embedded in the manifest object (⚠ same mismatch) |
| `BuildingMeshGenerator.cpp:64-93` | buildings JSON | `buildings[].{id, type, name, height, area_m2, footprint[].{x,y}}` (⚠ mismatches compiler output, §7) |
| `RuntimeCityLoader.cpp:51-100` (legacy/experimental) | single "citypack JSON" | `city_id`, `routes[].{route_id, spline_points[].{x,y,z}}`, `spawn_points[].{id, location.{x,y,z}}` — this matches the **level-spec** shape, not the manifest shape |
| `PreflightSystem.cpp:247-268` | file existence only | manifest, `akron_routes.json`, `generated/AkronWorld_LevelSpec.json` |
| `tools/ue5-import-level-spec.py` | level spec | see §3 |

### Versioning recommendation

- Keep `manifest.version` as **pack schema+data semver**. The runtime compares
  `major.minor` of `version` against the game build (`CruiseSprintGameMode.cpp:249-262`),
  so: bump **patch** for data-only refreshes, **minor** for additive schema changes,
  **major** for anything not backward compatible (which requires the §6 deprecation
  process and a game-side change).
- Either start honoring `game_version_min`/`game_version_max` in the runtime or delete
  them; today they are dead fields that disagree with the check actually performed
  (Akron pack says `version: 0.2.0` vs game `0.1.0-beta` → permanent compat warning).

---

## 6. Change policy

1. **Additive-only.** New fields, new files, new enum values, and new optional manifest
   keys may be added freely. Consumers use tolerant accessors (`TryGet*Field`,
   `dict.get`), so unknown fields are ignored.
2. **Never silently change:** field names, field types, top-level container types
   (object vs array), units (meters, WGS84 degrees, km/h), coordinate conventions
   (§4.3), filename patterns, or enum spellings of existing values.
3. **Required test/verification updates for any contract-touching change:**
   - Re-validate the manifest against `docs/schemas/citypack-manifest.schema.json`
     (`jsonschema`) — CI-friendly one-liner in §8.
   - Re-run `tools/generate-level-spec.py` and confirm the level spec still imports via
     `tools/ue5-import-level-spec.py` (preview mode) without error.
   - Update this document and the schema in the **same commit** as the producer change.
4. **Deprecation process.** A field/file may be removed only after:
   (a) it is documented as deprecated here for at least one minor version,
   (b) all consumers listed in §5 have shipped a release that tolerates its absence,
   (c) `manifest.version` major is bumped, and
   (d) the schema's `required` list is updated in the same change.
5. **Dual-dialect rule.** Until the dialects are unified (§7), any producer change must
   state explicitly which dialect(s) it affects, and consumers must keep tolerating both.

---

## 7. Known ambiguities / undocumented divergences (hardening backlog)

1. **Two manifest dialects** (§2.1): flat refs + `display_name` (Akron, `0.2.0`) vs
   `files` dict + `name` + hard-coded `version: "2.0.0"` (compiler). No consumer reads
   `version` as a schema discriminator; `generate-level-spec.py:102` branches on `files`.
2. **Missing `akron_buildings.json`** despite the manifest referencing it and claiming
   `building_count: 25022` — manifest stats are not validated against on-disk files.
3. **`bounds.lat_min`/`lon_min` mismatch:** `LoadManifest` reads keys that exist in
   neither dialect (both use `west/south/east/north`); world origin silently `(0,0)`.
4. **`oneway` vs `one_way`:** the importer reads `oneway`; both the compiler
   (`road_network.py:74`) and the real pack write `one_way`. One-way info is lost on the
   JSON fallback path.
5. **Per-route files vs array:** `LoadRouteSplines` globs `<pack>/routes/*.json`
   expecting one object per file with `distance_m`; the pack ships a single
   `akron_routes.json` array with `distance_meters`. The directory does not exist, so
   runtime route loading finds nothing.
6. **Embedded vs referenced spawns/POIs:** `LoadSpawnPoints`/`LoadPOIs` are passed the
   manifest path but expect `spawn_points`/`pois` arrays inside it; the manifest stores
   filename strings. Net effect: runtime spawns/POIs never load.
7. **Buildings shape mismatch:** compiler emits lat/lon footprints, `building_type`,
   `height_meters`; `BuildingMeshGenerator` expects world-space `footprint[].{x,y}`,
   `type`, `height`, `area_m2`.
8. **Checkpoint shape drift:** real pack uses `cp_NNN` ids without
   `distance_from_start_m`; compiler uses `{route_id}_cp_NNN` with it. Real routes add an
   undocumented `scoring` object (`time_bonus`, `clean_driving_bonus`,
   `missed_checkpoint_penalty`) and use difficulty `"beta"`, outside the compiler's
   `easy|medium|hard|extreme` enum.
9. **`node_count` / `bounds` / `origin` top-level asymmetry:** real road graph has
   `node_count` (not emitted by the compiler); compiler emits `bounds` and `origin`
   (absent in the real pack).
10. **Meters-per-degree-lat disagreement:** C++/level-spec use `110540`, the geocoder
    uses `111.0 km/deg` (and `111.32` for lon) — bounds math and world projection use
    different earth models.
11. **`game_version_min`/`game_version_max` are dead fields** (§5).
12. **Silent OSM cache reuse** (`fetch_overpass.py:108`) makes "same inputs" not
    reproducible across agents unless the cache file is deleted.
13. **`gameplay_layer.json` is Akron-only:** no producer, no schema, no consumer found
    in the C++; documented here from the artifact alone.
14. **XODR producer unknown:** no code in `tools/universal-city-compiler/` writes
    `.xodr`; the importer's XODR contract (§2.8) currently has no in-repo generator to
    bind to.
15. **Importer geo defaults:** if `geoReference` is missing, origin silently defaults to
    Akron (`41.08, -81.52`) — wrong for any other city.

---

## 8. Validation quick reference

```bash
python -c "import json, jsonschema; \
s=json.load(open('docs/schemas/citypack-manifest.schema.json')); \
m=json.load(open('citypacks/akron-oh-beta-001/akron_semantic_manifest.json')); \
jsonschema.validate(m, s); print('manifest OK')"
```
