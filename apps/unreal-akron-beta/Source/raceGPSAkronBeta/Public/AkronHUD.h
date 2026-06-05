#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "AkronHUD.generated.h"

UCLASS()
class RACEGPSAKRONBETA_API AAkronHUD : public AHUD
{
    GENERATED_BODY()

public:
    AAkronHUD(const FObjectInitializer& ObjectInitializer);

    virtual void DrawHUD() override;
    virtual void BeginPlay() override;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|HUD")
    void SetRaceTime(float Seconds);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|HUD")
    void SetCheckpointProgress(int32 Current, int32 Total);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|HUD")
    void SetSpeedKmh(float Speed);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|HUD")
    void SetCountdownValue(int32 Value);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|HUD")
    void ShowCountdown(bool bVisible);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|HUD")
    void ShowRaceFinished(float FinalTime, const FString& Medal);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|HUD")
    UFont* MainFont;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|HUD")
    UFont* LargeFont;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|HUD")
    FLinearColor TextColor = FLinearColor::White;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|HUD")
    FLinearColor GoldColor = FLinearColor(1.0f, 0.84f, 0.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|HUD")
    FLinearColor SilverColor = FLinearColor(0.75f, 0.75f, 0.75f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|HUD")
    FLinearColor BronzeColor = FLinearColor(0.8f, 0.5f, 0.2f);

protected:
    float RaceTime = 0.0f;
    int32 CurrentCheckpoint = 0;
    int32 TotalCheckpoints = 0;
    float SpeedKmh = 0.0f;
    int32 CountdownValue = 0;
    bool bShowCountdown = false;
    bool bShowFinished = false;
    FString FinishedMedal;
    float FinishedTime = 0.0f;

    void DrawRaceInfo();
    void DrawCountdown();
    void DrawFinishedScreen();
    FString FormatTime(float Seconds) const;
};
