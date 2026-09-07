#include "ClevelandLookDirector.h"
#include "DayNightCycle.h"
#include "PostProcessController.h"
#include "VisualQualitySettings.h"
#include "ClevelandEnvironmentActor.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ReflectionCapture.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/SkyLight.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "UObject/UnrealType.h"

AClevelandLookDirector::AClevelandLookDirector()
{
    PrimaryActorTick.bCanEverTick = false;
    Mode = EClevelandVisualMode::MidnightRun;
}

void AClevelandLookDirector::BeginPlay()
{
    Super::BeginPlay();
    ApplyVisualMode(Mode);
}

void AClevelandLookDirector::EnsureCycleAndPost()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    Cycle = Cast<ADayNightCycle>(UGameplayStatics::GetActorOfClass(World, ADayNightCycle::StaticClass()));
    if (!Cycle)
    {
        Cycle = World->SpawnActor<ADayNightCycle>(ADayNightCycle::StaticClass());
    }
    Post = Cast<APostProcessController>(UGameplayStatics::GetActorOfClass(World, APostProcessController::StaticClass()));
    if (!Post)
    {
        Post = World->SpawnActor<APostProcessController>(APostProcessController::StaticClass());
    }
}

void AClevelandLookDirector::SuppressCompetingLights() const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    int32 DisabledDir = 0;
    TArray<AActor*> DirLights;
    UGameplayStatics::GetAllActorsOfClass(World, ADirectionalLight::StaticClass(), DirLights);
    for (AActor* LightActor : DirLights)
    {
        if (!LightActor)
        {
            continue;
        }
        TArray<UDirectionalLightComponent*> Comps;
        LightActor->GetComponents<UDirectionalLightComponent>(Comps);
        for (UDirectionalLightComponent* Comp : Comps)
        {
            if (!Comp)
            {
                continue;
            }
            if (Cycle && Comp == Cycle->SunLight)
            {
                continue;
            }
            Comp->SetVisibility(false);
            Comp->SetIntensity(0.f);
            ++DisabledDir;
        }
    }

    int32 DisabledCaptures = 0;
    TArray<AActor*> Captures;
    UGameplayStatics::GetAllActorsOfClass(World, AReflectionCapture::StaticClass(), Captures);
    for (AActor* Capture : Captures)
    {
        if (!Capture)
        {
            continue;
        }
        Capture->SetActorHiddenInGame(true);
        Capture->SetActorEnableCollision(false);
        if (USceneComponent* Root = Capture->GetRootComponent())
        {
            Root->SetVisibility(false, true);
        }
        ++DisabledCaptures;
    }

    UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland look: suppressed extra directional lights=%d reflection captures=%d"),
        DisabledDir, DisabledCaptures);
}

void AClevelandLookDirector::ApplyEpicConsoleVars() const
{
    if (!GEngine)
    {
        return;
    }
    UWorld* World = GetWorld();
    GEngine->Exec(World, TEXT("DisableAllScreenMessages"));
    GEngine->Exec(World, TEXT("r.VolumetricCloud 1"));
    GEngine->Exec(World, TEXT("r.SkyAtmosphere 1"));
    GEngine->Exec(World, TEXT("r.ShadowQuality 5"));
    GEngine->Exec(World, TEXT("sg.ShadowQuality 4"));
    GEngine->Exec(World, TEXT("r.BloomQuality 5"));
    GEngine->Exec(World, TEXT("r.ReflectionMethod 1"));
    GEngine->Exec(World, TEXT("r.DynamicGlobalIlluminationMethod 1"));
    GEngine->Exec(World, TEXT("r.Tonemapper.Quality 5"));
    GEngine->Exec(World, TEXT("r.DefaultFeature.Bloom 1"));
    GEngine->Exec(World, TEXT("r.DefaultFeature.AutoExposure 1"));
    GEngine->Exec(World, TEXT("r.EyeAdaptationQuality 2"));
    GEngine->Exec(World, TEXT("r.Histogram.Min -4"));
    GEngine->Exec(World, TEXT("r.Histogram.Max 4"));
    GEngine->Exec(World, TEXT("r.ViewDistanceScale 1.5"));
    GEngine->Exec(World, TEXT("r.SceneColorFringeQuality 1"));
    GEngine->Exec(World, TEXT("r.MotionBlurQuality 4"));
    UVisualQualitySettings::ApplyTier(EVisualQualityTier::Epic);
}

void AClevelandLookDirector::ApplyLookToEnvironment() const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    if (AClevelandEnvironmentActor* Env = Cast<AClevelandEnvironmentActor>(
            UGameplayStatics::GetActorOfClass(World, AClevelandEnvironmentActor::StaticClass())))
    {
        Env->ApplyLookMode(Mode == EClevelandVisualMode::MidnightRun);
    }
}

void AClevelandLookDirector::ApplyVisualMode(EClevelandVisualMode InMode)
{
    Mode = InMode;
    EnsureCycleAndPost();
    ApplyEpicConsoleVars();
    SuppressCompetingLights();
    if (Mode == EClevelandVisualMode::MidnightRun)
    {
        ApplyMidnightRun();
    }
    else
    {
        ApplySunnyDay();
    }
    ApplyLookToEnvironment();
    EnsureLightingFailsafe();
    LogFinalLook(Mode == EClevelandVisualMode::MidnightRun ? TEXT("MidnightRun") : TEXT("SunnyDay"));
}

