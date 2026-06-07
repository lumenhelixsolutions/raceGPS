#!/usr/bin/env python3
"""
garageGPS — Custom Car Kit + Garage Design Editor CLI
Usage:
    python garagegps.py validate <build.json>
    python garagegps.py build <build.json>
    python garagegps.py export-unreal <build.json>
    python garagegps.py export-carla <build.json>
"""
import argparse
import json
import sys
from pathlib import Path

import jsonschema

SCHEMA_DIR = Path(__file__).resolve().parent.parent.parent / "packages" / "car-kit-schema"


def load_schema(name: str) -> dict:
    path = SCHEMA_DIR / f"{name}.schema.json"
    with open(path) as f:
        return json.load(f)


def validate_build(build_path: Path) -> list[str]:
    """Validate a car build JSON against the schema and custom rules."""
    errors = []
    try:
        with open(build_path) as f:
            build = json.load(f)
    except json.JSONDecodeError as e:
        return [f"Invalid JSON: {e}"]

    schema = load_schema("car-build")
    try:
        jsonschema.validate(build, schema)
    except jsonschema.ValidationError as e:
        errors.append(f"Schema error: {e.message} at {list(e.path)}")

    # Custom validation rules
    if "visual_parts" in build:
        parts = build["visual_parts"]
        if "wheels" not in parts:
            errors.append("Missing required visual part: wheels")

    if "materials" in build:
        mats = build["materials"]
        if "body" not in mats:
            errors.append("Missing required material: body")
        else:
            body = mats["body"]
            if body.get("metallic", 0) > 0.9 and body.get("roughness", 1) < 0.1:
                errors.append("Warning: extreme metallic + low roughness may cause shader artifacts")

    return errors


def build_car(build_path: Path) -> dict:
    """Assemble a validated car build manifest."""
    errors = validate_build(build_path)
    if errors:
        print("Validation failed:")
        for e in errors:
            print(f"  - {e}")
        sys.exit(1)

    with open(build_path) as f:
        build = json.load(f)

    # Enrich with computed fields
    build["_computed"] = {
        "total_parts": len(build.get("visual_parts", {})),
        "has_underglow": build.get("materials", {}).get("underglow", {}).get("enabled", False),
        "is_pursuit": build.get("kit") == "pursuit",
    }
    return build


def export_unreal_manifest(build: dict, out_path: Path):
    """Generate Unreal import manifest."""
    manifest = {
        "version": "0.1.0",
        "car_id": build["car_id"],
        "display_name": build["display_name"],
        "content_paths": {
            "skeletal_mesh": f"/Game/RaceGPS/Vehicles/Base/SK_{build['car_id']}",
            "materials": [f"/Game/RaceGPS/Vehicles/Materials/{k}" for k in build.get("materials", {})],
            "parts": [f"/Game/RaceGPS/Vehicles/Parts/{v}" for v in build.get("visual_parts", {}).values()],
        },
        "blueprint_class": f"/Game/RaceGPS/Vehicles/Blueprints/BP_{build['car_id']}",
        "physics_preset": build.get("physics_preset", "arcade_street"),
    }
    out_path.write_text(json.dumps(manifest, indent=2))
    print(f"Unreal manifest: {out_path}")


def export_carla_manifest(build: dict, out_path: Path):
    """Generate CARLA compatibility manifest."""
    manifest = {
        "make": "raceGPS",
        "model": build["display_name"].replace(" ", ""),
        "generation": 1,
        "number_of_wheels": 4,
        "blueprint_class": f"/Game/Carla/Blueprints/Vehicles/RaceGPS/BP_{build['car_id']}",
        "skeletal_mesh": f"/Game/RaceGPS/Vehicles/Base/SK_{build['car_id']}",
        "wheel_blueprints": {
            "front_left": "BP_RGPS_Wheel_Front",
            "front_right": "BP_RGPS_Wheel_Front",
            "rear_left": "BP_RGPS_Wheel_Rear",
            "rear_right": "BP_RGPS_Wheel_Rear",
        },
    }
    out_path.write_text(json.dumps(manifest, indent=2))
    print(f"CARLA manifest: {out_path}")


def main():
    parser = argparse.ArgumentParser(description="garageGPS CLI")
    sub = parser.add_subparsers(dest="command")

    p_val = sub.add_parser("validate", help="Validate car build JSON")
    p_val.add_argument("build_json", type=Path)

    p_build = sub.add_parser("build", help="Build and validate car manifest")
    p_build.add_argument("build_json", type=Path)

    p_ue = sub.add_parser("export-unreal", help="Export Unreal import manifest")
    p_ue.add_argument("build_json", type=Path)
    p_ue.add_argument("--output", type=Path, default=None)

    p_carla = sub.add_parser("export-carla", help="Export CARLA compatibility manifest")
    p_carla.add_argument("build_json", type=Path)
    p_carla.add_argument("--output", type=Path, default=None)

    args = parser.parse_args()

    if args.command == "validate":
        errors = validate_build(args.build_json)
        if errors:
            print(f"Found {len(errors)} issues:")
            for e in errors:
                print(f"  - {e}")
            sys.exit(1)
        else:
            print(f"{args.build_json} is valid.")

    elif args.command == "build":
        build = build_car(args.build_json)
        out = args.build_json.with_suffix(".built.json")
        out.write_text(json.dumps(build, indent=2))
        print(f"Built manifest: {out}")

    elif args.command == "export-unreal":
        build = build_car(args.build_json)
        out = args.output or args.build_json.with_suffix(".unreal.json")
        export_unreal_manifest(build, out)

    elif args.command == "export-carla":
        build = build_car(args.build_json)
        out = args.output or args.build_json.with_suffix(".carla.json")
        export_carla_manifest(build, out)

    else:
        parser.print_help()


if __name__ == "__main__":
    main()
