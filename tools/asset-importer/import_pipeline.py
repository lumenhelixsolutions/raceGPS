#!/usr/bin/env python3
"""
raceGPS Asset Import Pipeline
Handles downloading, validating, and organizing open-source 3D assets
following CARLA's strict naming conventions.

Usage:
    python import_pipeline.py --category vehicles --source sketchfab --query "sedan"
    python import_pipeline.py --category materials --source epic --name "M_Master_Vehicle_Paint"
    python import_pipeline.py --validate Content/
"""
import argparse
import json
import os
import re
import shutil
import sys
import zipfile
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]
CONTENT_DIR = PROJECT_ROOT / "apps" / "unreal-akron-beta" / "Content"
ASSET_CATALOG = PROJECT_ROOT / "tools" / "asset-importer" / "asset_catalog.json"

# CARLA-inspired material slot naming convention
VEHICLE_SLOT_MAP = {
    "Bodywork": "M_Master_Vehicle_Paint",
    "Glass_Ext": "M_Master_Vehicle_Glass_Ext",
    "Glass_Int": "M_Master_Vehicle_Glass_Int",
    "Lights": "M_Master_Vehicle_Lights",
    "LicensePlate": "M_Master_Vehicle_LicensePlate",
    "Wheel": "M_Master_Vehicle_Rubber",
    "Chassis": "M_Master_Vehicle_Chrome",
    "Interior": "M_Master_Vehicle_Paint",
}

ROAD_SLOT_MAP = {
    "Asphalt": "M_Master_Road_Asphalt",
    "Marking": "M_Master_Road_Marking",
    "Curb": "M_Master_Road_Curb",
    "Sidewalk": "M_Master_Road_Sidewalk",
}

BUILDING_SLOT_MAP = {
    "Glass": "M_Master_Building_Glass",
    "Concrete": "M_Master_Building_Concrete",
    "Brick": "M_Master_Building_Brick",
    "Industrial": "M_Master_Building_Industrial",
    "Residential": "M_Master_Building_Residential",
}


def validate_naming(path: Path) -> list[str]:
    """Validate that assets follow CARLA naming conventions."""
    errors = []
    for f in path.rglob("*"):
        if f.is_dir():
            continue
        name = f.stem
        # Meshes
        if f.suffix in {".fbx",".obj",".gltf",".glb"}:
            if not re.match(r"^(SM|SK|SKM|UCX|SM_sc)_", name):
                errors.append(f"Mesh {f} missing prefix (SM_/SK_/SKM_/UCX_/SM_sc_)")
        # Textures
        elif f.suffix in {".png",".jpg",".tga",".exr",".hdr"}:
            if not re.match(r"^(T_|ORM_)", name):
                errors.append(f"Texture {f} missing prefix (T_/ORM_)")
        # Materials
        elif f.suffix == ".uasset":
            if not re.match(r"^M_Master_", name):
                errors.append(f"Material {f} missing prefix (M_Master_)")
    return errors


def setup_folder_structure():
    """Create CARLA-style Content folder hierarchy."""
    folders = [
        "Materials/Master",
        "Materials/Instances",
        "Materials/Functions",
        "Meshes/Vehicles/Sedan",
        "Meshes/Vehicles/Sports",
        "Meshes/Vehicles/Truck",
        "Meshes/Vehicles/SUV",
        "Meshes/Environment/Roads",
        "Meshes/Environment/Buildings",
        "Meshes/Environment/Props",
        "Meshes/Environment/Vegetation",
        "Meshes/Environment/Water",
        "Textures/Vehicles",
        "Textures/Environment",
        "Textures/Shared",
        "Blueprints/Vehicles",
        "Blueprints/Environment",
        "Blueprints/VFX",
        "VFX/Weather",
        "VFX/Vehicle",
        "VFX/Environment",
        "Maps",
        "UI",
    ]
    for folder in folders:
        (CONTENT_DIR / folder).mkdir(parents=True, exist_ok=True)
    print(f"Folder structure created under {CONTENT_DIR}")


def write_material_descriptor(name: str, category: str, properties: dict):
    """Write a JSON descriptor for a master material.
    These become .uasset files when UE5 Editor is available."""
    desc_path = CONTENT_DIR / "Materials" / "Master" / f"{name}.json"
    descriptor = {
        "name": name,
        "category": category,
        "master_material": True,
        "properties": properties,
        "source": "raceGPS auto-generated",
        "carla_compatible": True,
    }
    desc_path.write_text(json.dumps(descriptor, indent=2))
    print(f"Material descriptor: {desc_path}")


