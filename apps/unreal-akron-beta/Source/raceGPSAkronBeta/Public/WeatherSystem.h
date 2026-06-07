// Copyright raceGPS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeatherSystem.generated.h"

UENUM(BlueprintType)
enum class EWeatherType : uint8
{
    Clear       UMETA(DisplayName = "Clear"),
    Cloudy      UMETA(DisplayName = "Cloudy"),
    Rain        UMETA(DisplayName = "Rain"),
    HeavyRain   UMETA(DisplayName = "Heavy Rain"),
    Fog         UMETA(DisplayName = "Fog"),
    Snow        UMETA(DisplayName = "Snow"),
    Storm       UMETA(DisplayName = "Thunderstorm"),
};

/**
 * Dynamic weather system with particle effects, road wetness, and visibility.
 */
UCLASS()
class RACEGPSAKRONBETA_API AWeatherSystem : public AActor
{
    GENERATED_BODY()

public:
    AWeatherSystem();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
    EWeatherType CurrentWeather = EWeatherType::Clear;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
    float TransitionDurationSeconds = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
    bool bAutoCycle = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
    float CycleIntervalSeconds = 120.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Weather")
    float RoadWetness = 0.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Weather")
    float VisibilityMeters = 10000.0f;

    UFUNCTION(BlueprintCallable, Category = "Weather")
    void SetWeather(EWeatherType NewWeather);

    UFUNCTION(BlueprintCallable, Category = "Weather")
    EWeatherType GetRandomWeather() const;

    UFUNCTION(BlueprintPure, Category = "Weather")
    float GetTractionModifier() const;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    UPROPERTY()
    UParticleSystemComponent* RainParticles;

    UPROPERTY()
    UParticleSystemComponent* SnowParticles;

    UPROPERTY()
    UExponentialHeightFogComponent* FogComponent;

    // Niagara weather FX (preferred over Cascade in UE5)
    UPROPERTY()
    class UNiagaraComponent* NiagaraRain;

    UPROPERTY()
    class UNiagaraComponent* NiagaraSnow;

    UPROPERTY()
    class UNiagaraComponent* NiagaraDust;

    UPROPERTY()
    class UNiagaraComponent* NiagaraStorm;

    // Niagara system references (loaded from Content/)
    UPROPERTY(EditDefaultsOnly, Category = "Weather|Niagara")
    TSoftObjectPtr<class UNiagaraSystem> NiagaraRainAsset;

    UPROPERTY(EditDefaultsOnly, Category = "Weather|Niagara")
    TSoftObjectPtr<class UNiagaraSystem> NiagaraSnowAsset;

    UPROPERTY(EditDefaultsOnly, Category = "Weather|Niagara")
    TSoftObjectPtr<class UNiagaraSystem> NiagaraDustAsset;

    UPROPERTY(EditDefaultsOnly, Category = "Weather|Niagara")
    TSoftObjectPtr<class UNiagaraSystem> NiagaraStormAsset;

    float TransitionAlpha = 0.0f;
    EWeatherType TargetWeather = EWeatherType::Clear;
    FTimerHandle CycleTimer;

    void OnCycleTimer();
    void UpdateEffects(float DeltaTime);
    void UpdateNiagaraEffects(EWeatherType Weather);
    void SpawnNiagaraIfNeeded();
};
