// Copyright raceGPS. All Rights Reserved.

#include "SettingsAutoConfigurator.h"
#include "PreflightSystem.h"
#include "Engine/Engine.h"
#include "Scalability.h"

void USettingsAutoConfigurator::ApplyPreset(ERaceGPSGraphicsPreset Preset)
{
    int32 QualityLevel = 2; // Medium default
    switch (Preset)
    {
    case ERaceGPSGraphicsPreset::Low:    QualityLevel = 0; break;
    case ERaceGPSGraphicsPreset::Medium: QualityLevel = 2; break;
    case ERaceGPSGraphicsPreset::High:   QualityLevel = 3; break;
    case ERaceGPSGraphicsPreset::Ultra:  QualityLevel = 4; break;
    }
    SetScalabilitySettings(QualityLevel);
    SetResolutionSettings();
}

void USettingsAutoConfigurator::ApplyRecommendedPreset()
{
    ApplyPreset(GetRecommendedPresetEnum());
}

ERaceGPSGraphicsPreset USettingsAutoConfigurator::GetRecommendedPresetEnum()
{
    FString PresetStr = UPreflightSystem::GetRecommendedGraphicsPreset();
    if (PresetStr == TEXT("Ultra")) return ERaceGPSGraphicsPreset::Ultra;
    if (PresetStr == TEXT("High"))  return ERaceGPSGraphicsPreset::High;
    if (PresetStr == TEXT("Low"))   return ERaceGPSGraphicsPreset::Low;
    return ERaceGPSGraphicsPreset::Medium;
}

void USettingsAutoConfigurator::SetScalabilitySettings(int32 QualityLevel)
{
    Scalability::FQualityLevels Quality;
    Quality.SetFromSingleQualityLevel(QualityLevel);
    Scalability::SetQualityLevels(Quality);

    // Additional per-platform tweaks
    if (GEngine)
    {
        GEngine->Exec(nullptr, *FString::Printf(TEXT("r.ViewDistanceScale %.2f"), 0.4f + QualityLevel * 0.3f));
        GEngine->Exec(nullptr, *FString::Printf(TEXT("r.Shadow.DistanceScale %.2f"), 0.5f + QualityLevel * 0.25f));
    }
}

void USettingsAutoConfigurator::SetResolutionSettings()
{
    // Set sensible defaults
    if (GEngine)
    {
        GEngine->Exec(nullptr, TEXT("r.FullScreenMode 1")); // Borderless
        GEngine->Exec(nullptr, TEXT("r.VSync 1"));
        GEngine->Exec(nullptr, TEXT("t.MaxFPS 60"));
    }
}