void AClevelandLookDirector::ApplySunnyDay()
{
    if (Cycle)
    {
        Cycle->bMoonAtNight = false;
        Cycle->SetTimeOfDay(15.0f);
        Cycle->bPaused = true;
        Cycle->bUseSkyAtmosphere = true;
        Cycle->bUseVolumetricClouds = true;
        if (Cycle->SunLight)
        {
            Cycle->SunLight->SetIntensity(2.6f);
            Cycle->SunLight->SetLightColor(FLinearColor(1.0f, 0.97f, 0.90f));
            Cycle->SunLight->SetVisibility(true);
        }
        if (Cycle->SkyLight)
        {
            Cycle->SkyLight->SetIntensity(1.15f);
            Cycle->SkyLight->SetLightColor(FLinearColor(0.78f, 0.86f, 1.0f));
        }
    }
    if (Post)
    {
        Post->EpicPreset.BloomIntensity = 1.65f;
        Post->EpicPreset.BloomThreshold = 0.75f;
        Post->EpicPreset.Contrast = 1.12f;
        Post->EpicPreset.Saturation = 1.18f;
        Post->EpicPreset.ChromaticAberrationIntensity = 0.06f;
        Post->EpicPreset.VignetteIntensity = 0.28f;
        Post->EpicPreset.SceneColorTintR = 1.05f;
        Post->EpicPreset.SceneColorTintG = 1.00f;
        Post->EpicPreset.SceneColorTintB = 0.94f;
        Post->EpicPreset.AutoExposureBias = 0.12f;
        Post->EpicPreset.FilmGrainIntensity = 0.015f;
        Post->EpicPreset.MotionBlurAmount = 0.40f;
        Post->ApplyPresetForTier(EVisualQualityTier::Epic);
    }
    if (GEngine)
    {
        GEngine->Exec(GetWorld(), TEXT("r.VolumetricCloud 1"));
        GEngine->Exec(GetWorld(), TEXT("r.SkyAtmosphere 1"));
        GEngine->Exec(GetWorld(), TEXT("r.BloomQuality 5"));
    }
    UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland look: SunnyDay applied (15:00 volumetric, competing lights off)"));
}

void AClevelandLookDirector::ApplyMidnightRun()
{
    if (Cycle)
    {
        Cycle->bMoonAtNight = true;
        Cycle->NightMoonIntensity = 2.35f;
        Cycle->bPaused = true;
        Cycle->bUseSkyAtmosphere = true;
        // V8: volumetric clouds at night read as mottled water/noise on the upper sky.
        Cycle->bUseVolumetricClouds = false;
        // SetTimeOfDay calls UpdateSunRotation: with bMoonAtNight the directional stays
        // a high moon instead of +69 below-horizon (the black-screen cause).
        Cycle->SetTimeOfDay(22.0f);
        if (Cycle->SunLight)
        {
            Cycle->SunLight->SetIntensity(2.40f);
            Cycle->SunLight->SetLightColor(FLinearColor(0.82f, 0.86f, 1.0f));
            Cycle->SunLight->SetVisibility(true);
            Cycle->SunLight->bAtmosphereSunLight = true;
            Cycle->SunLight->DynamicShadowDistanceMovableLight = 40000.f;
        }
        if (Cycle->SkyLight)
        {
            Cycle->SkyLight->SetIntensity(2.20f);
            Cycle->SkyLight->SetLightColor(FLinearColor(0.70f, 0.74f, 0.86f));
            Cycle->SkyLight->SetVisibility(true);
            Cycle->SkyLight->RecaptureSky();
        }
    }
    if (Post)
    {
        // Mutate EpicPreset then ApplyPresetForTier so BuildSettings actually ships the night grade.
        Post->EpicPreset.BloomIntensity = 0.55f;
        Post->EpicPreset.BloomThreshold = 1.10f; // V15: stop wet-apron bloom blowout
        Post->EpicPreset.Contrast = 1.12f;
        Post->EpicPreset.Saturation = 1.18f;
        Post->EpicPreset.ChromaticAberrationIntensity = 0.06f;
        Post->EpicPreset.VignetteIntensity = 0.28f;
        Post->EpicPreset.SceneColorTintR = 1.02f;
        Post->EpicPreset.SceneColorTintG = 0.98f;
        Post->EpicPreset.SceneColorTintB = 0.96f; // V13: warmer grade, less navy ground
        Post->EpicPreset.AutoExposureBias = 1.15f;
        Post->EpicPreset.AutoExposureMinBrightness = 0.55f;
        Post->EpicPreset.FilmGrainIntensity = 0.018f;
        Post->EpicPreset.MotionBlurAmount = 0.22f;
        Post->EpicPreset.LensFlareIntensity = 0.25f;
        Post->ApplyPresetForTier(EVisualQualityTier::Epic);
    }
    EnsureNightFogAndLamps();
    ApplyNightSkyFix();
    ApplyNightCityHISMGlow();
    HideSprawlBuildingHISM();
    ApplyNightGroundWetness();
    EnsureBurkeWetApron();
    QuietCesiumTilesets();
    if (GEngine)
    {
        GEngine->Exec(GetWorld(), TEXT("r.VolumetricCloud 0"));
        GEngine->Exec(GetWorld(), TEXT("r.SkyAtmosphere 1"));
        GEngine->Exec(GetWorld(), TEXT("r.BloomQuality 5"));
        GEngine->Exec(GetWorld(), TEXT("r.Tonemapper.Quality 5"));
        GEngine->Exec(GetWorld(), TEXT("r.DefaultFeature.AutoExposure 1"));
        GEngine->Exec(GetWorld(), TEXT("r.EyeAdaptationQuality 2"));
        // V15: sprawl HISMs are hidden; keep far downtown band + Karla drawable.
        GEngine->Exec(GetWorld(), TEXT("r.ViewDistanceScale 1.35"));
    }
    UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland look: MidnightRun V15 applied (no Cesium, hide T10 roof sprawl, wet apron, horizon lock)"));
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
                Sky = Spawned->GetLightComponent();
                UE_LOG(LogTemp, Warning, TEXT("raceGPS Cleveland look: spawned failsafe ASkyLight"));
            }
        }
    }
    if (Sky)
    {
        Sky->SetVisibility(true);
        if (Sky->Intensity < 1.8f)
        {
            Sky->SetIntensity(2.10f);
        }
        Sky->SetLightColor(FLinearColor(0.68f, 0.72f, 0.84f));
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
    }
}

