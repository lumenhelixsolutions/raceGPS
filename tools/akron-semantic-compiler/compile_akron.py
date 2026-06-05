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
from building_extractor import extract_buildings, export_buildings
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
    graph_path = CITYPACK_DIR / "akron_road_graph.json"

    if osm_path.exists():
        print(f"[1/7] Using cached OSM: {osm_path}")
        with open(osm_path, "r", encoding="utf-8") as f:
            osm_xml = f.read()
    elif graph_path.exists():
        print(f"[1/7] Using cached road graph (OSM fetch skipped): {graph_path}")
        osm_xml = None
    else:
        print("[1/7] Fetching Akron OSM data...")
        try:
            osm_xml = fetch_osm_for_bounds(**AKRON_BOUNDS)
            with open(osm_path, "w", encoding="utf-8") as f:
                f.write(osm_xml)
            print(f"      Saved: {osm_path}")
        except Exception as e:
            print(f"[1/7] OSM fetch failed: {e}")
            print("      No cached data available. Aborting.")
            return 1

    # Step 2: Convert road graph → OpenDRIVE (pure Python, no CARLA)
    xodr_path = CITYPACK_DIR / "akron.xodr"
    if not xodr_path.exists():
        print("[2/7] Generating OpenDRIVE from road graph...")
        from osm_to_xodr import generate_xodr
        # If OSM exists, build from OSM; otherwise reuse cached road_graph.json
        if osm_path.exists():
            from osm_to_xodr import convert_osm_to_xodr
            xodr = convert_osm_to_xodr(osm_path)
            with open(xodr_path, "w", encoding="utf-8") as f:
                f.write(xodr)
        else:
            graph_path = CITYPACK_DIR / "akron_road_graph.json"
            with open(graph_path, "r", encoding="utf-8") as f:
                road_graph = json.load(f)
            generate_xodr(road_graph, xodr_path)
        print(f"      Saved: {xodr_path}")
    else:
        print(f"[2/7] Using cached OpenDRIVE: {xodr_path}")

    # Step 3: Build or reuse semantic road graph
    graph_path = CITYPACK_DIR / "akron_road_graph.json"
    if osm_path.exists():
        print("[3/7] Building semantic road graph from OSM...")
        road_graph = build_road_graph(osm_path)
        with open(graph_path, "w") as f:
            json.dump(road_graph, f, indent=2)
    else:
        print(f"[3/7] Reusing cached road graph: {graph_path}")
        with open(graph_path, "r") as f:
            road_graph = json.load(f)
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

    # Step 6: Extract buildings
    buildings_path = CITYPACK_DIR / "akron_buildings.json"
    if osm_path.exists():
        print("[6/8] Extracting building footprints from OSM...")
        origin_lat = (AKRON_BOUNDS["south"] + AKRON_BOUNDS["north"]) / 2.0
        origin_lon = (AKRON_BOUNDS["west"] + AKRON_BOUNDS["east"]) / 2.0
        buildings = extract_buildings(osm_path, origin_lat, origin_lon)
        export_buildings(buildings, buildings_path)
    elif buildings_path.exists():
        print(f"[6/8] Reusing cached buildings: {buildings_path}")
        with open(buildings_path, "r") as f:
            buildings = json.load(f).get("buildings", [])
    else:
        print("[6/8] No OSM or cached buildings available. Skipping.")
        buildings = []
    print(f"      Buildings: {len(buildings)}")

    # Step 7: Classify or reuse POIs
    pois_path = CITYPACK_DIR / "akron_pois.json"
    if osm_path.exists():
        print("[7/8] Classifying POIs from OSM...")
        pois = classify_pois(osm_path)
        with open(pois_path, "w") as f:
            json.dump(pois, f, indent=2)
    else:
        print(f"[7/8] Reusing cached POIs: {pois_path}")
        with open(pois_path, "r") as f:
            pois = json.load(f)
    print(f"      POIs: {len(pois)}")

    # Step 8: Export bundle
    print("[8/8] Exporting Unreal bundle...")
    manifest = export_bundle(CITYPACK_DIR, AKRON_BOUNDS, routes, pois, buildings)
    with open(CITYPACK_DIR / "akron_semantic_manifest.json", "w") as f:
        json.dump(manifest, f, indent=2)
    print(f"      Manifest: {CITYPACK_DIR / 'akron_semantic_manifest.json'}")

    print("=" * 60)
    print("Done. Output:", CITYPACK_DIR)
    print("=" * 60)
    return 0


if __name__ == "__main__":
    sys.exit(main())
