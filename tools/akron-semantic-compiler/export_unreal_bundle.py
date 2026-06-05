#!/usr/bin/env python3
"""Export the final semantic bundle + manifest for Unreal consumption."""

from pathlib import Path
from typing import Any


def export_bundle(
    citypack_dir: Path,
    bounds: dict[str, float],
    routes: list[dict[str, Any]],
    pois: list[dict[str, Any]],
) -> dict[str, Any]:
    """Build the semantic manifest and write spawn points + gameplay layer."""

    # Spawn points: start of each route
    spawn_points = []
    for route in routes:
        spawn_points.append({
            "id": f"spawn_{route['route_id']}",
            "lat": route["start"]["lat"],
            "lon": route["start"]["lon"],
            "heading": route.get("checkpoints", [{}])[0].get("heading", 0) if route.get("checkpoints") else 0,
            "route_id": route["route_id"],
        })

    with open(citypack_dir / "akron_spawn_points.json", "w") as f:
        import json
        json.dump(spawn_points, f, indent=2)

    # Gameplay layer: zones, speed limits, objective areas
    gameplay_layer = {
        "speed_zones": [
            {"type": "highway", "highways": ["motorway", "trunk"], "speed_kmh": 90},
            {"type": "arterial", "highways": ["primary", "secondary"], "speed_kmh": 65},
            {"type": "local", "highways": ["tertiary", "residential", "unclassified"], "speed_kmh": 45},
            {"type": "service", "highways": ["service"], "speed_kmh": 25},
        ],
        "pickup_zones": [],
        "objective_zones": [],
        "challenge_zones": [],
    }

    with open(citypack_dir / "akron_gameplay_layer.json", "w") as f:
        json.dump(gameplay_layer, f, indent=2)

    # Manifest
    manifest = {
        "city_id": "akron-oh-beta-001",
        "display_name": "Akron, Ohio",
        "version": "0.1.0",
        "bounds": bounds,
        "opendrive_file": "akron.xodr",
        "routes": "akron_routes.json",
        "road_graph": "akron_road_graph.json",
        "spawn_points": "akron_spawn_points.json",
        "pois": "akron_pois.json",
        "gameplay_layer": "akron_gameplay_layer.json",
        "route_count": len(routes),
        "poi_count": len(pois),
        "spawn_point_count": len(spawn_points),
    }

    return manifest
