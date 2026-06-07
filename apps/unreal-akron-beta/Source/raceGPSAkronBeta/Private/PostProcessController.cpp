// Copyright raceGPS. All Rights Reserved.

#include "PostProcessController.h"
#include "Components/PostProcessComponent.h"
#include "VisualQualitySettings.h"
#include "Engine/Texture.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"

APostProcessController::APostProcessController()
{
    PrimaryActorTick.bCanEverTick = false;

    PostProcessComp = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
    PostProcessComp->bUnbound = true; // Global post-process
    RootComponent = PostProcessComp;

    // Menu preset: high contrast, cinematic, no motion blur
    MenuPreset.BloomIntensity = 1.2f;
    MenuPreset.BloomThreshold = 0.8f;
    MenuPreset.MotionBlurAmount = 0.0f;
    MenuPreset.MotionBlurMax = 0.0f;
    MenuPreset.DepthOfFieldFocalDistance = 5000.0f;
    MenuPreset.DepthOfFieldDepthBlurAmount = 0.0f;
    MenuPreset.LensFlareIntensity = 0.5f;
    MenuPreset.ChromaticAberrationIntensity = 0.0f;
    MenuPreset.FilmGrainIntensity = 0.03f;
    MenuPreset.VignetteIntensity = 0.6f;
    MenuPreset.AutoExposureBias = 0.0f;
    MenuPreset.Contrast = 1.1f;
    MenuPreset.Saturation = 1.05f;
    MenuPreset.Gamma = 1.0f;

    // Low preset: minimal effects for performance
    LowPreset = MenuPreset;
    LowPreset.BloomIntensity = 0.5f;
    LowPreset.FilmGrainIntensity = 0.0f;
    LowPreset.VignetteIntensity = 0.2f;
    LowPreset.Contrast = 1.0f;
    LowPreset.Saturation = 1.0f;

    // Medium preset: balanced
    MediumPreset = MenuPreset;
    MediumPreset.MotionBlurAmount = 0.3f;
    MediumPreset.MotionBlurMax = 0.5f;
    MediumPreset.ChromaticAberrationIntensity = 0.05f;
    MediumPreset.FilmGrainIntensity = 0.01f;
    MediumPreset.VignetteIntensity = 0.4f;

    // High preset: full cinematic
    HighPreset = MenuPreset;
    HighPreset.BloomIntensity = 1.5f;
    HighPreset.BloomThreshold = 0.7f;
    HighPreset.MotionBlurAmount = 0.5f;
    HighPreset.MotionBlurMax = 1.0f;
    HighPreset.DepthOfFieldFocalDistance = 1000.0f;
    HighPreset.DepthOfFieldDepthBlurAmount = 1.0f;
    HighPreset.LensFlareIntensity = 1.0f;
    HighPreset.ChromaticAberrationIntensity = 0.15f;
    HighPreset.FilmGrainIntensity = 0.02f;
    HighPreset.VignetteIntensity = 0.5f;
    HighPreset.Contrast = 1.15f;
    HighPreset.Saturation = 1.1f;

    // Epic preset: maximum fidelity
    EpicPreset = HighPreset;
    EpicPreset.BloomIntensity = 2.0f;
    EpicPreset.MotionBlurAmount = 0.6f;
    EpicPreset.DepthOfFieldDepthBlurAmount = 2.0f;
    EpicPreset.FilmGrainIntensity = 0.03f;
    EpicPreset.Contrast = 1.2f;
    EpicPreset.Saturation = 1.15f;
}

void APostProcessController::BeginPlay()
{
    Super::BeginPlay();
    ApplyPresetForTier(UVisualQualitySettings::GetRecommendedTier());
}

void APostProcessController::ApplyPresetForTier(EVisualQualityTier Tier)
{
    switch (Tier)
    {
    case EVisualQualityTier::Low:    ApplyPreset(LowPreset, Tier);    break;
    case EVisualQualityTier::Medium: ApplyPreset(MediumPreset, Tier); break;
    case EVisualQualityTier::High:   ApplyPreset(HighPreset, Tier);   break;
    case EVisualQualityTier::Epic:   ApplyPreset(EpicPreset, Tier);   break;
    }
}

void APostProcessController::ApplyMenuPreset()
{
    ApplyPreset(MenuPreset, EVisualQualityTier::Epic);
}

