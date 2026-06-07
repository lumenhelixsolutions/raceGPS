#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DayNightCycle.generated.h"

UCLASS()
class RACEGPSAKRONBETA_API ADayNightCycle : public AActor
{
    GENERATED_BODY()

public:
    ADayNightCycle(const FObjectInitializer& ObjectInitializer);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Time")
    float DayLengthMinutes = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Time")
    float StartTimeOfDay = 12.0f; // 0-24 hours

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Time")
    bool bPaused = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Time")
    float TimeScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Atmosphere")
    bool bUseSkyAtmosphere = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Atmosphere")
    bool bUseVolumetricClouds = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Atmosphere")
    TSoftObjectPtr<class UTexture2D> HDRIEnvironmentMap;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Time")
    void SetTimeOfDay(float Hour);

    UFUNCTION(BlueprintPure, Category = "raceGPS|Time")
    float GetTimeOfDay() const { return CurrentTimeOfDay; }

    UFUNCTION(BlueprintPure, Category = "raceGPS|Time")
    FString GetTimeString() const;

    UFUNCTION(BlueprintPure, Category = "raceGPS|Time")
    bool IsDaytime() const { return CurrentTimeOfDay >= 6.0f && CurrentTimeOfDay < 18.0f; }

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Time")
    TObjectPtr<class UDirectionalLightComponent> SunLight;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Time")
    TObjectPtr<class USkyLightComponent> SkyLight;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Time")
    TObjectPtr<class UStaticMeshComponent> SkySphere;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Atmosphere")
    TObjectPtr<class USkyAtmosphereComponent> SkyAtmosphere;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Atmosphere")
    TObjectPtr<class UVolumetricCloudComponent> VolumetricClouds;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    void UpdateSunRotation();
    void UpdateSkyColor();
    void UpdateSkyAtmosphere();
    void UpdateVolumetricClouds();
    FLinearColor GetSkyColor(float Hour) const;
    float GetSunElevation() const;

    float CurrentTimeOfDay = 12.0f;
    float DayProgress = 0.0f;
};
