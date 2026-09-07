from pathlib import Path

ROOT = Path(r"C:\projects\racegps\apps\unreal-akron-beta\Source\raceGPSAkronBeta")

# --- ClevelandLookDirector.h ---
h = (ROOT / "Public" / "ClevelandLookDirector.h").read_text(encoding="utf-8")
old = '''    void ApplyEpicConsoleVars() const;
    void ApplyLookToEnvironment() const;
'''
new = '''    void ApplyEpicConsoleVars() const;
    void ApplyLookToEnvironment() const;
    void EnsureLightingFailsafe() const;
    void LogFinalLook(const TCHAR* Tag) const;
'''
if old not in h:
    raise SystemExit('LookDirector.h helpers not found')
(ROOT / "Public" / "ClevelandLookDirector.h").write_text(h.replace(old, new, 1), encoding="utf-8")
print('patched ClevelandLookDirector.h')

# --- ClevelandLookDirector.cpp ---
cpp = (ROOT / "Private" / "ClevelandLookDirector.cpp").read_text(encoding="utf-8")

old_inc = '''#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
'''
new_inc = '''#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/SkyLight.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Camera/PlayerCameraManager.h"
'''
if old_inc not in cpp:
    raise SystemExit('LookDirector includes not found')
cpp = cpp.replace(old_inc, new_inc, 1)

old_vars = '''    GEngine->Exec(World, TEXT("r.DefaultFeature.Bloom 1"));
    GEngine->Exec(World, TEXT("r.DefaultFeature.AutoExposure 1"));
    GEngine->Exec(World, TEXT("r.ViewDistanceScale 1.5"));
'''
new_vars = '''    GEngine->Exec(World, TEXT("r.DefaultFeature.Bloom 1"));
    GEngine->Exec(World, TEXT("r.DefaultFeature.AutoExposure 1"));
    GEngine->Exec(World, TEXT("r.DefaultFeature.AutoExposure.Method 1"));
    GEngine->Exec(World, TEXT("r.EyeAdaptationQuality 2"));
    GEngine->Exec(World, TEXT("r.EyeAdaptation.PreExposureOverride 0"));
    GEngine->Exec(World, TEXT("r.Histogram.Min -4"));
    GEngine->Exec(World, TEXT("r.Histogram.Max 4"));
    GEngine->Exec(World, TEXT("r.ViewDistanceScale 1.5"));
'''
if old_vars not in cpp:
    raise SystemExit('ApplyEpicConsoleVars autoexposure block not found')
cpp = cpp.replace(old_vars, new_vars, 1)

old_apply = '''    ApplyLookToEnvironment();
}
'''
new_apply = '''    ApplyLookToEnvironment();
    EnsureLightingFailsafe();
    LogFinalLook(Mode == EClevelandVisualMode::MidnightRun ? TEXT("MidnightRun") : TEXT("SunnyDay"));
}
'''
if old_apply not in cpp:
    raise SystemExit('ApplyVisualMode tail not found')
cpp = cpp.replace(old_apply, new_apply, 1)

