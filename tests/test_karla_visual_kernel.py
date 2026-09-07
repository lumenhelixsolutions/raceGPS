import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from karla_visual_kernel import build_skyline, circuit_south_edge, build_airport_hangars  # noqa: E402


def test_keeps_skyline_south_of_circuit_and_tallest():
    racing = {
        "samples": [
            {"lat": 41.52, "lon": -81.68},
            {"lat": 41.515, "lon": -81.69},
        ]
    }
    south = circuit_south_edge(racing)
    buildings = [
        {"name": "north hangar", "lat": 41.53, "lon": -81.68, "height_m": 200},  # north of circuit — drop
        {"name": "short shack", "lat": 41.50, "lon": -81.69, "height_m": 10},  # too short
        {"name": "Key-ish", "lat": 41.501, "lon": -81.694, "height_m": 280},
        {"name": "mid", "lat": 41.499, "lon": -81.69, "levels": 40},
    ]
    landmarks = [{"name": "Terminal Tower", "lat": 41.49847, "lon": -81.69395, "height_m": 235,
                  "width_m": 48, "depth_m": 48, "yaw_deg": 0, "material": "Building_Concrete"}]
    sky = build_skyline(buildings, south, landmarks=landmarks, max_volumes=10)
    names = [b["name"] for b in sky["buildings"]]
    assert "Terminal Tower" in names
    assert "Key-ish" in names
    assert "north hangar" not in names
    assert "short shack" not in names
    key = next(b for b in sky["buildings"] if b["name"] == "Key-ish")
    assert key["material"] == "Building_Glass"
    assert sky["generator"] == "karla_visual_kernel"


def test_script_exists():
    assert (ROOT / "scripts" / "karla_visual_kernel.py").is_file()


def test_airport_hangars_do_not_touch_skyline():
    buildings = [
        {"name": "Signature hangar", "lat": 41.5130, "lon": -81.6840, "height_m": 11,
         "width_m": 40, "depth_m": 24, "building": "hangar"},
        {"name": "downtown tower", "lat": 41.4985, "lon": -81.6940, "height_m": 235},
        {"name": "far hangar", "lat": 41.40, "lon": -81.50, "height_m": 10, "aeroway": "hangar"},
    ]
    hang = build_airport_hangars(buildings, origin_lat=41.51722, origin_lon=-81.68306, radius_m=900)
    names = {b["name"] for b in hang["airport_boxes"]}
    assert "Signature hangar" in names
    assert "downtown tower" not in names
    assert "far hangar" not in names
    box = hang["airport_boxes"][0]
    assert box["mesh_path"].startswith("/Game/Carla/")
    assert box["complement_hism"] is True
    assert hang["carla_required"] is False
    assert "SM_Hangar" in hang["hangar_mesh_candidates"][0]
