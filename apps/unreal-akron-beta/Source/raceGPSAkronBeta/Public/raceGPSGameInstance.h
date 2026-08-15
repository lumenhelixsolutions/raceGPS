#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "raceGPSGameInstance.generated.h"

UCLASS()
class RACEGPSAKRONBETA_API UraceGPSGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    UraceGPSGameInstance(const FObjectInitializer& ObjectInitializer);

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "raceGPS|Settings")
    float MasterVolume = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "raceGPS|Settings")
    float MusicVolume = 0.7f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "raceGPS|Settings")
    float SFXVolume = 0.8f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "raceGPS|Settings")
    bool bInvertSteering = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "raceGPS|Settings")
    float SteeringSensitivity = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "raceGPS|Settings")
    FString LastSelectedRoute = TEXT("akron_cruise_sprint_001");

    // Default hero vehicle: CARLA Dodge Charger 2024 (T5). Matched by
    // DisplayName against ACruiseSprintGameMode::VehiclePresets in StartPlay;
    // falls back to VehiclePresets[0] if the preset is ever removed.
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "raceGPS|Settings")
    FString LastSelectedVehicle = TEXT("CARLA Charger");

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "raceGPS|Settings")
    FString LastSelectedHandlingMode = TEXT("Arcade");

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Settings")
    TObjectPtr<class UVehicleTuningData> LastSelectedVehicleTuning;

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Progress")
    TMap<FString, float> BestTimes;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Progress")
    void UpdateBestTime(const FString& RouteId, float TimeSeconds);

    UFUNCTION(BlueprintPure, Category = "raceGPS|Progress")
    float GetBestTime(const FString& RouteId) const;

    UFUNCTION(BlueprintPure, Category = "raceGPS|Progress")
    FString GetMedalForTime(float TimeSeconds) const;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Settings")
    void SaveSettings();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Settings")
    void LoadSettings();

protected:
    virtual void Init() override;

    FString GetSaveSlotName() const;
};
