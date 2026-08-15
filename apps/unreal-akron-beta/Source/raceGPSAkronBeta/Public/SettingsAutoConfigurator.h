// Copyright raceGPS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SettingsAutoConfigurator.generated.h"

UENUM(BlueprintType)
enum class ERaceGPSGraphicsPreset : uint8
{
    Low     UMETA(DisplayName = "Low"),
    Medium  UMETA(DisplayName = "Medium"),
    High    UMETA(DisplayName = "High"),
    Ultra   UMETA(DisplayName = "Ultra"),
};

/**
 * Applies recommended graphics settings based on hardware preflight results.
 */
UCLASS()
class RACEGPSAKRONBETA_API USettingsAutoConfigurator : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Settings")
    static void ApplyPreset(ERaceGPSGraphicsPreset Preset);

    UFUNCTION(BlueprintCallable, Category = "Settings")
    static void ApplyRecommendedPreset();

    UFUNCTION(BlueprintPure, Category = "Settings")
    static ERaceGPSGraphicsPreset GetRecommendedPresetEnum();

private:
    static void SetScalabilitySettings(int32 QualityLevel);
    static void SetResolutionSettings();
};
