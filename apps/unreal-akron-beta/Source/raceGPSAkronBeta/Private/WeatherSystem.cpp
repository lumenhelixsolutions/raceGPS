// Copyright raceGPS. All Rights Reserved.

#include "WeatherSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
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

    // Niagara placeholders — will be spawned at BeginPlay if assets exist
    NiagaraRain = nullptr;
    NiagaraSnow = nullptr;
    NiagaraDust = nullptr;
    NiagaraStorm = nullptr;

    // Default soft references for Niagara assets
    NiagaraRainAsset = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/VFX/Weather/NS_Rain.NS_Rain")));
    NiagaraSnowAsset = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/VFX/Weather/NS_Snow.NS_Snow")));
    NiagaraDustAsset = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/VFX/Weather/NS_Dust.NS_Dust")));
    NiagaraStormAsset = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/VFX/Weather/NS_Storm.NS_Storm")));
}

void AWeatherSystem::BeginPlay()
{
    Super::BeginPlay();
    SpawnNiagaraIfNeeded();

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

    // Cascade fallback
    if (RainParticles)
    {
        RainParticles->SetVisibility(bRain);
    }
    if (SnowParticles)
    {
        SnowParticles->SetVisibility(bSnow);
    }

    // Niagara primary
    UpdateNiagaraEffects(TargetWeather);

    if (TransitionAlpha >= 1.0f)
    {
        CurrentWeather = TargetWeather;
    }
}

void AWeatherSystem::UpdateNiagaraEffects(EWeatherType Weather)
{
    auto SetNiagaraActive = [this](UNiagaraComponent* Comp, bool bActive)
    {
        if (!Comp) return;
        Comp->SetVisibility(bActive);
        if (bActive && !Comp->IsActive())
        {
            Comp->Activate();
        }
        else if (!bActive && Comp->IsActive())
        {
            Comp->Deactivate();
        }
    };

    switch (Weather)
    {
    case EWeatherType::Clear:
        SetNiagaraActive(NiagaraRain, false);
        SetNiagaraActive(NiagaraSnow, false);
        SetNiagaraActive(NiagaraDust, false);
        SetNiagaraActive(NiagaraStorm, false);
        break;
    case EWeatherType::Cloudy:
        SetNiagaraActive(NiagaraRain, false);
        SetNiagaraActive(NiagaraSnow, false);
        SetNiagaraActive(NiagaraDust, true);  // Light dust/pollen
        SetNiagaraActive(NiagaraStorm, false);
        break;
    case EWeatherType::Rain:
        SetNiagaraActive(NiagaraRain, true);
        SetNiagaraActive(NiagaraSnow, false);
        SetNiagaraActive(NiagaraDust, false);
        SetNiagaraActive(NiagaraStorm, false);
        break;
    case EWeatherType::HeavyRain:
        SetNiagaraActive(NiagaraRain, true);
        SetNiagaraActive(NiagaraSnow, false);
        SetNiagaraActive(NiagaraDust, false);
        SetNiagaraActive(NiagaraStorm, false);
        break;
    case EWeatherType::Fog:
        SetNiagaraActive(NiagaraRain, false);
        SetNiagaraActive(NiagaraSnow, false);
        SetNiagaraActive(NiagaraDust, false);
        SetNiagaraActive(NiagaraStorm, false);
        break;
    case EWeatherType::Snow:
        SetNiagaraActive(NiagaraRain, false);
        SetNiagaraActive(NiagaraSnow, true);
        SetNiagaraActive(NiagaraDust, false);
        SetNiagaraActive(NiagaraStorm, false);
        break;
    case EWeatherType::Storm:
        SetNiagaraActive(NiagaraRain, false);
        SetNiagaraActive(NiagaraSnow, false);
        SetNiagaraActive(NiagaraDust, false);
        SetNiagaraActive(NiagaraStorm, true);
        break;
    }
}

void AWeatherSystem::SpawnNiagaraIfNeeded()
{
    auto TrySpawn = [this](TSoftObjectPtr<UNiagaraSystem> Asset, UNiagaraComponent*& OutComp, const TCHAR* Name)
    {
        if (Asset.IsValid())
        {
            UNiagaraSystem* System = Asset.LoadSynchronous();
            if (System)
            {
                OutComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
                    System, RootComponent, NAME_None,
                    FVector::ZeroVector, FRotator::ZeroRotator,
                    EAttachLocation::KeepRelativeOffset,
                    false); // Auto destroy false — we manage lifetime
                if (OutComp)
                {
                    OutComp->SetVisibility(false);
                    OutComp->Deactivate();
                }
            }
        }
    };

    TrySpawn(NiagaraRainAsset, NiagaraRain, TEXT("NiagaraRain"));
    TrySpawn(NiagaraSnowAsset, NiagaraSnow, TEXT("NiagaraSnow"));
    TrySpawn(NiagaraDustAsset, NiagaraDust, TEXT("NiagaraDust"));
    TrySpawn(NiagaraStormAsset, NiagaraStorm, TEXT("NiagaraStorm"));
}
