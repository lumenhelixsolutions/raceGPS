import json
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
PRESET_DIR = PROJECT_ROOT / "apps" / "unreal-akron-beta" / "Content" / "Data" / "VehiclePresets"
GAME_INSTANCE_CPP = PROJECT_ROOT / "apps" / "unreal-akron-beta" / "Source" / "raceGPSAkronBeta" / "Private" / "raceGPSGameInstance.cpp"
GAME_INSTANCE_H = PROJECT_ROOT / "apps" / "unreal-akron-beta" / "Source" / "raceGPSAkronBeta" / "Public" / "raceGPSGameInstance.h"
MAIN_MENU_CPP = PROJECT_ROOT / "apps" / "unreal-akron-beta" / "Source" / "raceGPSAkronBeta" / "Private" / "MainMenuWidget.cpp"
MAIN_MENU_H = PROJECT_ROOT / "apps" / "unreal-akron-beta" / "Source" / "raceGPSAkronBeta" / "Public" / "MainMenuWidget.h"
CRUISE_CPP = PROJECT_ROOT / "apps" / "unreal-akron-beta" / "Source" / "raceGPSAkronBeta" / "Private" / "CruiseSprintGameMode.cpp"
CRUISE_H = PROJECT_ROOT / "apps" / "unreal-akron-beta" / "Source" / "raceGPSAkronBeta" / "Public" / "CruiseSprintGameMode.h"
RUNTIME_CITY_CPP = PROJECT_ROOT / "apps" / "unreal-akron-beta" / "Source" / "raceGPSAkronBeta" / "Private" / "RuntimeCityLoader.cpp"


def test_handling_preset_files_are_canonical_and_complete():
    expected = {"Arcade", "Drift", "Simulation"}
    actual = set()
    files = sorted(PRESET_DIR.glob("*.json"))
    assert files, f"no preset files found in {PRESET_DIR}"

    for path in files:
        data = json.loads(path.read_text(encoding="utf-8"))
        preset_name = data["PresetName"]
        actual.add(preset_name)
        assert path.stem == preset_name, (
            f"preset filename must match PresetName exactly: {path.name} vs {preset_name}"
        )
        assert len(data["Wheels"]) == 4, f"{preset_name} should define exactly 4 wheels"
        assert "Transmission" in data and "Differential" in data

    assert actual == expected, f"expected presets {expected}, found {actual}"


def test_game_instance_persists_vehicle_and_handling_mode_selection():
    header = GAME_INSTANCE_H.read_text(encoding="utf-8")
    source = GAME_INSTANCE_CPP.read_text(encoding="utf-8")

    # Default hero vehicle is the CARLA Dodge Charger 2024 (story T5); the
    # string must match the DisplayName of the "CARLA Charger" preset in
    # CruiseSprintGameMode.cpp::CreateDefaultVehiclePresets.
    assert 'LastSelectedVehicle = TEXT("CARLA Charger")' in header
    assert 'LastSelectedHandlingMode = TEXT("Arcade")' in header
    assert 'Root->SetStringField(TEXT("LastSelectedVehicle"), LastSelectedVehicle);' in source
    assert 'Root->SetStringField(TEXT("LastSelectedHandlingMode"), LastSelectedHandlingMode);' in source
    assert 'Root->TryGetStringField(TEXT("LastSelectedVehicle"), LastSelectedVehicle);' in source
    assert 'Root->TryGetStringField(TEXT("LastSelectedHandlingMode"), LastSelectedHandlingMode);' in source


def test_main_menu_exposes_and_saves_two_axis_garage_selection():
    header = MAIN_MENU_H.read_text(encoding="utf-8")
    source = MAIN_MENU_CPP.read_text(encoding="utf-8")

    assert 'UComboBoxString* VehicleSelector;' in header
    assert 'UComboBoxString* HandlingSelector;' in header
    assert 'UTextBlock* HandlingInfoText;' in header
    assert 'void OnHandlingSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);' in header
    assert 'FString GetSelectedHandlingMode() const;' in header

    assert 'HandlingSelector->ClearOptions();' in source
    assert 'HandlingSelector->AddOption(' in source
    assert 'GI->LastSelectedVehicle = SelectedVehicle->DisplayName;' in source
    assert 'GI->LastSelectedHandlingMode = GetSelectedHandlingMode();' in source


def test_cruise_sprint_game_mode_merges_vehicle_class_and_handling_mode():
    header = CRUISE_H.read_text(encoding="utf-8")
    source = CRUISE_CPP.read_text(encoding="utf-8")

    assert 'TMap<FString, TObjectPtr<class UVehicleTuningData>> HandlingModePresets;' in header
    assert 'void LoadHandlingModePresets();' in header
    assert 'TObjectPtr<class UVehicleTuningData> BuildMergedVehicleTuning' in header
    assert 'LoadHandlingModePresets();' in source
    assert 'BuildMergedVehicleTuning(' in source
    assert 'GI->LastSelectedHandlingMode' in source
    assert 'Content/Data/VehiclePresets' in source
    assert 'const float BehaviorBlend' in source
    assert 'Merged->VehicleMass = BasePreset->VehicleMass;' in source
    assert 'Merged->ChassisWidth = BasePreset->ChassisWidth;' in source
    assert 'Merged->ChassisHeight = BasePreset->ChassisHeight;' in source
    assert 'Merged->Wheels = BasePreset->Wheels;' in source
    assert 'Merged->Transmission.FinalDriveRatio = FMath::Lerp' in source
    assert 'Merged->Differential.FrontRearSplit = FMath::Lerp' in source


def test_runtime_city_loader_is_explicitly_legacy_not_primary_world_path():
    source = RUNTIME_CITY_CPP.read_text(encoding="utf-8")
    assert 'legacy experimental loader' in source.lower()