def generate_master_materials():
    """Generate JSON descriptors for all master materials."""
    materials = [
        ("M_Master_Road_Asphalt", "road", {
            "base_color": [0.15, 0.15, 0.15],
            "roughness": 0.85,
            "metallic": 0.0,
            "normal_strength": 1.0,
            "blend_mode": "Opaque",
            "use_normal_map": True,
        }),
        ("M_Master_Road_Marking", "road", {
            "base_color": [0.9, 0.9, 0.9],
            "roughness": 0.7,
            "metallic": 0.0,
            "blend_mode": "Masked",
            "opacity_mask": True,
        }),
        ("M_Master_Road_Curb", "road", {
            "base_color": [0.6, 0.6, 0.55],
            "roughness": 0.9,
            "metallic": 0.0,
            "blend_mode": "Opaque",
        }),
        ("M_Master_Road_Sidewalk", "road", {
            "base_color": [0.7, 0.7, 0.68],
            "roughness": 0.95,
            "metallic": 0.0,
            "blend_mode": "Opaque",
        }),
        ("M_Master_Building_Glass", "building", {
            "base_color": [0.1, 0.15, 0.2],
            "roughness": 0.05,
            "metallic": 0.0,
            "transmission": 0.9,
            "blend_mode": "Translucent",
        }),
        ("M_Master_Building_Concrete", "building", {
            "base_color": [0.55, 0.55, 0.53],
            "roughness": 0.9,
            "metallic": 0.0,
            "blend_mode": "Opaque",
        }),
        ("M_Master_Building_Brick", "building", {
            "base_color": [0.6, 0.25, 0.15],
            "roughness": 0.85,
            "metallic": 0.0,
            "blend_mode": "Opaque",
        }),
        ("M_Master_Building_Industrial", "building", {
            "base_color": [0.4, 0.42, 0.45],
            "roughness": 0.8,
            "metallic": 0.3,
            "blend_mode": "Opaque",
        }),
        ("M_Master_Building_Residential", "building", {
            "base_color": [0.75, 0.7, 0.6],
            "roughness": 0.75,
            "metallic": 0.0,
            "blend_mode": "Opaque",
        }),
        ("M_Master_Vehicle_Paint", "vehicle", {
            "base_color": [0.5, 0.5, 0.5],
            "roughness": 0.3,
            "metallic": 0.6,
            "clearcoat": 1.0,
            "clearcoat_roughness": 0.1,
            "flake": True,
            "blend_mode": "Opaque",
        }),
        ("M_Master_Vehicle_Glass_Ext", "vehicle", {
            "base_color": [0.05, 0.05, 0.05],
            "roughness": 0.02,
            "metallic": 0.0,
            "transmission": 0.95,
            "blend_mode": "Translucent",
        }),
        ("M_Master_Vehicle_Glass_Int", "vehicle", {
            "base_color": [0.1, 0.1, 0.1],
            "roughness": 0.05,
            "metallic": 0.0,
            "transmission": 0.85,
            "blend_mode": "Translucent",
        }),
        ("M_Master_Vehicle_Lights", "vehicle", {
            "base_color": [1.0, 0.9, 0.6],
            "roughness": 0.2,
            "metallic": 0.0,
            "emissive": 5.0,
            "blend_mode": "Opaque",
        }),
        ("M_Master_Vehicle_Rubber", "vehicle", {
            "base_color": [0.08, 0.08, 0.08],
            "roughness": 0.9,
            "metallic": 0.0,
            "blend_mode": "Opaque",
        }),
        ("M_Master_Vehicle_Chrome", "vehicle", {
            "base_color": [0.8, 0.8, 0.8],
            "roughness": 0.05,
            "metallic": 1.0,
            "blend_mode": "Opaque",
        }),
        ("M_Master_Vehicle_LicensePlate", "vehicle", {
            "base_color": [0.9, 0.9, 0.9],
            "roughness": 0.6,
            "metallic": 0.0,
            "blend_mode": "Opaque",
        }),
        ("M_Master_Vegetation_Grass", "vegetation", {
            "base_color": [0.15, 0.35, 0.08],
            "roughness": 0.8,
            "metallic": 0.0,
            "subsurface": 0.5,
            "blend_mode": "Masked",
            "opacity_mask": True,
        }),
        ("M_Master_Vegetation_Tree", "vegetation", {
            "base_color": [0.1, 0.25, 0.05],
            "roughness": 0.7,
            "metallic": 0.0,
            "subsurface": 0.3,
            "blend_mode": "Masked",
            "opacity_mask": True,
        }),
        ("M_Master_Water_Surface", "water", {
            "base_color": [0.02, 0.08, 0.15],
            "roughness": 0.1,
            "metallic": 0.0,
            "transmission": 0.8,
            "blend_mode": "Translucent",
        }),
        ("M_Master_Decal_Oil", "decal", {
            "base_color": [0.05, 0.05, 0.05],
            "roughness": 0.2,
            "metallic": 0.0,
            "blend_mode": "Translucent",
        }),
        ("M_Master_Decal_RoadWear", "decal", {
            "base_color": [0.4, 0.4, 0.4],
            "roughness": 0.9,
            "metallic": 0.0,
            "blend_mode": "Translucent",
        }),
    ]
    for name, category, props in materials:
        write_material_descriptor(name, category, props)


def main():
    parser = argparse.ArgumentParser(description="raceGPS Asset Import Pipeline")
    sub = parser.add_subparsers(dest="command")

    p_setup = sub.add_parser("setup", help="Create folder structure")
    p_gen = sub.add_parser("generate-materials", help="Generate master material descriptors")
    p_val = sub.add_parser("validate", help="Validate naming conventions")
    p_val.add_argument("path", type=Path, nargs="?", default=CONTENT_DIR)

    args = parser.parse_args()

    if args.command == "setup":
        setup_folder_structure()
    elif args.command == "generate-materials":
        generate_master_materials()
    elif args.command == "validate":
        errors = validate_naming(args.path)
        if errors:
            print(f"Found {len(errors)} naming violations:")
            for e in errors:
                print(f"  - {e}")
            sys.exit(1)
        else:
            print("All assets follow CARLA naming conventions.")
    else:
        parser.print_help()


if __name__ == "__main__":
    main()
