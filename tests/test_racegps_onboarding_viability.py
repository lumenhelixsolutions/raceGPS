from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
PRE_H = PROJECT_ROOT / "apps" / "unreal-akron-beta" / "Source" / "raceGPSAkronBeta" / "Public" / "PreflightSystem.h"
PRE_CPP = PROJECT_ROOT / "apps" / "unreal-akron-beta" / "Source" / "raceGPSAkronBeta" / "Private" / "PreflightSystem.cpp"
IMPORTER_CPP = PROJECT_ROOT / "apps" / "unreal-akron-beta" / "Source" / "raceGPSAkronBeta" / "Private" / "AkronXodrImporter.cpp"
ONBOARD_CPP = PROJECT_ROOT / "apps" / "unreal-akron-beta" / "Source" / "raceGPSAkronBeta" / "Private" / "OnboardingManager.cpp"
MENU_CPP = PROJECT_ROOT / "apps" / "unreal-akron-beta" / "Source" / "raceGPSAkronBeta" / "Private" / "MainMenuWidget.cpp"
CRUISE_CPP = PROJECT_ROOT / "apps" / "unreal-akron-beta" / "Source" / "raceGPSAkronBeta" / "Private" / "CruiseSprintGameMode.cpp"
README = PROJECT_ROOT / "apps" / "unreal-akron-beta" / "README.md"


def test_preflight_summary_exposes_viability_gate_and_required_paths():
    header = PRE_H.read_text(encoding="utf-8")
    source = PRE_CPP.read_text(encoding="utf-8")
    importer = IMPORTER_CPP.read_text(encoding="utf-8")

    assert 'bool bCanLaunch = false;' in header
    assert 'Summary.bCanLaunch = Summary.FailCount == 0;' in source

    # Citypack paths are resolved from the active city (config/cvar/command
    # line) rather than hardcoded to Akron.
    assert 'UAkronXodrImporter::GetActiveCityId()' in source
    assert 'UAkronXodrImporter::ResolveCityLayout(Layout)' in source
    assert 'Layout.ManifestPath' in source
    assert 'Layout.RoutesPath' in source
    assert 'Layout.LevelSpecPath' in source
    assert 'akron_routes.json' not in source

    # Akron remains the built-in default when nothing is configured.
    assert 'RACEGPS_DEFAULT_CITY_ID = TEXT("akron-oh-beta-001")' in importer


def test_save_directory_check_does_not_false_pass_not_writable():
    source = PRE_CPP.read_text(encoding="utf-8")
    assert 'Status.StartsWith(TEXT("Save directory writable:"))' in source
    assert 'Status.Contains(TEXT("writable"))' not in source


def test_mark_first_run_complete_creates_config_dir_before_save():
    source = PRE_CPP.read_text(encoding="utf-8")
    assert 'FString ConfigDir = FPaths::ProjectSavedDir() / TEXT("Config");' in source
    assert 'CreateDirectoryTree(*ConfigDir);' in source


def test_onboarding_finish_persists_preflight_summary_for_clear_pass_fail():
    source = ONBOARD_CPP.read_text(encoding="utf-8")
    assert 'const TArray<FPreflightCheck> Checks = UPreflightSystem::RunAllChecks();' in source
    assert 'const FPreflightSummary Summary = UPreflightSystem::GetSummary(Checks);' in source
    assert 'Root->SetBoolField(TEXT("preflight_can_launch"), Summary.bCanLaunch);' in source
    assert 'Root->SetNumberField(TEXT("preflight_fail_count"), Summary.FailCount);' in source
    assert 'Root->SetNumberField(TEXT("preflight_warning_count"), Summary.WarningCount);' in source


def test_main_menu_play_path_has_explicit_gate_for_missing_route_vehicle_or_handling():
    source = MENU_CPP.read_text(encoding="utf-8")
    assert 'RouteSelector->GetSelectedOption().IsEmpty()' in source
    assert 'GetSelectedVehicle()' in source
    assert 'GetSelectedHandlingMode().IsEmpty()' in source or 'GetSelectedHandlingMode()' in source
    assert 'UE_LOG(LogTemp, Warning, TEXT("[raceGPS] Cannot start play:' in source


def test_race_start_proof_points_exist_in_game_mode():
    source = CRUISE_CPP.read_text(encoding="utf-8")
    assert 'LoadCityData();' in source
    assert 'SpawnPlayerAtStart();' in source
    assert 'SpawnCheckpoints();' in source
    assert 'ApplyVehicleTuningToPlayer();' in source
    assert 'TutorialSystem->StartTutorial();' in source


def test_readme_has_clear_editor_blocker_language_for_akronworld():
    readme = README.read_text(encoding="utf-8")
    assert 'AkronWorld.umap' in readme
    assert 'must be finalized inside the Unreal Editor' in readme
