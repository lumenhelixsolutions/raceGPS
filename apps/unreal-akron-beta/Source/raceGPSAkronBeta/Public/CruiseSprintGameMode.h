#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AkronXodrImporter.h"
#include "CruiseSprintGameMode.generated.h"

UENUM(BlueprintType)
enum class ECruiseSprintState : uint8
{
    None            UMETA(Hidden),
    Loading         UMETA(DisplayName = "Loading"),
    Countdown       UMETA(DisplayName = "Countdown"),
    Racing          UMETA(DisplayName = "Racing"),
    Finished        UMETA(DisplayName = "Finished"),
    Paused          UMETA(DisplayName = "Paused")
};

UCLASS()
class RACEGPSAKRONBETA_API ACruiseSprintGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ACruiseSprintGameMode(const FObjectInitializer& ObjectInitializer);

    virtual void StartPlay() override;
    virtual void Tick(float DeltaTime) override;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|GameMode")
    void StartRace();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|GameMode")
    void PauseRace();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|GameMode")
    void ResumeRace();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|GameMode")
    void FinishRace();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|GameMode")
    void RestartRace();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|GameMode")
    void OnCheckpointReached(int32 CheckpointIndex);

    UFUNCTION(BlueprintPure, Category = "raceGPS|GameMode")
    ECruiseSprintState GetRaceState() const { return CurrentState; }

    UFUNCTION(BlueprintPure, Category = "raceGPS|GameMode")
    float GetElapsedTime() const { return ElapsedTime; }

    UFUNCTION(BlueprintPure, Category = "raceGPS|GameMode")
    int32 GetCurrentCheckpoint() const { return CurrentCheckpoint; }

    UFUNCTION(BlueprintPure, Category = "raceGPS|GameMode")
    int32 GetTotalCheckpoints() const;

    UFUNCTION(BlueprintPure, Category = "raceGPS|GameMode")
    float GetTotalRaceDistance() const;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|GameMode")
    FString CityPackPath = TEXT("citypacks/akron-oh-beta-001/");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|GameMode")
    FString ManifestFile = TEXT("manifest.json");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|GameMode")
    FString XodrFile = TEXT("akron.xodr");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|GameMode")
    FString RouteDir = TEXT("citypacks/akron-oh-beta-001/routes/");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|GameMode")
    int32 SelectedRouteIndex = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|GameMode")
    float CountdownDuration = 3.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|GameMode")
    float GoldTimeSeconds = 120.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|GameMode")
    float SilverTimeSeconds = 150.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|GameMode")
    float BronzeTimeSeconds = 200.0f;

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|GameMode")
    TArray<FAkronRouteSpline> LoadedRoutes;

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|GameMode")
    TArray<FAkronSpawnPoint> LoadedSpawns;

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|GameMode")
    TArray<FAkronPOI> LoadedPOIs;

protected:
    virtual void OnRaceStateChanged(ECruiseSprintState NewState);

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|GameMode")
    ECruiseSprintState CurrentState = ECruiseSprintState::None;

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|GameMode")
    float ElapsedTime = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|GameMode")
    int32 CurrentCheckpoint = 0;

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|GameMode")
    float CountdownTimer = 0.0f;

    float WorldOriginLat = 41.08f;
    float WorldOriginLon = -81.52f;

    void LoadCityData();
    void SpawnPlayerAtStart();
    void SpawnCheckpoints();
    void UpdateCountdown(float DeltaTime);
};
