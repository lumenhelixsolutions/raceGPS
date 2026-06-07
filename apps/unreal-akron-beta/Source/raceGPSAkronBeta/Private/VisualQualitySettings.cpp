// Copyright raceGPS. All Rights Reserved.

#include "VisualQualitySettings.h"
#include "PreflightSystem.h"
#include "Engine/Engine.h"
#include "Scalability.h"

FVisualQualityConfig UVisualQualitySettings::GetConfigForTier(EVisualQualityTier Tier)
{
    FVisualQualityConfig Config;
    Config.Tier = Tier;

    switch (Tier)
    {
    case EVisualQualityTier::Low:
        Config.ViewDistanceQuality = 0;
        Config.AntiAliasingQuality = 0;
        Config.ShadowQuality = 0;
        Config.PostProcessQuality = 0;
        Config.TextureQuality = 0;
        Config.EffectsQuality = 0;
        Config.FoliageQuality = 0;
        Config.ShadingQuality = 0;
        Config.bEnableMotionBlur = false;
        Config.bEnableBloom = false;
        Config.bEnableAmbientOcclusion = false;
        Config.bEnableDepthOfField = false;
        Config.bEnableLensFlare = false;
        Config.bEnableChromaticAberration = false;
        Config.bEnableFilmGrain = false;
        Config.bEnableVignette = false;
        break;

    case EVisualQualityTier::Medium:
        Config.ViewDistanceQuality = 1;
        Config.AntiAliasingQuality = 1;
        Config.ShadowQuality = 1;
        Config.PostProcessQuality = 1;
        Config.TextureQuality = 1;
        Config.EffectsQuality = 1;
        Config.FoliageQuality = 1;
        Config.ShadingQuality = 1;
        Config.bEnableMotionBlur = true;
        Config.bEnableBloom = true;
        Config.bEnableAmbientOcclusion = true;
        Config.bEnableDepthOfField = false;
        Config.bEnableLensFlare = true;
        Config.bEnableChromaticAberration = false;
        Config.bEnableFilmGrain = false;
        Config.bEnableVignette = false;
        break;

    case EVisualQualityTier::High:
        Config.ViewDistanceQuality = 2;
        Config.AntiAliasingQuality = 2;
        Config.ShadowQuality = 2;
        Config.PostProcessQuality = 2;
        Config.TextureQuality = 2;
        Config.EffectsQuality = 2;
        Config.FoliageQuality = 2;
        Config.ShadingQuality = 2;
        Config.bEnableMotionBlur = true;
        Config.bEnableBloom = true;
        Config.bEnableAmbientOcclusion = true;
        Config.bEnableDepthOfField = true;
        Config.bEnableLensFlare = true;
        Config.bEnableChromaticAberration = true;
        Config.bEnableFilmGrain = false;
        Config.bEnableVignette = false;
        break;

    case EVisualQualityTier::Epic:
        Config.ViewDistanceQuality = 3;
        Config.AntiAliasingQuality = 3;
        Config.ShadowQuality = 3;
        Config.PostProcessQuality = 3;
        Config.TextureQuality = 3;
        Config.EffectsQuality = 3;
        Config.FoliageQuality = 3;
        Config.ShadingQuality = 3;
        Config.bEnableMotionBlur = true;
        Config.bEnableBloom = true;
        Config.bEnableAmbientOcclusion = true;
        Config.bEnableDepthOfField = true;
        Config.bEnableLensFlare = true;
        Config.bEnableChromaticAberration = true;
        Config.bEnableFilmGrain = true;
        Config.bEnableVignette = true;
        break;
    }
    return Config;
}

EVisualQualityTier UVisualQualitySettings::GetRecommendedTier()
{
    FString Preset = UPreflightSystem::GetRecommendedGraphicsPreset();
    if (Preset == TEXT("Ultra")) return EVisualQualityTier::Epic;
    if (Preset == TEXT("High")) return EVisualQualityTier::High;
    if (Preset == TEXT("Low")) return EVisualQualityTier::Low;
    return EVisualQualityTier::Medium;
}

void UVisualQualitySettings::ApplyTier(EVisualQualityTier Tier)
{
    FVisualQualityConfig Config = GetConfigForTier(Tier);

    Scalability::FQualityLevels Quality;
    Quality.ResolutionQuality = 100;
    Quality.ViewDistanceQuality = Config.ViewDistanceQuality;
    Quality.AntiAliasingQuality = Config.AntiAliasingQuality;
    Quality.ShadowQuality = Config.ShadowQuality;
    Quality.PostProcessQuality = Config.PostProcessQuality;
    Quality.TextureQuality = Config.TextureQuality;
    Quality.EffectsQuality = Config.EffectsQuality;
    Quality.FoliageQuality = Config.FoliageQuality;
    Quality.ShadingQuality = Config.ShadingQuality;
    Scalability::SetQualityLevels(Quality);

    if (GEngine)
    {
        GEngine->Exec(nullptr, *FString::Printf(TEXT("r.MotionBlurQuality %d"), Config.bEnableMotionBlur ? 3 : 0));
        GEngine->Exec(nullptr, *FString::Printf(TEXT("r.BloomQuality %d"), Config.bEnableBloom ? 5 : 0));
        GEngine->Exec(nullptr, *FString::Printf(TEXT("r.AmbientOcclusionLevels %d"), Config.bEnableAmbientOcclusion ? 2 : 0));
        GEngine->Exec(nullptr, *FString::Printf(TEXT("r.DepthOfFieldQuality %d"), Config.bEnableDepthOfField ? 2 : 0));
        GEngine->Exec(nullptr, *FString::Printf(TEXT("r.LensFlareQuality %d"), Config.bEnableLensFlare ? 2 : 0));
        GEngine->Exec(nullptr, *FString::Printf(TEXT("r.SceneColorFringeQuality %d"), Config.bEnableChromaticAberration ? 1 : 0));
        GEngine->Exec(nullptr, *FString::Printf(TEXT("r.FilmGrain %d"), Config.bEnableFilmGrain ? 1 : 0));
        GEngine->Exec(nullptr, *FString::Printf(TEXT("r.Vignette %d"), Config.bEnableVignette ? 1 : 0));
    }
}

void UVisualQualitySettings::ApplyRecommendedTier()
{
    ApplyTier(GetRecommendedTier());
}
