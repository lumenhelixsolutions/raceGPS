#include "DayNightCycle.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"

ADayNightCycle::ADayNightCycle(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;

    SunLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("SunLight"));
    SunLight->SetMobility(EComponentMobility::Movable);
    SunLight->Intensity = 2.5f;
    SunLight->LightColor = FColor::White;
    SunLight->bUsedAsAtmosphereSunLight = true;
    SunLight->AtmosphereSunLightIndex = 0;
    RootComponent = SunLight;

    SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
    SkyLight->SetMobility(EComponentMobility::Movable);
    SkyLight->bRealTimeCapture = true;
    SkyLight->SetupAttachment(RootComponent);

    SkySphere = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SkySphere"));
    SkySphere->SetMobility(EComponentMobility::Static);
    SkySphere->SetupAttachment(RootComponent);
    SkySphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMesh.Succeeded())
    {
        SkySphere->SetStaticMesh(SphereMesh.Object);
        SkySphere->SetRelativeScale3D(FVector(10000.0f, 10000.0f, 10000.0f));
    }

    // UE5 SkyAtmosphere — only created if engine supports it
    SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
    if (SkyAtmosphere)
    {
        SkyAtmosphere->SetupAttachment(RootComponent);
        SkyAtmosphere->SetMobility(EComponentMobility::Static);
        SkyAtmosphere->TransformMode = ESkyAtmosphereTransformMode::PlanetTopAtmosphereAbsolute;
    }

    // Volumetric Clouds — only created if engine supports it
    VolumetricClouds = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("VolumetricClouds"));
    if (VolumetricClouds)
    {
        VolumetricClouds->SetupAttachment(RootComponent);
        VolumetricClouds->SetMobility(EComponentMobility::Static);
        VolumetricClouds->bUsePerSampleAtmosphericLightTransmittance = true;
    }
}

void ADayNightCycle::BeginPlay()
{
    Super::BeginPlay();
    CurrentTimeOfDay = StartTimeOfDay;
    UpdateSunRotation();
    UpdateSkyColor();
    UpdateSkyAtmosphere();
    UpdateVolumetricClouds();

    // Apply HDRI if set
    if (SkyLight && HDRIEnvironmentMap.IsValid())
    {
        UTexture2D* HDRI = HDRIEnvironmentMap.LoadSynchronous();
        if (HDRI)
        {
            SkyLight->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap;
            SkyLight->Cubemap = HDRI;
            SkyLight->UpdateCaptureSkyLight();
        }
    }
}

void ADayNightCycle::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bPaused)
        return;

    float RealSecondsPerGameDay = DayLengthMinutes * 60.0f;
    float GameHourPerSecond = 24.0f / RealSecondsPerGameDay;
    CurrentTimeOfDay += GameHourPerSecond * DeltaTime * TimeScale;

    if (CurrentTimeOfDay >= 24.0f)
    {
        CurrentTimeOfDay -= 24.0f;
    }

    UpdateSunRotation();
    UpdateSkyColor();
    UpdateSkyAtmosphere();
    UpdateVolumetricClouds();
}

void ADayNightCycle::SetTimeOfDay(float Hour)
{
    CurrentTimeOfDay = FMath::Fmod(Hour, 24.0f);
    UpdateSunRotation();
    UpdateSkyColor();
    UpdateSkyAtmosphere();
    UpdateVolumetricClouds();
}

FString ADayNightCycle::GetTimeString() const
{
    int32 Hours = FMath::FloorToInt(CurrentTimeOfDay);
    int32 Minutes = FMath::FloorToInt((CurrentTimeOfDay - Hours) * 60.0f);
    return FString::Printf(TEXT("%02d:%02d"), Hours, Minutes);
}

void ADayNightCycle::UpdateSunRotation()
{
    DayProgress = CurrentTimeOfDay / 24.0f;
    float SunAngle = (DayProgress - 0.25f) * 360.0f; // Sunrise at 6:00 (0.25)

    FRotator SunRot;
    SunRot.Pitch = -FMath::Sin(FMath::DegreesToRadians(SunAngle)) * 80.0f;
    SunRot.Yaw = SunAngle + 90.0f;
    SunRot.Roll = 0.0f;

    SunLight->SetWorldRotation(SunRot);

    // Adjust intensity based on time
    float DayIntensity = 2.5f;
    float NightIntensity = 0.05f;
    float Intensity = IsDaytime() ? DayIntensity : NightIntensity;
    SunLight->SetIntensity(FMath::Lerp(SunLight->Intensity, Intensity, 0.1f));

    // Update SkyAtmosphere sun disc
    if (SkyAtmosphere && bUseSkyAtmosphere)
    {
        SkyAtmosphere->SetTickGroup(TG_DuringPhysics);
    }
}

