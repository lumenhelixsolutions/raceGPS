#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VehicleAudioComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RACEGPSAKRONBETA_API UVehicleAudioComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UVehicleAudioComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Audio")
    TObjectPtr<class USoundBase> EngineIdleSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Audio")
    TObjectPtr<class USoundBase> EngineRevSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Audio")
    TObjectPtr<class USoundBase> TireScreechSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Audio")
    TObjectPtr<class USoundBase> BrakeSquealSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Audio")
    TObjectPtr<class USoundBase> CollisionSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Audio")
    float EnginePitchMin = 0.8f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Audio")
    float EnginePitchMax = 2.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Audio")
    float ScreechThresholdKmh = 60.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Audio")
    float ScreechMinSlip = 0.3f;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Audio")
    void OnCollision(float ImpactSpeedKmh);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Audio")
    void SetMasterVolume(float Volume);

protected:
    UPROPERTY()
    TObjectPtr<class UAudioComponent> EngineAudio;

    UPROPERTY()
    TObjectPtr<class UAudioComponent> ScreechAudio;

    UPROPERTY()
    TObjectPtr<class UAudioComponent> BrakeAudio;

    float CurrentVolume = 1.0f;
    float LastSpeedKmh = 0.0f;

    void UpdateEngineSound(float SpeedKmh, float RPM);
    void UpdateScreechSound(float SpeedKmh, float Slip);
    void UpdateBrakeSound(float BrakeInput, float SpeedKmh);
};
