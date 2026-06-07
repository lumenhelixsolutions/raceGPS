// Copyright raceGPS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "VisualQualitySettings.generated.h"

UENUM(BlueprintType)
enum class EVisualQualityTier : uint8
{
    Low     UMETA(DisplayName = "Low"),
    Medium  UMETA(DisplayName = "Medium"),
    High    UMETA(DisplayName = "High"),
    Epic    UMETA(DisplayName = "Epic"),
};

USTRUCT(BlueprintType)
struct FVisualQualityConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VisualQuality")
    EVisualQualityTier Tier = EVisualQualityTier::Medium;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VisualQuality")
    int32 ViewDistanceQuality = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VisualQuality")
    int32 AntiAliasingQuality = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VisualQuality")
    int32 ShadowQuality = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VisualQuality")
    int32 PostProcessQuality = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VisualQuality")
    int32 TextureQuality = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VisualQuality")
    int32 EffectsQuality = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VisualQuality")
    int32 FoliageQuality = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VisualQuality")
    int32 ShadingQuality = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VisualQuality")
    bool bEnableMotionBlur = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VisualQuality")
    bool bEnableBloom = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VisualQuality")
    bool bEnableAmbientOcclusion = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VisualQuality")
    bool bEnableDepthOfField = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VisualQuality")
    bool bEnableLensFlare = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VisualQuality")
    bool bEnableChromaticAberration = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VisualQuality")
    bool bEnableFilmGrain = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VisualQuality")
    bool bEnableVignette = false;
};

/**
 * Static quality settings lookup. Maps hardware tiers to full config.
 */
UCLASS()
class RACEGPSAKRONBETA_API UVisualQualitySettings : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "VisualQuality")
    static FVisualQualityConfig GetConfigForTier(EVisualQualityTier Tier);

    UFUNCTION(BlueprintPure, Category = "VisualQuality")
    static EVisualQualityTier GetRecommendedTier();

    UFUNCTION(BlueprintCallable, Category = "VisualQuality")
    static void ApplyTier(EVisualQualityTier Tier);

    UFUNCTION(BlueprintCallable, Category = "VisualQuality")
    static void ApplyRecommendedTier();
};