void AClevelandLookDirector::EnsureNightFogAndLamps()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    if (!NightFog)
    {
        NightFog = Cast<AExponentialHeightFog>(
            UGameplayStatics::GetActorOfClass(World, AExponentialHeightFog::StaticClass()));
        if (!NightFog)
        {
            NightFog = World->SpawnActor<AExponentialHeightFog>(AExponentialHeightFog::StaticClass());
        }
    }
    if (NightFog)
    {
        if (UExponentialHeightFogComponent* Fog = NightFog->GetComponent())
        {
            // V15: lighter fog now that T10 roof sprawl is hidden; keep night air.
            Fog->SetFogDensity(0.0026f);
            Fog->SetFogHeightFalloff(0.16f);
            Fog->SetFogMaxOpacity(0.42f);
            Fog->SetStartDistance(1800.f);
            Fog->SetFogInscatteringColor(FLinearColor(0.06f, 0.058f, 0.07f));
            Fog->SetVolumetricFog(false);
            Fog->SetVisibility(true);
        }
    }

    if (NightLamps.Num() == 0)
    {
        // Sparse Burke trackside lamps. Runway ~WSW (geo 247) through world origin.
        const FVector Runway(-0.92f, -0.39f, 0.f);
        const FVector Side(-Runway.Y, Runway.X, 0.f);
        const FLinearColor Warm(1.0f, 0.78f, 0.48f);
        const FLinearColor Cool(0.55f, 0.72f, 1.0f);
        for (int32 i = 0; i < 16; ++i)
        {
            const float Along = (i - 7.5f) * 4200.f;
            const FVector Loc = Runway * Along + Side * 1600.f + FVector(0.f, 0.f, 780.f);
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            if (APointLight* Lamp = World->SpawnActor<APointLight>(APointLight::StaticClass(), Loc, FRotator::ZeroRotator, Params))
            {
                if (UPointLightComponent* Comp = Lamp->FindComponentByClass<UPointLightComponent>())
                {
                    Comp->SetIntensity(180000.f);
                    Comp->SetAttenuationRadius(14000.f);
                    Comp->SetLightColor((i % 2 == 0) ? Warm : Cool);
                    Comp->SetCastShadows(false);
                    Comp->SetVisibility(true);
                }
                NightLamps.Add(Lamp);
            }
        }
        // Grid key light so the 3-car pack reads at night (hero still / chase blend).
        {
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            const FVector KeyLoc(0.f, 0.f, 900.f);
            if (APointLight* Key = World->SpawnActor<APointLight>(APointLight::StaticClass(), KeyLoc, FRotator::ZeroRotator, Params))
            {
                if (UPointLightComponent* Comp = Key->FindComponentByClass<UPointLightComponent>())
                {
                    Comp->SetIntensity(240000.f);
                    Comp->SetAttenuationRadius(18000.f);
                    Comp->SetLightColor(FLinearColor(1.0f, 0.90f, 0.75f));
                    Comp->SetCastShadows(false);
                    Comp->SetVisibility(true);
                }
                NightLamps.Add(Key);
            }
        }
        // Neon strips along hangar / barrier line (south side of runway).
        for (int32 n = 0; n < 8; ++n)
        {
            const float AlongN = (n - 3.5f) * 3800.f;
            const FVector NLoc = Runway * AlongN + Side * (-2200.f) + FVector(0.f, 0.f, 420.f);
            FActorSpawnParameters NParams;
            NParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            if (APointLight* Neon = World->SpawnActor<APointLight>(APointLight::StaticClass(), NLoc, FRotator::ZeroRotator, NParams))
            {
                if (UPointLightComponent* Comp = Neon->FindComponentByClass<UPointLightComponent>())
                {
                    Comp->SetIntensity(90000.f);
                    Comp->SetAttenuationRadius(7000.f);
                    Comp->SetLightColor((n % 2 == 0) ? FLinearColor(1.0f, 0.15f, 0.55f) : FLinearColor(0.15f, 0.85f, 1.0f));
                    Comp->SetCastShadows(false);
                    Comp->SetVisibility(true);
                }
                NightLamps.Add(Neon);
            }
        }
    }
    UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland look: night fog=%s lamps=%d"),
        NightFog ? TEXT("yes") : TEXT("NO"), NightLamps.Num());
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

