// Copyright raceGPS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VisualQualitySettings.h"
#include "PostProcessController.generated.h"

USTRUCT(BlueprintType)
struct FPostProcessPreset
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess")
    float BloomIntensity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess")
    float BloomThreshold = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess")
    float MotionBlurAmount = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess")
    float MotionBlurMax = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess")
    float DepthOfFieldFocalDistance = 1000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess")
    float DepthOfFieldDepthBlurAmount = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess")
    float LensFlareIntensity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess")
    float ChromaticAberrationIntensity = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess")
    float FilmGrainIntensity = 0.02f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess")
    float VignetteIntensity = 0.4f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess")
    float AutoExposureBias = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess")
    float SceneColorTintR = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess")
    float SceneColorTintG = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess")
    float SceneColorTintB = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess")
    float Contrast = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess")
    float Saturation = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess")
    float Gamma = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess")
    TSoftObjectPtr<class UTexture> ColorLUT;
};

/**
 * Applies quality-tiered post-process settings to the active camera.
 * Designed for racing game cinematic feel: high contrast, subtle film grain,
 * chromatic aberration on speed, bloom on neon/glass.
 */
UCLASS()
class RACEGPSAKRONBETA_API APostProcessController : public AActor
{
    GENERATED_BODY()

public:
    APostProcessController();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess")
    FPostProcessPreset LowPreset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess")
    FPostProcessPreset MediumPreset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess")
    FPostProcessPreset HighPreset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess")
    FPostProcessPreset EpicPreset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess")
    FPostProcessPreset MenuPreset;

    UFUNCTION(BlueprintCallable, Category = "PostProcess")
    void ApplyPresetForTier(EVisualQualityTier Tier);

    UFUNCTION(BlueprintCallable, Category = "PostProcess")
    void ApplyMenuPreset();

    UFUNCTION(BlueprintCallable, Category = "PostProcess")
    void ApplyRacingBoostEffect(float SpeedKmh);

    UFUNCTION(BlueprintCallable, Category = "PostProcess")
    void ResetToDefault();

    UFUNCTION(BlueprintCallable, Category = "PostProcess")
    void SetColorLUT(UTexture* LUT);

protected:
    virtual void BeginPlay() override;

    UPROPERTY()
    TObjectPtr<class UPostProcessComponent> PostProcessComp;

    void ApplyPreset(const FPostProcessPreset& Preset, EVisualQualityTier Tier);
    FPostProcessSettings BuildSettings(const FPostProcessPreset& Preset, EVisualQualityTier Tier);
};
