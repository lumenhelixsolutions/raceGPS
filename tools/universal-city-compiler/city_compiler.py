#!/usr/bin/env python3
"""Universal City Compiler — main orchestrator.

Compiles ANY city into a game-ready semantic driving package.
"""

import json
import sys
from datetime import datetime, timezone
from pathlib import Path

from geocode import geocode_city, expand_bounds, bounds_from_center
from fetch_overpass import fetch_and_cache
from road_network import build_road_graph
from route_engine import generate_routes, place_checkpoints
from poi_engine import classify_pois
from building_extractor import extract_buildings, export_buildings
from elevation import generate_heightmap_grid
from biome_engine import classify_biome
from water_extractor import extract_water
from vegetation_scatter import extract_vegetation_zones
from export_bundle import export_bundle


def compile_city(city_query: str, output_dir: Path, radius_km: float = 5.0,
                 detail: str = "standard", route_count: int = 4, seed: int = 42) -> dict:
    """Compile a city from a query string (name or lat,lon) into a citypack.

    Args:
        city_query: City name ("Cleveland, OH") or "lat,lon" ("41.5,-81.7")
        output_dir: Where to write the citypack
        radius_km: Radius around city center to fetch
        detail: "minimal", "standard", or "full"
        route_count: Number of routes per mode
        seed: Random seed for reproducible routes

    Returns:
        dict with manifest and status info
    """
    print("=" * 60)
    print(f"raceGPS Universal City Compiler")
    print(f"Target: {city_query}")
    print("=" * 60)

    # Step 1: Resolve bounds
    if "," in city_query and all(part.strip().replace(".", "").replace("-", "").isdigit() for part in city_query.split(",")[:2]):
        parts = city_query.split(",")
        lat, lon = float(parts[0].strip()), float(parts[1].strip())
        bounds = bounds_from_center(lat, lon, radius_km)
        city_name = f"custom_{lat}_{lon}"
        display_name = f"Custom ({lat}, {lon})"
    else:
        geo = geocode_city(city_query)
        if not geo:
            print(f"[ERROR] Could not geocode: {city_query}")
            return {"success": False, "error": "Geocoding failed"}
        bounds = expand_bounds(geo["bounds"], padding_km=radius_km * 0.3)
        city_name = geo["name"].lower().replace(" ", "_").replace(",", "").replace(".", "")
        display_name = geo["display_name"]

    city_id = f"{city_name}_{radius_km}km"
    citypack_dir = output_dir / city_id
    citypack_dir.mkdir(parents=True, exist_ok=True)

    print(f"[1/8] Bounds: {bounds}")
    print(f"      City ID: {city_id}")

    # Step 2: Fetch OSM
    osm_path = citypack_dir / f"{city_id}_raw.osm"
    try:
        xml = fetch_and_cache(bounds, osm_path, detail)
    except Exception as e:
        print(f"[ERROR] OSM fetch failed: {e}")
        return {"success": False, "error": str(e)}

    # Step 3: Build road graph
    print("[3/8] Building semantic road graph...")
    origin_lat = (bounds["south"] + bounds["north"]) / 2.0
    origin_lon = (bounds["west"] + bounds["east"]) / 2.0
    road_graph = build_road_graph(osm_path, origin_lat, origin_lon)
    print(f"      Roads: {road_graph['road_count']}")
    print(f"      Intersections: {road_graph['intersection_count']}")

    if road_graph["road_count"] < 3:
        print("[WARN] Too few roads. Try a larger radius or different location.")
        return {"success": False, "error": "Too few roads", "roads": road_graph["road_count"]}

    # Step 4: Generate routes
    print("[4/8] Generating routes...")
    routes = generate_routes(road_graph, city_id, mode="all", count=route_count, seed=seed)
    for route in routes:
        route["checkpoints"] = place_checkpoints(route, spacing_meters=350.0)
    print(f"      Routes: {len(routes)}")

    # Step 5: Classify POIs
    print("[5/8] Classifying POIs...")
    pois = classify_pois(osm_path)
    print(f"      POIs: {len(pois)}")

    # Step 6: Extract buildings
    print("[6/10] Extracting buildings...")
    buildings = extract_buildings(osm_path, origin_lat, origin_lon)
    print(f"      Buildings: {len(buildings)}")

    # Step 7: Extract water bodies
    print("[7/10] Extracting water bodies...")
    water = extract_water(osm_path, origin_lat, origin_lon)
    print(f"      Rivers: {water['river_count']}, Lakes: {water['lake_count']}, Coastlines: {water['coastline_count']}")

    # Step 8: Extract vegetation zones
    print("[8/10] Extracting vegetation zones...")
    vegetation = extract_vegetation_zones(osm_path)
    print(f"      Zones: {len(vegetation)}")

    # Step 9: Generate elevation + biome
    print("[9/10] Generating elevation heightmap and biome...")
    heightmap = None
    try:
        heightmap = generate_heightmap_grid(bounds, resolution=32)
        print(f"      Elevation range: {heightmap['min_elevation']}m → {heightmap['max_elevation']}m")
    except Exception as e:
        print(f"      Elevation skipped: {e}")

    biome = classify_biome(origin_lat, origin_lon,
                           avg_elevation=heightmap["min_elevation"] if heightmap else 0.0)
    print(f"      Biome: {biome['biome']} ({biome['temperature_c']}°C)")

    # Step 10: Export bundle
    print("[10/10] Exporting citypack bundle...")
    manifest = export_bundle(citypack_dir, city_id, bounds, routes, pois, buildings, road_graph,
                             water=water, vegetation=vegetation, heightmap=heightmap, biome=biome)
    manifest["generated_at"] = datetime.now(timezone.utc).isoformat()
    manifest["query"] = city_query
    manifest["display_name"] = display_name
    manifest["radius_km"] = radius_km
    manifest["detail"] = detail

    # Rewrite manifest with enriched data
    (citypack_dir / f"{city_id}_semantic_manifest.json").write_text(
        json.dumps(manifest, indent=2), encoding="utf-8"
    )

    # Generate level spec (reuse existing tool with citypack arg)
    print("      Generating UE5 level spec...")
    try:
        import subprocess
        result = subprocess.run(
            [sys.executable, str(Path(__file__).resolve().parents[2] / "tools" / "generate-level-spec.py"),
             "--citypack", str(citypack_dir)],
            capture_output=True, text=True
        )
        if result.returncode == 0:
            print(f"      {result.stdout.strip()}")
        else:
            print(f"      Level spec warning: {result.stderr.strip()}")
    except Exception as e:
        print(f"      Level spec skipped: {e}")

    print("=" * 60)
    print(f"Done. Output: {citypack_dir}")
    print("=" * 60)
    return {"success": True, "manifest": manifest, "citypack_dir": str(citypack_dir)}
