"""
garageGPS Validation Tests
"""
import json
import subprocess
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
GARAGEGPS = PROJECT_ROOT / "tools" / "garagegps" / "garagegps.py"
SCHEMA_DIR = PROJECT_ROOT / "packages" / "car-kit-schema"
BUILD_DIR = PROJECT_ROOT / "assets" / "car-kit" / "builds"


def test_schema_valid_json():
    path = SCHEMA_DIR / "car-build.schema.json"
    assert path.exists()
    data = json.loads(path.read_text())
    assert data["title"] == "raceGPS Car Build Schema"
    assert "properties" in data


def test_sample_build_valid():
    path = BUILD_DIR / "akron_street_coupe_001.json"
    assert path.exists()
    result = subprocess.run(
        [sys.executable, str(GARAGEGPS), "validate", str(path)],
        capture_output=True, text=True
    )
    assert result.returncode == 0, f"Validation failed: {result.stdout}{result.stderr}"
    assert "is valid" in result.stdout


def test_build_command():
    path = BUILD_DIR / "akron_street_coupe_001.json"
    result = subprocess.run(
        [sys.executable, str(GARAGEGPS), "build", str(path)],
        capture_output=True, text=True
    )
    assert result.returncode == 0
    assert "Built manifest" in result.stdout
    built_path = path.with_suffix(".built.json")
    assert built_path.exists()
    data = json.loads(built_path.read_text())
    assert "_computed" in data


def test_export_unreal():
    path = BUILD_DIR / "akron_street_coupe_001.json"
    result = subprocess.run(
        [sys.executable, str(GARAGEGPS), "export-unreal", str(path)],
        capture_output=True, text=True
    )
    assert result.returncode == 0
    assert "Unreal manifest" in result.stdout
    manifest_path = path.with_suffix(".unreal.json")
    assert manifest_path.exists()
    data = json.loads(manifest_path.read_text())
    assert "content_paths" in data
    assert "blueprint_class" in data


def test_export_carla():
    path = BUILD_DIR / "akron_street_coupe_001.json"
    result = subprocess.run(
        [sys.executable, str(GARAGEGPS), "export-carla", str(path)],
        capture_output=True, text=True
    )
    assert result.returncode == 0
    assert "CARLA manifest" in result.stdout
    manifest_path = path.with_suffix(".carla.json")
    assert manifest_path.exists()
    data = json.loads(manifest_path.read_text())
    assert "wheel_blueprints" in data
    assert data["make"] == "raceGPS"


def test_invalid_build_fails():
    bad_build = {
        "schema_version": "0.1.0",
        "car_id": "invalid-id",
        "display_name": "Bad Car",
        "base_chassis": "rgps_chassis_4w_sport",
        "visual_parts": {},
        "materials": {},
        "physics_preset": "arcade_sprint"
    }
    bad_path = BUILD_DIR / "_test_invalid.json"
    bad_path.write_text(json.dumps(bad_build))
    result = subprocess.run(
        [sys.executable, str(GARAGEGPS), "validate", str(bad_path)],
        capture_output=True, text=True
    )
    bad_path.unlink()
    assert result.returncode != 0


if __name__ == "__main__":
    import pytest
    sys.exit(pytest.main([__file__, "-v"]))
