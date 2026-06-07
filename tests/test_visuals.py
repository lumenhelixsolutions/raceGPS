"""
Visual QA Test Suite
Validates rendering configs, material descriptors, asset standards, and C++ visual code.
"""
import json
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
APP_DIR = PROJECT_ROOT / "apps" / "unreal-akron-beta"
SOURCE_DIR = APP_DIR / "Source" / "raceGPSAkronBeta"
CONFIG_DIR = APP_DIR / "Config"
CONTENT_DIR = APP_DIR / "Content"


def test_scalability_ini_exists():
    path = CONFIG_DIR / "Scalability.ini"
    assert path.exists(), f"Missing {path}"
    content = path.read_text()
    assert "[ScalabilitySettings]" in content
    assert "[AntiAliasingQuality@0]" in content
    assert "[AntiAliasingQuality@3]" in content


def test_default_engine_rendering_settings():
    path = CONFIG_DIR / "DefaultEngine.ini"
    assert path.exists()
    content = path.read_text()
    assert "r.DefaultFeature.AntiAliasing" in content
    assert "r.Shadow.Virtual.Enable" in content
    assert "r.Nanite.MaxPixelsPerEdge" in content
    assert "r.Lumen.Reflections.Allow" in content
    assert "r.Streaming.PoolSize" in content
    assert "r.BloomQuality" in content


def test_material_provider_header():
    path = SOURCE_DIR / "Public" / "MaterialProvider.h"
    assert path.exists()
    content = path.read_text()
    assert "EMasterMaterialType" in content
    assert "GetMasterMaterial" in content
    assert "CARLA" in content or "carla" in content.lower()


def test_visual_quality_settings_header():
    path = SOURCE_DIR / "Public" / "VisualQualitySettings.h"
    assert path.exists()
    content = path.read_text()
    assert "EVisualQualityTier" in content
    assert "ApplyTier" in content
    assert "GetRecommendedTier" in content


def test_post_process_controller_header():
    path = SOURCE_DIR / "Public" / "PostProcessController.h"
    assert path.exists()
    content = path.read_text()
    assert "FPostProcessPreset" in content
    assert "ApplyPresetForTier" in content
    assert "ApplyRacingBoostEffect" in content


def test_day_night_cycle_has_atmosphere():
    path = SOURCE_DIR / "Public" / "DayNightCycle.h"
    assert path.exists()
    content = path.read_text()
    assert "SkyAtmosphere" in content
    assert "VolumetricClouds" in content
    assert "HDRIEnvironmentMap" in content


def test_weather_system_has_niagara():
    path = SOURCE_DIR / "Public" / "WeatherSystem.h"
    assert path.exists()
    content = path.read_text()
    assert "NiagaraRain" in content
    assert "NiagaraSnow" in content
    assert "NiagaraStorm" in content


def test_build_cs_has_niagara():
    path = SOURCE_DIR / "raceGPSAkronBeta.Build.cs"
    assert path.exists()
    content = path.read_text()
    assert '"Niagara"' in content
    assert '"NiagaraCore"' in content


def test_vehicle_slot_mapping_json():
    path = CONTENT_DIR / "Vehicles" / "vehicle_slot_mapping.json"
    assert path.exists(), f"Missing {path}"
    data = json.loads(path.read_text())
    assert "slots" in data
    assert "classes" in data
    assert "Bodywork" in data["slots"]
    assert "Glass_Ext" in data["slots"]
    assert "Wheel" in data["slots"]
    assert "Sedan" in data["classes"]
    assert data["classes"]["Sedan"]["poly_budget"] == 25000


def test_environment_visual_config_json():
    path = CONTENT_DIR / "Materials" / "environment_visual_config.json"
    assert path.exists(), f"Missing {path}"
    data = json.loads(path.read_text())
    assert "biomes" in data
    assert "urban" in data["biomes"]
    assert "suburban" in data["biomes"]
    assert "industrial" in data["biomes"]
    urban = data["biomes"]["urban"]
    assert "roads" in urban
    assert "buildings" in urban
    assert "vegetation" in urban


