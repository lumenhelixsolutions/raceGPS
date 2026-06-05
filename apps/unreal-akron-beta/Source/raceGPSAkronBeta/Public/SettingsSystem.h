#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SettingsSystem.generated.h"

USTRUCT(BlueprintType)
struct FVideoSettings
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    int32 ResolutionX = 1920;

    UPROPERTY(BlueprintReadWrite)
    int32 ResolutionY = 1080;

    UPROPERTY(BlueprintReadWrite)
    bool bFullscreen = true;

    UPROPERTY(BlueprintReadWrite)
    bool bVSync = true;

    UPROPERTY(BlueprintReadWrite)
    float FieldOfView = 90.0f;

    UPROPERTY(BlueprintReadWrite)
    int32 ShadowQuality = 2; // 0-3

    UPROPERTY(BlueprintReadWrite)
    int32 AntiAliasing = 2; // 0-3

    UPROPERTY(BlueprintReadWrite)
    float DrawDistance = 1.0f; // 0.5-2.0
};

USTRUCT(BlueprintType)
struct FAudioSettings
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    float MasterVolume = 1.0f;

    UPROPERTY(BlueprintReadWrite)
    float MusicVolume = 0.7f;

    UPROPERTY(BlueprintReadWrite)
    float SFXVolume = 0.8f;

    UPROPERTY(BlueprintReadWrite)
    float EngineVolume = 0.9f;

    UPROPERTY(BlueprintReadWrite)
    bool bMuteOnFocusLost = false;
};

USTRUCT(BlueprintType)
struct FControlSettings
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    float SteeringSensitivity = 1.0f;

    UPROPERTY(BlueprintReadWrite)
    float ThrottleSensitivity = 1.0f;

    UPROPERTY(BlueprintReadWrite)
    bool bInvertSteering = false;

    UPROPERTY(BlueprintReadWrite)
    bool bInvertThrottle = false;

    UPROPERTY(BlueprintReadWrite)
    bool bManualTransmission = false;

    UPROPERTY(BlueprintReadWrite)
    bool bToggleHandbrake = false;
};

UCLASS()
class RACEGPSAKRONBETA_API USettingsSystem : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "raceGPS|Settings")
    void LoadSettings();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Settings")
    void SaveSettings();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Settings")
    void ApplyVideoSettings();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Settings")
    void ApplyAudioSettings();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Settings")
    void ApplyControlSettings(class AChaosVehiclePawn* Vehicle);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Settings")
    void ResetToDefaults();

    UPROPERTY(BlueprintReadWrite, Category = "raceGPS|Settings")
    FVideoSettings Video;

    UPROPERTY(BlueprintReadWrite, Category = "raceGPS|Settings")
    FAudioSettings Audio;

    UPROPERTY(BlueprintReadWrite, Category = "raceGPS|Settings")
    FControlSettings Controls;

private:
    FString GetSettingsPath() const;
};