old_mid = '''void AClevelandLookDirector::ApplyMidnightRun()
{
    if (Cycle)
    {
        Cycle->SetTimeOfDay(22.0f);
        Cycle->bPaused = true;
        Cycle->bUseSkyAtmosphere = true;
        Cycle->bUseVolumetricClouds = true;
        if (Cycle->SunLight)
        {
            // Readable Midnight Club night, not a black void. Cool moonlight, DayNightCycle sun only.
            Cycle->SunLight->SetIntensity(1.15f);
            Cycle->SunLight->SetLightColor(FLinearColor(0.55f, 0.68f, 1.0f));
            Cycle->SunLight->SetVisibility(true);
        }
        if (Cycle->SkyLight)
        {
            Cycle->SkyLight->SetIntensity(1.55f);
            Cycle->SkyLight->SetLightColor(FLinearColor(0.55f, 0.65f, 1.0f));
        }
    }
    if (Post)
    {
        // Mutate EpicPreset then ApplyPresetForTier so BuildSettings actually ships the night grade.
        Post->EpicPreset.BloomIntensity = 2.85f;
        Post->EpicPreset.BloomThreshold = 0.42f;
        Post->EpicPreset.Contrast = 1.32f;
        Post->EpicPreset.Saturation = 0.92f;
        Post->EpicPreset.ChromaticAberrationIntensity = 0.14f;
        Post->EpicPreset.VignetteIntensity = 0.58f;
        Post->EpicPreset.SceneColorTintR = 0.70f;
        Post->EpicPreset.SceneColorTintG = 0.80f;
        Post->EpicPreset.SceneColorTintB = 1.18f;
        Post->EpicPreset.AutoExposureBias = -0.28f;
        Post->EpicPreset.FilmGrainIntensity = 0.045f;
        Post->EpicPreset.MotionBlurAmount = 0.48f;
        Post->EpicPreset.LensFlareIntensity = 0.85f;
        Post->ApplyPresetForTier(EVisualQualityTier::Epic);
        Post->ApplyRacingBoostEffect(110.0f);
    }
    if (GEngine)
    {
        GEngine->Exec(GetWorld(), TEXT("r.VolumetricCloud 1"));
        GEngine->Exec(GetWorld(), TEXT("r.SkyAtmosphere 1"));
        GEngine->Exec(GetWorld(), TEXT("r.BloomQuality 5"));
        GEngine->Exec(GetWorld(), TEXT("r.Tonemapper.Quality 5"));
    }
    UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland look: MidnightRun applied (22:00 moonlight, MC night grade, competing lights off)"));
}
'''
new_mid = '''void AClevelandLookDirector::ApplyMidnightRun()
{
    if (Cycle)
    {
        Cycle->bMoonAtNight = true;
        Cycle->NightMoonIntensity = 1.55f;
        Cycle->bPaused = true;
        Cycle->bUseSkyAtmosphere = true;
        Cycle->bUseVolumetricClouds = true;
        // SetTimeOfDay calls UpdateSunRotation: with bMoonAtNight the directional stays
        // a high moon (~-52 pitch) instead of +69 below-horizon (the black-screen cause).
        Cycle->SetTimeOfDay(22.0f);
        if (Cycle->SunLight)
        {
            Cycle->SunLight->SetIntensity(1.55f);
            Cycle->SunLight->SetLightColor(FLinearColor(0.62f, 0.76f, 1.0f));
            Cycle->SunLight->SetVisibility(true);
        }
        if (Cycle->SkyLight)
        {
            Cycle->SkyLight->SetIntensity(1.85f);
            Cycle->SkyLight->SetLightColor(FLinearColor(0.62f, 0.74f, 1.0f));
            Cycle->SkyLight->SetVisibility(true);
            Cycle->SkyLight->RecaptureSky();
        }
    }
    if (Post)
    {
        // Mutate EpicPreset then ApplyPresetForTier so BuildSettings actually ships the night grade.
        Post->EpicPreset.BloomIntensity = 2.20f;
        Post->EpicPreset.BloomThreshold = 0.65f;
        Post->EpicPreset.Contrast = 1.12f;
        Post->EpicPreset.Saturation = 1.02f;
        Post->EpicPreset.ChromaticAberrationIntensity = 0.08f;
        Post->EpicPreset.VignetteIntensity = 0.32f;
        Post->EpicPreset.SceneColorTintR = 0.75f;
        Post->EpicPreset.SceneColorTintG = 0.85f;
        Post->EpicPreset.SceneColorTintB = 1.15f;
        Post->EpicPreset.AutoExposureBias = 1.0f;
        Post->EpicPreset.FilmGrainIntensity = 0.025f;
        Post->EpicPreset.MotionBlurAmount = 0.28f;
        Post->EpicPreset.LensFlareIntensity = 0.55f;
        Post->ApplyPresetForTier(EVisualQualityTier::Epic);
        // Do not ApplyRacingBoostEffect here: parked grid + extra motion blur is not a night look.
    }
    if (GEngine)
    {
        GEngine->Exec(GetWorld(), TEXT("r.VolumetricCloud 1"));
        GEngine->Exec(GetWorld(), TEXT("r.SkyAtmosphere 1"));
        GEngine->Exec(GetWorld(), TEXT("r.BloomQuality 5"));
        GEngine->Exec(GetWorld(), TEXT("r.Tonemapper.Quality 5"));
        GEngine->Exec(GetWorld(), TEXT("r.DefaultFeature.AutoExposure 1"));
        GEngine->Exec(GetWorld(), TEXT("r.EyeAdaptationQuality 2"));
    }
    UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland look: MidnightRun applied (22:00 moon floor, exposure bias +1.0, competing lights off)"));
}

void AClevelandLookDirector::EnsureLightingFailsafe() const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    USkyLightComponent* Sky = Cycle ? Cycle->SkyLight.Get() : nullptr;
    if (!Sky)
    {
        TArray<AActor*> Existing;
        UGameplayStatics::GetAllActorsOfClass(World, ASkyLight::StaticClass(), Existing);
        if (Existing.Num() > 0)
        {
            Sky = Existing[0]->FindComponentByClass<USkyLightComponent>();
        }
        if (!Sky)
        {
            if (ASkyLight* Spawned = World->SpawnActor<ASkyLight>(ASkyLight::StaticClass()))
            {
                Sky = Spawned->GetCaptureComponent();
                UE_LOG(LogTemp, Warning, TEXT("raceGPS Cleveland look: spawned failsafe ASkyLight"));
            }
        }
    }
    if (Sky)
    {
        Sky->SetVisibility(true);
        if (Sky->Intensity < 1.2f)
        {
            Sky->SetIntensity(1.85f);
        }
        Sky->SetLightColor(FLinearColor(0.62f, 0.74f, 1.0f));
        Sky->RecaptureSky();
    }

    if (Cycle && Cycle->SkyAtmosphere)
    {
        Cycle->SkyAtmosphere->SetVisibility(true);
    }

    if (GEngine)
    {
        GEngine->Exec(World, TEXT("r.DefaultFeature.AutoExposure 1"));
        GEngine->Exec(World, TEXT("r.SkyAtmosphere 1"));
        GEngine->Exec(World, TEXT("r.DefaultFeature.AutoExposure.Bias 1.0"));
    }
}

void AClevelandLookDirector::LogFinalLook(const TCHAR* Tag) const
{
    const float SunI = (Cycle && Cycle->SunLight) ? Cycle->SunLight->Intensity : -1.f;
    const FRotator SunR = (Cycle && Cycle->SunLight) ? Cycle->SunLight->GetComponentRotation() : FRotator::ZeroRotator;
    const float SkyI = (Cycle && Cycle->SkyLight) ? Cycle->SkyLight->Intensity : -1.f;
    const float Bias = Post ? Post->EpicPreset.AutoExposureBias : 0.f;
    const float Hour = Cycle ? Cycle->GetTimeOfDay() : -1.f;
    UE_LOG(LogTemp, Warning,
        TEXT("raceGPS Cleveland look FINAL [%s]: hour=%.2f sunI=%.2f sunPitch=%.1f skyI=%.2f exposureBias=%.2f skylight=%s atmosphere=%s"),
        Tag, Hour, SunI, SunR.Pitch, SkyI, Bias,
        (Cycle && Cycle->SkyLight) ? TEXT("yes") : TEXT("NO"),
        (Cycle && Cycle->SkyAtmosphere) ? TEXT("yes") : TEXT("NO"));
}
'''
if old_mid not in cpp:
    raise SystemExit('ApplyMidnightRun block not found')
cpp = cpp.replace(old_mid, new_mid, 1)

(ROOT / "Private" / "ClevelandLookDirector.cpp").write_text(cpp, encoding="utf-8")
print('patched ClevelandLookDirector.cpp')
print('ok')