void ADayNightCycle::UpdateSkyColor()
{
    FLinearColor SkyColor = GetSkyColor(CurrentTimeOfDay);

    UMaterialInstanceDynamic* DynMat = SkySphere->CreateAndSetMaterialInstanceDynamic(0);
    if (DynMat)
    {
        DynMat->SetVectorParameterValue(TEXT("SkyColor"), SkyColor);
    }

    SkyLight->SetLightColor(SkyColor.ToFColor(true));
}

void ADayNightCycle::UpdateSkyAtmosphere()
{
    if (!SkyAtmosphere || !bUseSkyAtmosphere)
        return;

    // SkyAtmosphere auto-updates from directional light, but we can
    // tweak scattering properties based on time of day for richer colors
    float Elevation = GetSunElevation();
    float SunsetFactor = FMath::Clamp(1.0f - FMath::Abs(Elevation) / 15.0f, 0.0f, 1.0f);

    // Orange/red tint at sunrise/sunset
    FLinearColor SunsetTint = FLinearColor(1.0f, 0.7f, 0.4f);
    FLinearColor DayTint = FLinearColor(1.0f, 1.0f, 1.0f);
    FLinearColor Tint = FLinearColor::LerpUsingHSV(DayTint, SunsetTint, SunsetFactor);

    // Subtle adjustment to ground albedo color for bounce light
    SkyAtmosphere->GroundAlbedo = Tint;
}

void ADayNightCycle::UpdateVolumetricClouds()
{
    if (!VolumetricClouds || !bUseVolumetricClouds)
        return;

    // Adjust cloud density/opacity based on time
    float Elevation = GetSunElevation();
    float NightOpacity = 0.3f;
    float DayOpacity = 0.8f;
    float Opacity = FMath::Lerp(NightOpacity, DayOpacity, FMath::Clamp((Elevation + 10.0f) / 20.0f, 0.0f, 1.0f));

    // Scale cloud coverage using the altitude offset
    VolumetricClouds->LayerBottomAltitude = 5.0f;
    VolumetricClouds->LayerHeight = 8.0f;
}

FLinearColor ADayNightCycle::GetSkyColor(float Hour) const
{
    if (Hour >= 6.0f && Hour < 8.0f)
    {
        float T = (Hour - 6.0f) / 2.0f;
        return FLinearColor::LerpUsingHSV(FLinearColor(0.05f, 0.05f, 0.2f), FLinearColor(0.8f, 0.4f, 0.2f), T);
    }
    if (Hour >= 8.0f && Hour < 10.0f)
    {
        float T = (Hour - 8.0f) / 2.0f;
        return FLinearColor::LerpUsingHSV(FLinearColor(0.8f, 0.4f, 0.2f), FLinearColor(0.5f, 0.7f, 1.0f), T);
    }
    if (Hour >= 10.0f && Hour < 16.0f)
    {
        return FLinearColor(0.5f, 0.7f, 1.0f);
    }
    if (Hour >= 16.0f && Hour < 18.0f)
    {
        float T = (Hour - 16.0f) / 2.0f;
        return FLinearColor::LerpUsingHSV(FLinearColor(0.5f, 0.7f, 1.0f), FLinearColor(0.8f, 0.4f, 0.2f), T);
    }
    if (Hour >= 18.0f && Hour < 20.0f)
    {
        float T = (Hour - 18.0f) / 2.0f;
        return FLinearColor::LerpUsingHSV(FLinearColor(0.8f, 0.4f, 0.2f), FLinearColor(0.02f, 0.02f, 0.1f), T);
    }
    return FLinearColor(0.02f, 0.02f, 0.1f);
}

float ADayNightCycle::GetSunElevation() const
{
    // Return approximate sun elevation in degrees
    float SunAngle = (DayProgress - 0.25f) * 360.0f;
    return FMath::Sin(FMath::DegreesToRadians(SunAngle)) * 80.0f;
}