void APostProcessController::ApplyRacingBoostEffect(float SpeedKmh)
{
    if (!PostProcessComp) return;

    // Intensify chromatic aberration and motion blur at high speed
    float SpeedFactor = FMath::Clamp((SpeedKmh - 80.0f) / 120.0f, 0.0f, 1.0f);
    FPostProcessSettings& Settings = PostProcessComp->Settings;

    Settings.bOverride_SceneFringeIntensity = true;
    Settings.SceneFringeIntensity = FMath::Lerp(0.0f, 0.3f, SpeedFactor);

    Settings.bOverride_MotionBlurAmount = true;
    Settings.MotionBlurAmount = FMath::Lerp(0.3f, 0.8f, SpeedFactor);
}

void APostProcessController::ResetToDefault()
{
    if (PostProcessComp)
    {
        PostProcessComp->Settings = FPostProcessSettings();
    }
}

void APostProcessController::SetColorLUT(UTexture* LUT)
{
    if (!PostProcessComp || !LUT) return;
    PostProcessComp->Settings.bOverride_ColorGamma = true;
    PostProcessComp->Settings.ColorGamma = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
    PostProcessComp->Settings.bOverride_BloomIntensity = true;
}

void APostProcessController::ApplyPreset(const FPostProcessPreset& Preset, EVisualQualityTier Tier)
{
    if (!PostProcessComp) return;
    PostProcessComp->Settings = BuildSettings(Preset, Tier);
}

FPostProcessSettings APostProcessController::BuildSettings(const FPostProcessPreset& Preset, EVisualQualityTier Tier)
{
    FPostProcessSettings S;

    // Bloom
    S.bOverride_BloomIntensity = true;
    S.BloomIntensity = Preset.BloomIntensity;
    S.bOverride_BloomThreshold = true;
    S.BloomThreshold = Preset.BloomThreshold;
    S.bOverride_BloomSizeScale = true;
    S.BloomSizeScale = 1.0f;

    // Motion Blur
    S.bOverride_MotionBlurAmount = true;
    S.MotionBlurAmount = Preset.MotionBlurAmount;
    S.bOverride_MotionBlurMax = true;
    S.MotionBlurMax = Preset.MotionBlurMax;

    // Depth of Field
    S.bOverride_DepthOfFieldFocalDistance = true;
    S.DepthOfFieldFocalDistance = Preset.DepthOfFieldFocalDistance;
    S.bOverride_DepthOfFieldDepthBlurAmount = true;
    S.DepthOfFieldDepthBlurAmount = Preset.DepthOfFieldDepthBlurAmount;

    // Lens Flare
    S.bOverride_LensFlareIntensity = true;
    S.LensFlareIntensity = Preset.LensFlareIntensity;

    // Chromatic Aberration
    S.bOverride_SceneFringeIntensity = true;
    S.SceneFringeIntensity = Preset.ChromaticAberrationIntensity;

    // Film Grain
    S.bOverride_FilmGrainIntensity = true;
    S.FilmGrainIntensity = Preset.FilmGrainIntensity;

    // Vignette
    S.bOverride_VignetteIntensity = true;
    S.VignetteIntensity = Preset.VignetteIntensity;

    // Color grading
    S.bOverride_ColorContrast = true;
    S.ColorContrast = FVector4(Preset.Contrast, Preset.Contrast, Preset.Contrast, 1.0f);
    S.bOverride_ColorSaturation = true;
    S.ColorSaturation = FVector4(Preset.Saturation, Preset.Saturation, Preset.Saturation, 1.0f);
    S.bOverride_ColorGamma = true;
    S.ColorGamma = FVector4(Preset.Gamma, Preset.Gamma, Preset.Gamma, 1.0f);

    // Auto Exposure
    S.bOverride_AutoExposureBias = true;
    S.AutoExposureBias = Preset.AutoExposureBias;

    // Color LUT
    if (Preset.ColorLUT.IsValid())
    {
        S.bOverride_ColorGamma = true;
        // LUT application would require additional setup
    }

    // Quality-tier specific overrides
    FVisualQualityConfig QC = UVisualQualitySettings::GetConfigForTier(Tier);
    if (!QC.bEnableBloom)
    {
        S.BloomIntensity = 0.0f;
    }
    if (!QC.bEnableMotionBlur)
    {
        S.MotionBlurAmount = 0.0f;
        S.MotionBlurMax = 0.0f;
    }
    if (!QC.bEnableDepthOfField)
    {
        S.DepthOfFieldDepthBlurAmount = 0.0f;
    }
    if (!QC.bEnableLensFlare)
    {
        S.LensFlareIntensity = 0.0f;
    }
    if (!QC.bEnableChromaticAberration)
    {
        S.SceneFringeIntensity = 0.0f;
    }
    if (!QC.bEnableFilmGrain)
    {
        S.FilmGrainIntensity = 0.0f;
    }
    if (!QC.bEnableVignette)
    {
        S.VignetteIntensity = 0.0f;
    }

    return S;
}