void AClevelandLookDirector::ApplyNightSkyFix() const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    if (Cycle)
    {
        Cycle->bUseVolumetricClouds = false;
        if (Cycle->VolumetricClouds)
        {
            Cycle->VolumetricClouds->SetVisibility(false);
            Cycle->VolumetricClouds->SetHiddenInGame(true);
        }
        if (Cycle->SkySphere)
        {
            Cycle->SkySphere->SetVisibility(false);
            Cycle->SkySphere->SetHiddenInGame(true);
        }
        if (Cycle->SkyAtmosphere)
        {
            Cycle->SkyAtmosphere->SetVisibility(true);
        }
    }
    if (GEngine)
    {
        GEngine->Exec(World, TEXT("r.VolumetricCloud 0"));
        GEngine->Exec(World, TEXT("r.SkyAtmosphere 1"));
        GEngine->Exec(World, TEXT("r.VolumetricRenderTarget 0"));
    }
    int32 HiddenClouds = 0;
    TArray<AActor*> All;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), All);
    for (AActor* A : All)
    {
        if (!A)
        {
            continue;
        }
        TArray<UVolumetricCloudComponent*> Clouds;
        A->GetComponents<UVolumetricCloudComponent>(Clouds);
        for (UVolumetricCloudComponent* C : Clouds)
        {
            if (C)
            {
                C->SetVisibility(false);
                C->SetHiddenInGame(true);
                ++HiddenClouds;
            }
        }
    }
    if (Cycle && Cycle->SkyLight)
    {
        // Freeze after clouds are gone so realtime capture cannot re-bake mottled junk.
        Cycle->SkyLight->bRealTimeCapture = false;
        Cycle->SkyLight->SourceType = ESkyLightSourceType::SLS_CapturedScene;
        Cycle->SkyLight->SetIntensity(2.10f);
        Cycle->SkyLight->SetLightColor(FLinearColor(0.68f, 0.72f, 0.84f));
        Cycle->SkyLight->RecaptureSky();
    }
    // Hide any leftover "Cloud" labeled actors (non-component volumetric leftovers).
    for (AActor* A : All)
    {
        if (!A) { continue; }
        const FString L = A->GetActorNameOrLabel();
        if (L.Contains(TEXT("VolumetricCloud"), ESearchCase::IgnoreCase)
            || L.Contains(TEXT("SkyAtmosphereCloud"), ESearchCase::IgnoreCase))
        {
            A->SetActorHiddenInGame(true);
        }
    }
    UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland look: V8 sky fix — volumetric clouds OFF (hidden comps=%d), SkyAtmosphere kept"), HiddenClouds);
}

