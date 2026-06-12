#!/usr/bin/env python3
"""
UE5 Level Spec Importer

Reads generated/AkronWorld_LevelSpec.json and outputs UE5 Python commands.
When run inside the Unreal Editor Python plugin, it executes the commands directly.
When run outside UE5, it prints the commands for copy-paste.

Usage inside UE5 (Python Console):
    exec(open(r"D:\\projects\\racegps\\tools\\ue5-import-level-spec.py").read())

Usage outside UE5 (preview):
    python tools/ue5-import-level-spec.py
    python tools/ue5-import-level-spec.py --spec generated/Cleveland5.0KmWorld_LevelSpec.json
"""

import argparse
import json
from pathlib import Path

try:
    import unreal
    HAS_UNREAL = True
except ImportError:
    HAS_UNREAL = False


def _find_blueprint(path: str):
    """Load a Blueprint class if available, otherwise return None."""
    if HAS_UNREAL:
        try:
            return unreal.EditorAssetLibrary.load_blueprint_class(path)
        except Exception:
            return None
    return path


def _spawn_actor(actor_class, location: dict, rotation: dict, label: str):
    """Spawn an actor in the level or print the command."""
    if HAS_UNREAL:
        loc = unreal.Vector(location["x"], location["y"], location["z"])
        rot = unreal.Rotator(rotation["pitch"], rotation["yaw"], rotation["roll"])
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(actor_class, loc, rot)
        if actor:
            actor.set_actor_label(label)
        return actor
    print(f"# {label}")
    print(f"actor = unreal.EditorLevelLibrary.spawn_actor_from_class(")
    print(f"    {actor_class},")
    print(f"    unreal.Vector({location['x']}, {location['y']}, {location['z']}),")
    print(f"    unreal.Rotator({rotation['pitch']}, {rotation['yaw']}, {rotation['roll']})")
    print(f")")
    print(f"actor.set_actor_label('{label}')")
    print()
    return None


def _add_spline_points(actor, points: list[dict], label: str):
    """Add spline points to an actor or print the commands."""
    if HAS_UNREAL:
        comp = actor.get_component_by_class(unreal.SplineComponent)
        if not comp:
            comp = actor.add_component_by_class(unreal.SplineComponent)
        if comp:
            comp.clear_spline_points()
            for pt in points:
                comp.add_spline_point(
                    unreal.Vector(pt["x"], pt["y"], pt["z"]),
                    unreal.SplineCoordinateType.WORLD,
                )
        return
    print(f"# Spline for {label}")
    print(f"comp = actor.get_component_by_class(unreal.SplineComponent)")
    print(f"if not comp: comp = actor.add_component_by_class(unreal.SplineComponent)")
    print(f"comp.clear_spline_points()")
    for pt in points:
        print(f"comp.add_spline_point(unreal.Vector({pt['x']}, {pt['y']}, {pt['z']}), unreal.SplineCoordinateType.WORLD)")
    print()