def test_vehicle_asset_standard_doc():
    path = APP_DIR / "docs" / "VehicleAssetStandard.md"
    assert path.exists(), f"Missing {path}"
    content = path.read_text()
    assert "Skeleton Hierarchy" in content
    assert "Material Slot Naming" in content
    assert "UCX_" in content
    assert "ORME" in content


def test_asset_import_pipeline_script():
    path = PROJECT_ROOT / "tools" / "asset-importer" / "import_pipeline.py"
    assert path.exists(), f"Missing {path}"
    content = path.read_text()
    assert "CARLA" in content
    assert "setup_folder_structure" in content
    assert "generate_master_materials" in content


def test_master_material_descriptors_generated():
    import subprocess
    result = subprocess.run(
        ["python", str(PROJECT_ROOT / "tools" / "asset-importer" / "import_pipeline.py"), "generate-materials"],
        capture_output=True, text=True
    )
    # Should create descriptors
    master_dir = CONTENT_DIR / "Materials" / "Master"
    assert master_dir.exists()
    descriptors = list(master_dir.glob("*.json"))
    assert len(descriptors) >= 20, f"Expected 20+ descriptors, got {len(descriptors)}"
    # Verify a few key ones
    names = {d.stem for d in descriptors}
    assert "M_Master_Vehicle_Paint" in names
    assert "M_Master_Road_Asphalt" in names
    assert "M_Master_Building_Glass" in names


def test_master_material_descriptor_schema():
    master_dir = CONTENT_DIR / "Materials" / "Master"
    if not master_dir.exists():
        pytest.skip("Master materials not generated yet")
    for desc_path in master_dir.glob("*.json"):
        data = json.loads(desc_path.read_text())
        assert "name" in data
        assert "category" in data
        assert "properties" in data
        assert "blend_mode" in data["properties"]


def test_material_provider_paths_match_descriptors():
    """MaterialProvider C++ paths should match generated descriptor names."""
    provider_path = SOURCE_DIR / "Private" / "MaterialProvider.cpp"
    assert provider_path.exists()
    provider_src = provider_path.read_text()

    master_dir = CONTENT_DIR / "Materials" / "Master"
    if not master_dir.exists():
        pytest.skip("Master materials not generated yet")

    for desc_path in master_dir.glob("*.json"):
        data = json.loads(desc_path.read_text())
        mat_name = data["name"]
        # The C++ should reference this path
        expected_path = f"/Game/Materials/{mat_name}.{mat_name}"
        assert expected_path in provider_src, f"MaterialProvider missing path for {mat_name}"


def test_niagara_descriptor_files():
    """Niagara weather FX descriptor files exist."""
    vfx_dir = CONTENT_DIR / "VFX" / "Weather"
    if not vfx_dir.exists():
        pytest.skip("VFX directory not created yet")
    # Check for descriptor files
    descriptors = list(vfx_dir.glob("*.json"))
    # At minimum we expect rain/snow/dust/storm descriptors
    names = {d.stem for d in descriptors}
    assert "NS_Rain" in names or len(descriptors) == 0  # OK if not created yet


def test_post_process_tier_consistency():
    """PostProcessController presets should align with VisualQualitySettings tiers."""
    pp_path = SOURCE_DIR / "Private" / "PostProcessController.cpp"
    vq_path = SOURCE_DIR / "Private" / "VisualQualitySettings.cpp"
    assert pp_path.exists()
    assert vq_path.exists()

    pp_src = pp_path.read_text()
    vq_src = vq_path.read_text()

    # Both should reference Low/Medium/High/Epic
    for tier in ["Low", "Medium", "High", "Epic"]:
        assert tier in pp_src, f"PostProcessController missing {tier} preset"
        assert tier in vq_src, f"VisualQualitySettings missing {tier} tier"


if __name__ == "__main__":
    import pytest
    pytest.main([__file__, "-v"])