void AClevelandLookDirector::ApplyNightCityHISMGlow() const
{
    // V11 P0: V10 painted M_NightWindow onto ANY large HISM batch (LocalInstances>=500),
    // including the T10 "Water" actor (7k+ instances). Combined with a solid full-face
    // emissive (no window mask), that read as a ceiling of glowing rectangular slabs
    // floating in the sky. Narrow to Buildings only; glass slots get tiled window MID;
    // concrete/brick get dark non-emissive night façade.
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const FLinearColor WindowWarm(1.0f, 0.82f, 0.48f);
    const FLinearColor WindowCool(0.55f, 0.75f, 1.0f);
    const float EmissiveStrGlass = 1.85f; // V15: downtown band only; more facade read
    const float WindowTile = 9.0f;
    const float AmountOffGlass = 0.38f;
    const float AmountOffFacade = 0.62f;

    auto ForceIsmUsage = [](UMaterialInterface* Base) -> UMaterialInterface*
    {
        if (!Base)
        {
            return nullptr;
        }
        if (UMaterial* Mat = Cast<UMaterial>(Base->GetMaterial()))
        {
            bool bNeeds = false;
            Mat->SetMaterialUsage(bNeeds, MATUSAGE_InstancedStaticMeshes);
            (void)bNeeds;
        }
        return Base;
    };

    auto IsExcludedLabel = [](const FString& Label) -> bool
    {
        static const TCHAR* Bad[] = {
            TEXT("Water"), TEXT("Terrain"), TEXT("Landscape"), TEXT("Ground"), TEXT("Road"),
            TEXT("Asphalt"), TEXT("Sky"), TEXT("Cloud"), TEXT("Prop"), TEXT("Tree"),
            TEXT("Vegetation"), TEXT("Foliage"), TEXT("POI_"), TEXT("Grass"), TEXT("Lake")
        };
        for (const TCHAR* B : Bad)
        {
            if (Label.Contains(B, ESearchCase::IgnoreCase))
            {
                return true;
            }
        }
        return false;
    };

    auto IsBuildingActor = [&](const FString& Label) -> bool
    {
        if (IsExcludedLabel(Label))
        {
            return false;
        }
        return Label.Equals(TEXT("Buildings"), ESearchCase::IgnoreCase)
            || Label.StartsWith(TEXT("Building"), ESearchCase::IgnoreCase)
            || (Label.Contains(TEXT("Building"), ESearchCase::IgnoreCase)
                && !Label.Contains(TEXT("Generator"), ESearchCase::IgnoreCase));
    };

    auto IsGlassMat = [](const FString& MatName, const FString& MeshName) -> bool
    {
        return MatName.Contains(TEXT("glass"), ESearchCase::IgnoreCase)
            || MatName.Contains(TEXT("window"), ESearchCase::IgnoreCase)
            || MeshName.Contains(TEXT("glass"), ESearchCase::IgnoreCase)
            || MeshName.Contains(TEXT("window"), ESearchCase::IgnoreCase);
    };

    auto MakeWindowMID = [&](int32 Salt) -> UMaterialInstanceDynamic*
    {
        UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Materials/M_NightWindow.M_NightWindow"));
        Base = ForceIsmUsage(Base);
        if (!Base)
        {
            Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Building_glass.M_Building_glass"));
            Base = ForceIsmUsage(Base);
        }
        if (!Base)
        {
            return nullptr;
        }
        UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, const_cast<AClevelandLookDirector*>(this));
        if (!Mid)
        {
            return nullptr;
        }
        const FLinearColor Win = ((Salt % 3) == 0) ? WindowCool : WindowWarm;
        // V13: do NOT bake strength into EmissiveColor (V12 double-multiply -> sheet look).
        Mid->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.03f, 0.035f, 0.045f));
        Mid->SetVectorParameterValue(TEXT("EmissiveColor"), Win);
        Mid->SetVectorParameterValue(TEXT("Emissive"), Win);
        Mid->SetVectorParameterValue(TEXT("InteriorTint"), Win);
        Mid->SetScalarParameterValue(TEXT("EmissiveStrength"), EmissiveStrGlass);
        Mid->SetScalarParameterValue(TEXT("EmissiveIntensity"), EmissiveStrGlass);
        Mid->SetScalarParameterValue(TEXT("WindowLight"), EmissiveStrGlass);
        Mid->SetScalarParameterValue(TEXT("WindowTile"), WindowTile);
        Mid->SetScalarParameterValue(TEXT("AmountOff"), AmountOffGlass);
        Mid->SetScalarParameterValue(TEXT("InteriorExposure"), 1.15f);
        Mid->SetScalarParameterValue(TEXT("InteriorDepth"), 0.55f);
        Mid->SetScalarParameterValue(TEXT("LumaVariation"), 0.32f);
        Mid->SetScalarParameterValue(TEXT("WindowSeed"), float(Salt % 97) * 0.01f);
        Mid->SetScalarParameterValue(TEXT("Roughness"), 0.28f);
        Mid->SetScalarParameterValue(TEXT("Metallic"), 0.35f);
        return Mid;
    };

    auto MakeFacadeMID = [&](UMaterialInterface* Prefer) -> UMaterialInstanceDynamic*
    {
        UMaterialInterface* Base = Prefer;
        if (!Base)
        {
            Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Building_concrete.M_Building_concrete"));
        }
        Base = ForceIsmUsage(Base);
        if (!Base)
        {
            Base = UMaterial::GetDefaultMaterial(MD_Surface);
        }
        UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, const_cast<AClevelandLookDirector*>(this));
        if (!Mid)
        {
            return nullptr;
        }
        const FLinearColor BaseCol(0.07f, 0.068f, 0.065f);
        Mid->SetVectorParameterValue(TEXT("BaseColor"), BaseCol);
        Mid->SetVectorParameterValue(TEXT("Base_color"), BaseCol);
        Mid->SetVectorParameterValue(TEXT("Color"), BaseCol);
        Mid->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor::Black);
        Mid->SetVectorParameterValue(TEXT("Emissive"), FLinearColor::Black);
        Mid->SetScalarParameterValue(TEXT("EmissiveStrength"), 0.f);
        Mid->SetScalarParameterValue(TEXT("EmissiveIntensity"), 0.f);
        Mid->SetScalarParameterValue(TEXT("WindowLight"), 0.f);
        Mid->SetScalarParameterValue(TEXT("Roughness"), 0.78f);
        Mid->SetScalarParameterValue(TEXT("Metallic"), 0.02f);
        return Mid;
    };

    int32 HismComps = 0;
    int32 Instances = 0;
    int32 GlassSlots = 0;
    int32 FacadeSlots = 0;
    int32 SkippedActors = 0;
    TArray<AActor*> Actors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), Actors);
    for (AActor* A : Actors)
    {
        if (!A)
        {
            continue;
        }
        const FString Label = A->GetActorNameOrLabel();
        TArray<UHierarchicalInstancedStaticMeshComponent*> HISMs;
        A->GetComponents<UHierarchicalInstancedStaticMeshComponent>(HISMs);
        if (HISMs.Num() == 0)
        {
            continue;
        }
        if (!IsBuildingActor(Label))
        {
            ++SkippedActors;
            continue;
        }
        for (UHierarchicalInstancedStaticMeshComponent* H : HISMs)
        {
            if (!H || H->GetInstanceCount() == 0)
            {
                continue;
            }
            const FString MeshName = H->GetStaticMesh() ? H->GetStaticMesh()->GetName() : FString();
            // Extra mesh-level guard: skip water/terrain proxies that somehow live under Buildings.
            if (MeshName.Contains(TEXT("Water"), ESearchCase::IgnoreCase)
                || MeshName.Contains(TEXT("Ocean"), ESearchCase::IgnoreCase)
                || MeshName.Contains(TEXT("Landscape"), ESearchCase::IgnoreCase)
                || MeshName.Contains(TEXT("River"), ESearchCase::IgnoreCase))
            {
                continue;
            }
            ++HismComps;
            Instances += H->GetInstanceCount();
            const int32 NumMats = H->GetNumMaterials();
            for (int32 Mi = 0; Mi < FMath::Max(NumMats, 1); ++Mi)
            {
                UMaterialInterface* Base = (NumMats > 0) ? H->GetMaterial(Mi) : nullptr;
                const FString MatName = Base ? Base->GetName() : FString();
                // V11.1: tiled window MASK is safe on all building slots (no solid slabs).
                // Glass batches get fuller intensity; brick/concrete get dimmer office-window read.
                const bool bGlass = IsGlassMat(MatName, MeshName);
                if (UMaterialInstanceDynamic* Mid = MakeWindowMID(Mi + HismComps))
                {
                    const float Str = bGlass ? 1.90f : 0.55f; // V15: readable downtown facades after roof hide
                    const FLinearColor Win = (((Mi + HismComps) % 3) == 0)
                        ? FLinearColor(0.55f, 0.75f, 1.0f) : FLinearColor(1.0f, 0.82f, 0.48f);
                    Mid->SetVectorParameterValue(TEXT("EmissiveColor"), Win);
                    Mid->SetVectorParameterValue(TEXT("InteriorTint"), Win);
                    Mid->SetScalarParameterValue(TEXT("EmissiveStrength"), Str);
                    Mid->SetScalarParameterValue(TEXT("WindowTile"), bGlass ? 11.0f : 7.0f);
                    Mid->SetScalarParameterValue(TEXT("AmountOff"), bGlass ? AmountOffGlass : AmountOffFacade);
                    Mid->SetScalarParameterValue(TEXT("InteriorExposure"), bGlass ? 1.25f : 0.95f);
                    Mid->SetScalarParameterValue(TEXT("InteriorDepth"), 0.55f);
                    Mid->SetScalarParameterValue(TEXT("WindowSeed"), float((Mi + HismComps) % 97) * 0.01f);
                    Mid->SetVectorParameterValue(TEXT("BaseColor"),
                        bGlass ? FLinearColor(0.030f, 0.034f, 0.042f) : FLinearColor(0.055f, 0.052f, 0.048f));
                    H->SetMaterial(Mi, Mid);
                    if (bGlass) { ++GlassSlots; } else { ++FacadeSlots; }
                }
            }
            H->SetCullDistances(0.f, 900000.f); // V15: keep far downtown band drawable; sprawl is hidden next
            H->MarkRenderStateDirty();
        }
    }
    UE_LOG(LogTemp, Warning,
        TEXT("raceGPS Cleveland look: V15 CitySample windows (pre-hide) - comps=%d instances=%d glass=%d facade=%d skippedActors=%d"),
        HismComps, Instances, GlassSlots, FacadeSlots, SkippedActors);

    // Also quiet any Water HISM that V10 may have permanently dirtied in-session (PIE-safe).
    for (AActor* A : Actors)
    {
        if (!A)
        {
            continue;
        }
        const FString Label = A->GetActorNameOrLabel();
        if (!Label.Equals(TEXT("Water"), ESearchCase::IgnoreCase)
            && !Label.Contains(TEXT("Water"), ESearchCase::IgnoreCase))
        {
            continue;
        }
        TArray<UHierarchicalInstancedStaticMeshComponent*> HISMs;
        A->GetComponents<UHierarchicalInstancedStaticMeshComponent>(HISMs);
        UMaterialInterface* WaterMat = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Materials/M_Water_Blue.M_Water_Blue"));
        for (UHierarchicalInstancedStaticMeshComponent* H : HISMs)
        {
            if (!H)
            {
                continue;
            }
            const int32 NumMats = FMath::Max(H->GetNumMaterials(), 1);
            for (int32 Mi = 0; Mi < NumMats; ++Mi)
            {
                if (WaterMat)
                {
                    if (UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(WaterMat, const_cast<AClevelandLookDirector*>(this)))
                    {
                        Mid->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.01f, 0.03f, 0.06f));
                        Mid->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor::Black);
                        Mid->SetScalarParameterValue(TEXT("EmissiveStrength"), 0.f);
                        Mid->SetScalarParameterValue(TEXT("Roughness"), 0.15f);
                        Mid->SetScalarParameterValue(TEXT("Specular"), 0.8f);
                        H->SetMaterial(Mi, Mid);
                    }
                }
            }
            H->MarkRenderStateDirty();
        }
    }
}

