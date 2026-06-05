#!/usr/bin/env python3
"""
Akron Semantic Compiler — main orchestrator.
Compiles Akron, OH into a game-ready semantic driving package.
"""

import json
import os
import sys
from pathlib import Path

# Add project root to path
PROJECT_ROOT = Path(__file__).resolve().parents[2]
CITYPACK_DIR = PROJECT_ROOT / "citypacks" / "akron-oh-beta-001"
CITYPACK_DIR.mkdir(parents=True, exist_ok=True)

from fetch_osm import fetch_osm_for_bounds
from road_graph import build_road_graph
from route_generator import generate_cruise_sprint
from checkpoint_generator import place_checkpoints
from poi_classifier import classify_pois
from export_unreal_bundle import export_bundle

AKRON_BOUNDS = {
    "west": -81.62,
    "south": 40.98,
    "east": -81.43,
    "north": 41.16,
}


def main() -> int:
    print("=" * 60)
    print("raceGPS Akron Semantic Compiler")
    print("=" * 60)

    # Step 1: Load or fetch OSM data
    osm_path = CITYPACK_DIR / "akron_raw.osm"
    if osm_path.exists():
        print(f"[1/7] Using cached OSM: {osm_path}")
        with open(osm_path, "r", encoding="utf-8") as f:
            osm_xml = f.read()
    else:
        print("[1/7] Fetching Akron OSM data...")
        osm_xml = fetch_osm_for_bounds(**AKRON_BOUNDS)
        with open(osm_path, "w", encoding="utf-8") as f:
            f.write(osm_xml)
        print(f"      Saved: {osm_path}")

    # Step 2: Convert OSM → OpenDRIVE (optional, requires CARLA)
    xodr_path = CITYPACK_DIR / "akron.xodr"
    if not xodr_path.exists():
        try:
            from osm_to_xodr import convert_osm_to_xodr
            print("[2/7] Converting OSM → OpenDRIVE...")
            xodr = convert_osm_to_xodr(osm_path)
            with open(xodr_path, "w", encoding="utf-8") as f:
                f.write(xodr)
            print(f"      Saved: {xodr_path}")
        except ImportError:
            print("[2/7] SKIPPED: CARLA not installed. Install carla Python API for .xodr output.")
    else:
        print(f"[2/7] Using cached OpenDRIVE: {xodr_path}")

    # Step 3: Build semantic road graph
    print("[3/7] Building semantic road graph...")
    road_graph = build_road_graph(osm_path)
    with open(CITYPACK_DIR / "akron_road_graph.json", "w") as f:
        json.dump(road_graph, f, indent=2)
    print(f"      Roads: {len(road_graph.get('roads', []))}")
    print(f"      Intersections: {len(road_graph.get('intersections', []))}")

    # Step 4: Generate routes
    print("[4/7] Generating Cruise Sprint routes...")
    routes = generate_cruise_sprint(road_graph)
    with open(CITYPACK_DIR / "akron_routes.json", "w") as f:
        json.dump(routes, f, indent=2)
    print(f"      Routes: {len(routes)}")

    # Step 5: Place checkpoints
    print("[5/7] Placing checkpoint gates...")
    for route in routes:
        route["checkpoints"] = place_checkpoints(route, road_graph)
    with open(CITYPACK_DIR / "akron_routes.json", "w") as f:
        json.dump(routes, f, indent=2)
    print(f"      Total checkpoints: {sum(len(r['checkpoints']) for r in routes)}")

    # Step 6: Classify POIs
    print("[6/7] Classifying POIs...")
    pois = classify_pois(osm_path)
    with open(CITYPACK_DIR / "akron_pois.json", "w") as f:
        json.dump(pois, f, indent=2)
    print(f"      POIs: {len(pois)}")

    # Step 7: Export bundle
    print("[7/7] Exporting Unreal bundle...")
    manifest = export_bundle(CITYPACK_DIR, AKRON_BOUNDS, routes, pois)
    with open(CITYPACK_DIR / "akron_semantic_manifest.json", "w") as f:
        json.dump(manifest, f, indent=2)
    print(f"      Manifest: {CITYPACK_DIR / 'akron_semantic_manifest.json'}")

    print("=" * 60)
    print("Done. Output:", CITYPACK_DIR)
    print("=" * 60)
    return 0


if __name__ == "__main__":
    sys.exit(main())
