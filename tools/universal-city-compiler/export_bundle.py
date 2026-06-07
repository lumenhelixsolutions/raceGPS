#!/usr/bin/env python3
"""Export a generic citypack bundle with manifest, routes, POIs, buildings, and spawn points."""

import json
from pathlib import Path
from typing import Any


def export_bundle(citypack_dir: Path, city_id: str, bounds: dict, routes: list[dict],
                  pois: list[dict], buildings: list[dict], road_graph: dict[str, Any],
                  water: dict[str, Any] | None = None,
                  vegetation: list[dict] | None = None,
                  heightmap: dict[str, Any] | None = None,
                  biome: dict[str, Any] | None = None) -> dict:
    """Export the complete citypack bundle."""

    origin_lat = (bounds["south"] + bounds["north"]) / 2.0
    origin_lon = (bounds["west"] + bounds["east"]) / 2.0

    # Generate spawn points from route starts
    spawn_points = []
    seen = set()
    for route in routes:
        start = route.get("start")
        if not start:
            continue
        key = (round(start["lat"], 5), round(start["lon"], 5))
        if key in seen:
            continue
        seen.add(key)
        spawn_points.append({
            "id": f"spawn_{route['route_id']}",
            "lat": start["lat"],
            "lon": start["lon"],
            "heading": route.get("points", [{}])[0].get("heading", 0) if len(route.get("points", [])) > 1 else 0,
            "route_id": route["route_id"],
        })

    # Write individual files
    (citypack_dir / f"{city_id}_routes.json").write_text(json.dumps(routes, indent=2), encoding="utf-8")
    (citypack_dir / f"{city_id}_pois.json").write_text(json.dumps(pois, indent=2), encoding="utf-8")
    (citypack_dir / f"{city_id}_buildings.json").write_text(json.dumps({"buildings": buildings}, indent=2), encoding="utf-8")
    (citypack_dir / f"{city_id}_spawn_points.json").write_text(json.dumps(spawn_points, indent=2), encoding="utf-8")

    # Write road graph for debugging
    (citypack_dir / f"{city_id}_road_graph.json").write_text(json.dumps(road_graph, indent=2), encoding="utf-8")

    # Write M2 procedural world data
    files = {
        "routes": f"{city_id}_routes.json",
        "pois": f"{city_id}_pois.json",
        "buildings": f"{city_id}_buildings.json",
        "spawn_points": f"{city_id}_spawn_points.json",
        "road_graph": f"{city_id}_road_graph.json",
    }

    if water:
        (citypack_dir / f"{city_id}_water.json").write_text(json.dumps(water, indent=2), encoding="utf-8")
        files["water"] = f"{city_id}_water.json"

    if vegetation:
        (citypack_dir / f"{city_id}_vegetation.json").write_text(json.dumps(vegetation, indent=2), encoding="utf-8")
        files["vegetation"] = f"{city_id}_vegetation.json"

    if heightmap:
        (citypack_dir / f"{city_id}_heightmap.json").write_text(json.dumps(heightmap, indent=2), encoding="utf-8")
        files["heightmap"] = f"{city_id}_heightmap.json"

    if biome:
        (citypack_dir / f"{city_id}_biome.json").write_text(json.dumps(biome, indent=2), encoding="utf-8")
        files["biome"] = f"{city_id}_biome.json"

    manifest = {
        "city_id": city_id,
        "name": city_id.replace("_", " ").title(),
        "version": "2.0.0",
        "bounds": bounds,
        "origin": {"lat": origin_lat, "lon": origin_lon},
        "road_count": road_graph.get("road_count", 0),
        "intersection_count": road_graph.get("intersection_count", 0),
        "route_count": len(routes),
        "spawn_point_count": len(spawn_points),
        "poi_count": len(pois),
        "building_count": len(buildings),
        "water_count": water.get("river_count", 0) + water.get("lake_count", 0) if water else 0,
        "vegetation_zone_count": len(vegetation) if vegetation else 0,
        "has_heightmap": heightmap is not None,
        "biome": biome.get("biome") if biome else None,
        "files": files,
        "generated_by": "universal-city-compiler-v2",
        "generated_at": None,  # caller can set
    }

    (citypack_dir / f"{city_id}_semantic_manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    return manifest