void AClevelandLookDirector::ApplyNightGroundWetness() const
{
    // V11: runway/taxiway shouldn't read as flat navy void — dark wet asphalt + markings.
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    if (AClevelandEnvironmentActor* Env = Cast<AClevelandEnvironmentActor>(
            UGameplayStatics::GetActorOfClass(World, AClevelandEnvironmentActor::StaticClass())))
    {
        Env->ApplyNightGroundMaterials();
    }
    // T10 Terrain actor: nudge any PMC sections toward dark asphalt (no emissive).
    int32 TerrainSections = 0;
    TArray<AActor*> Actors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), Actors);
    UMaterialInterface* Asphalt = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Materials/M_NightAsphalt.M_NightAsphalt"));
    if (!Asphalt)
    {
        Asphalt = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Materials/M_Master_Road_Asphalt.M_Master_Road_Asphalt"));
    }
    for (AActor* A : Actors)
    {
        if (!A)
        {
            continue;
        }
        const FString Label = A->GetActorNameOrLabel();
        if (!Label.Equals(TEXT("Terrain"), ESearchCase::IgnoreCase)
            && !Label.Contains(TEXT("Terrain"), ESearchCase::IgnoreCase)
            && !Label.Contains(TEXT("Ground"), ESearchCase::IgnoreCase))
        {
            continue;
        }
        TArray<UPrimitiveComponent*> PrimComps;
        A->GetComponents<UPrimitiveComponent>(PrimComps);
        for (UPrimitiveComponent* P : PrimComps)
        {
            if (!P || !Asphalt)
            {
                continue;
            }
            const int32 Num = FMath::Max(P->GetNumMaterials(), 1);
            for (int32 Mi = 0; Mi < Num; ++Mi)
            {
                if (UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Asphalt, const_cast<AClevelandLookDirector*>(this)))
                {
                    Mid->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.125f, 0.112f, 0.095f));
                    Mid->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.125f, 0.112f, 0.095f));
                    Mid->SetScalarParameterValue(TEXT("Roughness"), 0.24f);
                    Mid->SetScalarParameterValue(TEXT("Specular"), 0.80f);
                    Mid->SetScalarParameterValue(TEXT("Metallic"), 0.05f);
                    Mid->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor::Black);
                    P->SetMaterial(Mi, Mid);
                    ++TerrainSections;
                }
            }
        }
    }
    UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland look: V15 night ground wetness - terrainSections=%d"), TerrainSections);
}


