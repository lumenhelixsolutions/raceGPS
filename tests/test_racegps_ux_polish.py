from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
MAIN_MENU_H = PROJECT_ROOT / "apps" / "unreal-akron-beta" / "Source" / "raceGPSAkronBeta" / "Public" / "MainMenuWidget.h"
MAIN_MENU_CPP = PROJECT_ROOT / "apps" / "unreal-akron-beta" / "Source" / "raceGPSAkronBeta" / "Private" / "MainMenuWidget.cpp"
ONBOARD_H = PROJECT_ROOT / "apps" / "unreal-akron-beta" / "Source" / "raceGPSAkronBeta" / "Public" / "OnboardingManager.h"
ONBOARD_CPP = PROJECT_ROOT / "apps" / "unreal-akron-beta" / "Source" / "raceGPSAkronBeta" / "Private" / "OnboardingManager.cpp"


def test_main_menu_exposes_polished_summary_surface():
    header = MAIN_MENU_H.read_text(encoding="utf-8")
    source = MAIN_MENU_CPP.read_text(encoding="utf-8")

    assert 'FString BuildDriveSummaryText() const;' in header
    assert 'FString BuildLaunchButtonText() const;' in header
    assert 'FString BuildVersionLine() const;' in header
    assert 'FString BuildVehicleClassLabel(const UVehicleTuningData* Vehicle) const;' in header
    assert 'TitleText->SetText' in source
    assert 'BuildLaunchButtonText()' in source
    assert 'BuildDriveSummaryText()' in source
    assert 'BuildVersionLine()' in source

    # Launch button text is built from the resolved city layout (display-name
    # driven), with Akron kept as the fallback default when nothing is configured.
    assert 'UAkronXodrImporter::ResolveCityLayout(Layout)' in source
    assert 'Layout.DisplayName' in source
    assert 'FString CityName = TEXT("Akron");' in source


def test_onboarding_exposes_premium_step_copy_and_viability_message():
    header = ONBOARD_H.read_text(encoding="utf-8")
    source = ONBOARD_CPP.read_text(encoding="utf-8")

    assert 'FString GetStepSubtitle(int32 StepIndex) const;' in header
    assert 'FString BuildCompletionMessage(bool bCanLaunch, int32 FailCount, int32 WarningCount) const;' in header
    assert 'Ready to drive' in source
    assert 'needs attention before your first drive' in source
    assert 'Pick your city' in source
