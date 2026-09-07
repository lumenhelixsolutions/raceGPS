from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "apps/unreal-akron-beta/Source/raceGPSAkronBeta"

def test_hellcat_enum_and_mesh_path():
    types = (SRC / "Public/ClevelandShowcaseTypes.h").read_text(encoding="utf-8")
    assert "EVehicleLook" in types
    assert "Hellcat" in types
    cpp = (SRC / "Private/ChaosVehiclePawn.cpp").read_text(encoding="utf-8")
    assert "SK_DodgeCharger2024" in cpp
    assert "ApplyHellcatTune" in cpp
    grid = (SRC / "Private/RaceGridManager.cpp").read_text(encoding="utf-8")
    assert "EVehicleLook::Hellcat" in grid

def test_no_carla_server_required():
    docs = (ROOT / "docs/CLEVELAND_VEHICLES.md").read_text(encoding="utf-8")
    assert "not a running CARLA server" in docs.lower() or "not a running CARLA" in docs


def test_paint_param_names_and_clearcoat():
    cpp = (SRC / "Private/ChaosVehiclePawn.cpp").read_text(encoding="utf-8")
    assert "Base_color" in cpp
    assert "Base_color_flakes" in cpp
    assert "SetScalarParameterValue" in cpp
    assert "ClearCoat" in cpp
    assert "Metallic" in cpp


def test_paint_stub_pngs_exist():
    tex = ROOT / "apps/unreal-akron-beta/Content/Carla/Static/GenericMaterials/00_MastersOpt/Textures/Source"
    for name in ("T_flakes_d.png", "T_flakes_n.png", "T_dirt_01.png"):
        path = tex / name
        assert path.is_file() and path.stat().st_size > 1000, path
    script = ROOT / "apps/unreal-akron-beta/Content/Python/import_carpaint_stub_textures.py"
    assert script.is_file()
    docs = (ROOT / "docs/CLEVELAND_VEHICLE_PAINT_FIX.md").read_text(encoding="utf-8")
    assert "T_flakes_d" in docs and "T_dirt_01" in docs