void AClevelandLookDirector::QuietCesiumTilesets() const
{
    // Chris paused ion Connect. Plugin may stay enabled; do not tick/load tilesets (401 spam).
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    UClass* TilesetClass = LoadClass<AActor>(nullptr, TEXT("/Script/CesiumRuntime.Cesium3DTileset"));
    if (!TilesetClass)
    {
        UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland look: V15 Cesium quiet - plugin class missing (ok)"));
        return;
    }
    TArray<AActor*> Tilesets;
    UGameplayStatics::GetAllActorsOfClass(World, TilesetClass, Tilesets);
    int32 Quieted = 0;
    for (AActor* T : Tilesets)
    {
        if (!T)
        {
            continue;
        }
        T->SetActorTickEnabled(false);
        T->SetActorHiddenInGame(true);
        T->SetActorEnableCollision(false);
        if (FBoolProperty* Sus = FindFProperty<FBoolProperty>(T->GetClass(), TEXT("SuspendUpdate")))
        {
            Sus->SetPropertyValue_InContainer(T, true);
        }
        TArray<UActorComponent*> Comps;
        T->GetComponents(Comps);
        for (UActorComponent* C : Comps)
        {
            if (C)
            {
                C->SetComponentTickEnabled(false);
            }
        }
        ++Quieted;
    }
    UE_LOG(LogTemp, Warning, TEXT("raceGPS Cleveland look: V15 Cesium tilesets quieted=%d (no ion load)"), Quieted);
}

void AClevelandLookDirector::EnsureBurkeWetApron()
{
    UWorld* World = GetWorld();
    if (!World || BurkeWetApron)
    {
        return;
    }
    UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
    if (!Plane)
    {
        UE_LOG(LogTemp, Warning, TEXT("raceGPS Cleveland look: V15 wet apron skipped (Engine Plane missing)"));
        return;
    }
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.Name = TEXT("ClevelandShowcaseWetApron");
    Params.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
    BurkeWetApron = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FVector(0.f, -2000.f, 3.f), FRotator::ZeroRotator, Params);
    if (!BurkeWetApron)
    {
        return;
    }
    BurkeWetApron->SetActorScale3D(FVector(1600.f, 2200.f, 1.f)); // ~1.6x2.2km runway apron, not a mirror lake
    if (UStaticMeshComponent* Mesh = BurkeWetApron->GetStaticMeshComponent())
    {
        Mesh->SetStaticMesh(Plane);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetCastShadow(false);
        Mesh->SetMobility(EComponentMobility::Movable);
        UMaterialInterface* Asphalt = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_NightAsphalt.M_NightAsphalt"));
        if (!Asphalt)
        {
            Asphalt = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Master_Road_Asphalt.M_Master_Road_Asphalt"));
        }
        if (Asphalt)
        {
            if (UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Asphalt, this))
            {
                const FLinearColor Wet(0.145f, 0.132f, 0.112f);
                Mid->SetVectorParameterValue(TEXT("BaseColor"), Wet);
                Mid->SetVectorParameterValue(TEXT("Color"), Wet);
                Mid->SetVectorParameterValue(TEXT("Tint"), Wet);
                Mid->SetScalarParameterValue(TEXT("Roughness"), 0.35f);
                Mid->SetScalarParameterValue(TEXT("Specular"), 0.55f);
                Mid->SetScalarParameterValue(TEXT("Metallic"), 0.02f);
                Mid->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor::Black);
                Mesh->SetMaterial(0, Mid);
            }
        }
        Mesh->MarkRenderStateDirty();
    }
    BurkeWetApron->SetActorHiddenInGame(false);
    UE_LOG(LogTemp, Warning, TEXT("raceGPS Cleveland look: V15 Burke wet apron spawned (1.6x2.2km M_NightAsphalt, matte-wet)"));
}

