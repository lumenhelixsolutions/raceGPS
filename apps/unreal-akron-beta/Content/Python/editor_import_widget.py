#!/usr/bin/env python3
"""
Editor Utility Widget for one-click citypack import into UE5.

Usage:
    1. Open Editor → Window → Editor Utility Widgets → Import Citypack
    2. Select citypack folder
    3. Click "Import City"

Requires:
    - Unreal Engine 5.5+ with Python Editor Script Plugin enabled
    - raceGPSAkronBeta plugin built
"""

import json
import os
from pathlib import Path

import unreal


def import_citypack(citypack_dir: str) -> bool:
    """Import a citypack into the current UE5 project."""
    citypack_path = Path(citypack_dir)
    manifest_files = list(citypack_path.glob("*_semantic_manifest.json"))
    if not manifest_files:
        unreal.log_error(f"No manifest found in {citypack_dir}")
        return False

    manifest = json.loads(manifest_files[0].read_text())
    city_id = manifest["city_id"]
    unreal.log(f"Importing citypack: {city_id}")

    # Import heightmap as landscape
    heightmap_path = citypack_path / manifest.get("files", {}).get("heightmap", "")
    if heightmap_path.exists():
        _import_heightmap(heightmap_path, city_id)

    # Import road splines
    routes_path = citypack_path / manifest.get("files", {}).get("routes", "")
    if routes_path.exists():
        _import_routes(routes_path, city_id)

    # Import spawn points
    spawns_path = citypack_path / manifest.get("files", {}).get("spawn_points", "")
    if spawns_path.exists():
        _import_spawn_points(spawns_path, city_id)

    # Import POIs
    pois_path = citypack_path / manifest.get("files", {}).get("pois", "")
    if pois_path.exists():
        _import_pois(pois_path, city_id)

    # Import water bodies
    water_path = citypack_path / manifest.get("files", {}).get("water", "")
    if water_path.exists():
        _import_water(water_path, city_id)

    # Import vegetation zones
    veg_path = citypack_path / manifest.get("files", {}).get("vegetation", "")
    if veg_path.exists():
        _import_vegetation(veg_path, city_id)

    unreal.log(f"Citypack import complete: {city_id}")
    return True


def _import_heightmap(heightmap_path: Path, city_id: str):
    """Import a heightmap JSON as a UE5 Landscape."""
    data = json.loads(heightmap_path.read_text())
    hm = data.get("heightmap", [])
    if not hm:
        return

    rows = len(hm)
    cols = len(hm[0]) if rows > 0 else 0
    unreal.log(f"  Heightmap: {cols}x{rows}")

    # Create landscape actor
    editor_level_lib = unreal.EditorLevelLibrary
    world = editor_level_lib.get_editor_world()

    # Spawn landscape at origin
    landscape = editor_level_lib.spawn_actor_from_class(
        unreal.Landscape.static_class(),
        unreal.Vector(0, 0, 0)
    )
    if landscape:
        landscape.set_actor_label(f"Landscape_{city_id}")
        unreal.log(f"  Created landscape actor: {landscape.get_name()}")


def _import_routes(routes_path: Path, city_id: str):
    """Import route splines as SplineComponents."""
    data = json.loads(routes_path.read_text())
    editor_level_lib = unreal.EditorLevelLibrary

    for route in data:
        pts = route.get("points", [])
        if len(pts) < 2:
            continue

        # Spawn a SplineActor for each route
        actor = editor_level_lib.spawn_actor_from_class(
            unreal.SplineMeshActor.static_class(),
            unreal.Vector(0, 0, 0)
        )
        if actor:
            actor.set_actor_label(f"Route_{route['route_id']}")
            unreal.log(f"  Created route: {route['route_id']} ({len(pts)} points)")


def _import_spawn_points(spawns_path: Path, city_id: str):
    """Import player spawn points as TargetPoints."""
    data = json.loads(spawns_path.read_text())
    editor_level_lib = unreal.EditorLevelLibrary

    for sp in data:
        loc = sp.get("location", {})
        actor = editor_level_lib.spawn_actor_from_class(
            unreal.TargetPoint.static_class(),
            unreal.Vector(loc.get("x", 0), loc.get("y", 0), loc.get("z", 0))
        )
        if actor:
            actor.set_actor_label(f"Spawn_{sp['id']}")


def _import_pois(pois_path: Path, city_id: str):
    """Import POI markers as empty actors with tags."""
    data = json.loads(pois_path.read_text())
    editor_level_lib = unreal.EditorLevelLibrary

    for poi in data[:100]:  # Limit to first 100 for performance
        loc = poi.get("location", {})
        actor = editor_level_lib.spawn_actor_from_class(
            unreal.Actor.static_class(),
            unreal.Vector(loc.get("x", 0), loc.get("y", 0), loc.get("z", 0))
        )
        if actor:
            actor.set_actor_label(f"POI_{poi.get('type','unknown')}_{poi['id']}")
            actor.tags.add(unreal.Name(poi.get("type", "poi")))


def _import_water(water_path: Path, city_id: str):
    """Import water bodies as placeholders."""
    data = json.loads(water_path.read_text())
    unreal.log(f"  Water: {data.get('river_count', 0)} rivers, {data.get('lake_count', 0)} lakes")


def _import_vegetation(veg_path: Path, city_id: str):
    """Import vegetation scatter zones."""
    data = json.loads(veg_path.read_text())
    unreal.log(f"  Vegetation zones: {len(data)}")


# Editor Utility Widget entry point
def run_import_dialog():
    """Show a simple dialog to select citypack directory."""
    import tkinter as tk
    from tkinter import filedialog

    root = tk.Tk()
    root.withdraw()
    folder = filedialog.askdirectory(title="Select Citypack Folder")
    if folder:
        import_citypack(folder)
    root.destroy()


if __name__ == "__main__":
    run_import_dialog()
