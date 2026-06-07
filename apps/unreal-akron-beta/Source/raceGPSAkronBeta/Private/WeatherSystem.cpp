// Copyright raceGPS. All Rights Reserved.

#include "WeatherSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Kismet/GameplayStatics.h"

AWeatherSystem::AWeatherSystem()
{
    PrimaryActorTick.bCanEverTick = true;

    RainParticles = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("RainParticles"));
    SnowParticles = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("SnowParticles"));
    FogComponent = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("FogComponent"));

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RainParticles->SetupAttachment(RootComponent);
    SnowParticles->SetupAttachment(RootComponent);
    FogComponent->SetupAttachment(RootComponent);
}

void AWeatherSystem::BeginPlay()
{
    Super::BeginPlay();
    if (bAutoCycle)
    {
        GetWorld()->GetTimerManager().SetTimer(CycleTimer, this, &AWeatherSystem::OnCycleTimer, CycleIntervalSeconds, true);
    }
}

void AWeatherSystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    UpdateEffects(DeltaTime);
}

void AWeatherSystem::SetWeather(EWeatherType NewWeather)
{
    if (CurrentWeather == NewWeather) return;
    TargetWeather = NewWeather;
    TransitionAlpha = 0.0f;
    UE_LOG(LogTemp, Log, TEXT("Weather changing to %d"), (int32)NewWeather);
}

EWeatherType AWeatherSystem::GetRandomWeather() const
{
    int32 Idx = FMath::RandRange(0, 6);
    return static_cast<EWeatherType>(Idx);
}

float AWeatherSystem::GetTractionModifier() const
{
    switch (CurrentWeather)
    {
    case EWeatherType::Clear: return 1.0f;
    case EWeatherType::Cloudy: return 0.95f;
    case EWeatherType::Rain: return 0.75f;
    case EWeatherType::HeavyRain: return 0.55f;
    case EWeatherType::Fog: return 0.85f;
    case EWeatherType::Snow: return 0.4f;
    case EWeatherType::Storm: return 0.35f;
    default: return 1.0f;
    }
}

void AWeatherSystem::OnCycleTimer()
{
    SetWeather(GetRandomWeather());
}

void AWeatherSystem::UpdateEffects(float DeltaTime)
{
    if (TransitionAlpha < 1.0f)
    {
        TransitionAlpha += DeltaTime / FMath::Max(TransitionDurationSeconds, 0.1f);
        TransitionAlpha = FMath::Clamp(TransitionAlpha, 0.0f, 1.0f);
    }

    // Target state
    float TargetWetness = 0.0f;
    float TargetVisibility = 10000.0f;
    bool bRain = false;
    bool bSnow = false;

    switch (TargetWeather)
    {
    case EWeatherType::Clear:
        TargetWetness = 0.0f;
        TargetVisibility = 10000.0f;
        break;
    case EWeatherType::Cloudy:
        TargetWetness = 0.0f;
        TargetVisibility = 6000.0f;
        break;
    case EWeatherType::Rain:
        TargetWetness = 0.5f;
        TargetVisibility = 4000.0f;
        bRain = true;
        break;
    case EWeatherType::HeavyRain:
        TargetWetness = 0.9f;
        TargetVisibility = 1500.0f;
        bRain = true;
        break;
    case EWeatherType::Fog:
        TargetWetness = 0.2f;
        TargetVisibility = 500.0f;
        break;
    case EWeatherType::Snow:
        TargetWetness = 0.3f;
        TargetVisibility = 2000.0f;
        bSnow = true;
        break;
    case EWeatherType::Storm:
        TargetWetness = 1.0f;
        TargetVisibility = 800.0f;
        bRain = true;
        break;
    }

    RoadWetness = FMath::Lerp(RoadWetness, TargetWetness, TransitionAlpha);
    VisibilityMeters = FMath::Lerp(VisibilityMeters, TargetVisibility, TransitionAlpha);

    if (FogComponent)
    {
        FogComponent->SetVisibility(VisibilityMeters < 5000.0f);
        FogComponent->FogDensity = FMath::GetMappedRangeValueClamped(FVector2D(500.0f, 5000.0f), FVector2D(0.1f, 0.0f), VisibilityMeters);
    }

    if (RainParticles)
    {
        RainParticles->SetVisibility(bRain);
    }
    if (SnowParticles)
    {
        SnowParticles->SetVisibility(bSnow);
    }

    if (TransitionAlpha >= 1.0f)
    {
        CurrentWeather = TargetWeather;
    }
}