def main() -> int:
    parser = argparse.ArgumentParser(description="Import UE5 level spec JSON into editor")
    parser.add_argument(
        "--spec",
        default="generated/AkronWorld_LevelSpec.json",
        help="Path to level spec JSON (default: generated/AkronWorld_LevelSpec.json)",
    )
    args = parser.parse_args()

    script_dir = Path(__file__).resolve().parent
    spec_path = Path(args.spec)
    if not spec_path.is_absolute():
        spec_path = script_dir.parent / spec_path

    if not spec_path.exists():
        print(f"ERROR: Level spec not found: {spec_path}")
        print("Run first: python tools/generate-level-spec.py")
        return 1

    spec = json.loads(spec_path.read_text(encoding="utf-8"))

    if not HAS_UNREAL:
        print("#" * 70)
        print("# UE5 Python Import Commands - Preview Mode")
        print("# Run inside UE5 Editor Python console to execute.")
        print("#" * 70)
        print()

    # ------------------------------------------------------------------
    # 1. Spawn Points
    # ------------------------------------------------------------------
    for sp in spec.get("spawn_points", []):
        loc = sp["location"]
        rot = sp["rotation"]
        label = f"SP_{sp['id']}"
        actor_class = unreal.PlayerStart if HAS_UNREAL else "unreal.PlayerStart"
        _spawn_actor(actor_class, loc, rot, label)

    # ------------------------------------------------------------------
    # 2. Route Splines
    # ------------------------------------------------------------------
    for route in spec.get("routes", []):
        route_id = route["route_id"]
        points = route.get("spline_points", [])
        if not points:
            continue
        label = f"RouteSpline_{route_id}"
        first = points[0]
        actor_class = unreal.Actor if HAS_UNREAL else "unreal.Actor"
        actor = _spawn_actor(actor_class, first, {"pitch": 0, "yaw": 0, "roll": 0}, label)
        _add_spline_points(actor, points, label)

    # ------------------------------------------------------------------
    # 3. Checkpoint Gates
    # ------------------------------------------------------------------
    bp_path = "/Game/Blueprints/BP_CheckpointGate"
    bp_class = _find_blueprint(bp_path)
    for route in spec.get("routes", []):
        for cp in route.get("checkpoints", []):
            loc = cp["location"]
            rot = cp["rotation"]
            label = f"CP_{cp['id']}_{route['route_id']}"
            if HAS_UNREAL and bp_class:
                actor_class = bp_class
            elif HAS_UNREAL:
                actor_class = unreal.Actor
            else:
                actor_class = f"(unreal.EditorAssetLibrary.load_blueprint_class('{bp_path}') or unreal.Actor)"
            _spawn_actor(actor_class, loc, rot, label)

    # ------------------------------------------------------------------
    # 4. Directional Light Rotation (time-of-day)
    # ------------------------------------------------------------------
    lighting = spec.get("lighting", {})
    sun_rot = lighting.get("sun_rotation", {"pitch": -50, "yaw": 135, "roll": 0})
    if HAS_UNREAL:
        for a in unreal.EditorLevelLibrary.get_all_level_actors():
            if a.get_class().get_name() == "DirectionalLight":
                a.set_actor_rotation(unreal.Rotator(sun_rot["pitch"], sun_rot["yaw"], sun_rot["roll"]))
                break
    else:
        print(f"# Set Directional Light for time-of-day ({lighting.get('time_of_day', 14)}:00)")
        print("for a in unreal.EditorLevelLibrary.get_all_level_actors():")
        print("    if a.get_class().get_name() == 'DirectionalLight':")
        print(f"        a.set_actor_rotation(unreal.Rotator({sun_rot['pitch']}, {sun_rot['yaw']}, {sun_rot['roll']}))")
        print("        break")
        print()

    # ------------------------------------------------------------------
    # 5. Reflection Captures near Landmarks
    # ------------------------------------------------------------------
    poi_markers = spec.get("poi_markers", [])
    landmarks = [p for p in poi_markers if p.get("type") == "landmark"][:10]
    for poi in landmarks:
        loc = poi["location"]
        label = f"ReflCapture_{poi['id']}"
        refl_loc = {"x": loc["x"], "y": loc["y"] + 50, "z": loc["z"]}
        if HAS_UNREAL:
            actor_class = unreal.SphereReflectionCapture if hasattr(unreal, "SphereReflectionCapture") else unreal.Actor
        else:
            actor_class = "getattr(unreal, 'SphereReflectionCapture', unreal.Actor)"
        _spawn_actor(actor_class, refl_loc, {"pitch": 0, "yaw": 0, "roll": 0}, label)

    # ------------------------------------------------------------------
    # 6. Traffic Spawn Volumes (as Box Volumes)
    # ------------------------------------------------------------------
    for vol in spec.get("traffic_spawn_volumes", []):
        bounds = vol["bounds"]
        center = {
            "x": (bounds["min"]["x"] + bounds["max"]["x"]) / 2,
            "y": (bounds["min"]["y"] + bounds["max"]["y"]) / 2,
            "z": (bounds["min"]["z"] + bounds["max"]["z"]) / 2,
        }
        label = f"TrafficVol_{vol['id']}"
        if HAS_UNREAL:
            actor_class = unreal.BoxReflectionCapture if hasattr(unreal, "BoxReflectionCapture") else unreal.TriggerVolume
        else:
            actor_class = "getattr(unreal, 'TriggerVolume', unreal.Actor)"
        _spawn_actor(actor_class, center, {"pitch": 0, "yaw": 0, "roll": 0}, label)

    if not HAS_UNREAL:
        print("#" * 70)
        print("# End of preview.")
        print("# Load AkronWorld.umap in UE5, open Python console, paste above.")
        print("#" * 70)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