void AClevelandLookDirector::HideSprawlBuildingHISM()
{
    // Showcase-only: hide T10 Building HISM sprawl that silhouettes as an overhead roof
    // cloud. Do not delete citypack/T10 data. Keep Karla named towers + a south
    // downtown band copied onto a transient actor.
    if (bSprawlHidden)
    {
        return;
    }
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    auto IsExcludedLabel = [](const FString& Label) -> bool
    {
        static const TCHAR* Bad[] = {
            TEXT("Water"), TEXT("Terrain"), TEXT("Landscape"), TEXT("Ground"), TEXT("Road"),
            TEXT("Asphalt"), TEXT("Sky"), TEXT("Cloud"), TEXT("Prop"), TEXT("Tree"),
            TEXT("Vegetation"), TEXT("Foliage"), TEXT("POI_"), TEXT("Grass"), TEXT("Lake"),
            TEXT("Karla"), TEXT("Generator"), TEXT("DowntownBand"), TEXT("ClevelandEnvironment"),
            TEXT("NamedTower"), TEXT("Hangar"), TEXT("Barrier"), TEXT("Cone")
        };
        for (const TCHAR* B : Bad)
        {
            if (Label.Contains(B, ESearchCase::IgnoreCase))
            {
                return true;
            }
        }
        return false;
    };

    auto IsBuildingActor = [&](const FString& Label) -> bool
    {
        if (IsExcludedLabel(Label))
        {
            return false;
        }
        return Label.Equals(TEXT("Buildings"), ESearchCase::IgnoreCase)
            || Label.StartsWith(TEXT("Building"), ESearchCase::IgnoreCase)
            || Label.Contains(TEXT("Building"), ESearchCase::IgnoreCase);
    };

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    DowntownBandActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (DowntownBandActor)
    {
        DowntownBandActor->SetActorTickEnabled(false);
        if (!DowntownBandActor->GetRootComponent())
        {
            USceneComponent* Root = NewObject<USceneComponent>(DowntownBandActor, TEXT("BandRoot"));
            Root->RegisterComponent();
            DowntownBandActor->SetRootComponent(Root);
        }
    }

    int32 HiddenComps = 0;
    int32 HiddenInst = 0;
    int32 KeptInst = 0;
    int32 BandComps = 0;
    TArray<AActor*> Actors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), Actors);
    for (AActor* A : Actors)
    {
        if (!A || A == DowntownBandActor)
        {
            continue;
        }
        const FString Label = A->GetActorNameOrLabel();
        TArray<UHierarchicalInstancedStaticMeshComponent*> HISMs;
        A->GetComponents<UHierarchicalInstancedStaticMeshComponent>(HISMs);
        if (HISMs.Num() == 0 || !IsBuildingActor(Label))
        {
            continue;
        }
        for (UHierarchicalInstancedStaticMeshComponent* H : HISMs)
        {
            if (!H || H->GetInstanceCount() == 0)
            {
                continue;
            }
            const int32 N = H->GetInstanceCount();
            float MeshHt = 400.f;
            if (UStaticMesh* SM = H->GetStaticMesh())
            {
                MeshHt = SM->GetBounds().BoxExtent.Z * 2.f;
            }
            TArray<FTransform> Keep;
            Keep.Reserve(64);
            for (int32 i = 0; i < N; ++i)
            {
                FTransform XF;
                H->GetInstanceTransform(i, XF, true);
                const FVector L = XF.GetLocation();
                const float WorldHt = MeshHt * FMath::Abs(XF.GetScale3D().Z);
                // Geo: Y=north. Downtown sits south of Burke (negative Y).
                // Keep only far south downtown / tall towers so midfield roofs cannot fill upper frustum.
                const bool bSouthBand = (L.Y < -240000.f && L.Y > -520000.f && FMath::Abs(L.X) < 180000.f);
                const bool bTallSkyline = (WorldHt > 18000.f && L.Y < -180000.f && L.Y > -520000.f && FMath::Abs(L.X) < 200000.f);
                if (bSouthBand || bTallSkyline)
                {
                    Keep.Add(XF);
                }
            }
            HiddenInst += (N - Keep.Num());
            KeptInst += Keep.Num();

            H->SetHiddenInGame(true);
            H->SetVisibility(false);
            H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            ++HiddenComps;

            if (DowntownBandActor && Keep.Num() > 0 && H->GetStaticMesh())
            {
                const FName CompName(*FString::Printf(TEXT("Band_%d"), BandComps));
                UHierarchicalInstancedStaticMeshComponent* Copy =
                    NewObject<UHierarchicalInstancedStaticMeshComponent>(DowntownBandActor, CompName);
                if (!Copy)
                {
                    continue;
                }
                Copy->SetStaticMesh(H->GetStaticMesh());
                Copy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                Copy->SetMobility(EComponentMobility::Static);
                Copy->SetCullDistances(0.f, 900000.f);
                const int32 NumMats = H->GetNumMaterials();
                for (int32 Mi = 0; Mi < NumMats; ++Mi)
                {
                    Copy->SetMaterial(Mi, H->GetMaterial(Mi));
                }
                Copy->SetupAttachment(DowntownBandActor->GetRootComponent());
                Copy->RegisterComponent();
                DowntownBandActor->AddInstanceComponent(Copy);
                Copy->AddInstances(Keep, false, true);
                Copy->SetHiddenInGame(false);
                Copy->SetVisibility(true);
                Copy->MarkRenderStateDirty();
                ++BandComps;
            }
        }
    }
    bSprawlHidden = true;
    UE_LOG(LogTemp, Warning,
        TEXT("raceGPS Cleveland look: V15 hide T10 roof sprawl hiddenComps=%d hiddenInst=%d keptSouthBandInst=%d bandComps=%d (Karla/dressing untouched)"),
        HiddenComps, HiddenInst, KeptInst, BandComps);
}
