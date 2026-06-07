// Copyright raceGPS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "RaceTimerSystem.generated.h"

USTRUCT(BlueprintType)
struct FRaceSectorTime
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 SectorIndex = 0;

    UPROPERTY(BlueprintReadOnly)
    float TimeSeconds = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float BestTimeSeconds = 0.0f;
};

/**
 * Lap timer + sector splits.
 * Saves best times to JSON in Saved/RaceTimes/.
 */
UCLASS()
class RACEGPSAKRONBETA_API URaceTimerSystem : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "raceGPS|RaceTimer")
    void StartRace();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|RaceTimer")
    void EndRace();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|RaceTimer")
    void StartLap();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|RaceTimer")
    void EndLap();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|RaceTimer")
    void RecordSector(int32 SectorIndex);

    UFUNCTION(BlueprintPure, Category = "raceGPS|RaceTimer")
    float GetCurrentLapTime() const;

    UFUNCTION(BlueprintPure, Category = "raceGPS|RaceTimer")
    float GetBestLapTime() const { return BestLapTime; }

    UFUNCTION(BlueprintPure, Category = "raceGPS|RaceTimer")
    float GetSectorTime(int32 SectorIndex) const;

    UFUNCTION(BlueprintPure, Category = "raceGPS|RaceTimer")
    float GetBestSectorTime(int32 SectorIndex) const;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|RaceTimer")
    bool SaveBestTimes(const FString& TrackId);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|RaceTimer")
    bool LoadBestTimes(const FString& TrackId);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|RaceTimer")
    int32 SectorCount = 3;

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|RaceTimer")
    float RaceStartTime = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|RaceTimer")
    float CurrentLapStartTime = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|RaceTimer")
    float BestLapTime = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|RaceTimer")
    TArray<FRaceSectorTime> CurrentSectorTimes;

protected:
    bool bRaceActive = false;
    bool bLapActive = false;

    FString GetRaceTimesPath(const FString& TrackId) const;
    void UpdateBestSector(int32 SectorIndex, float TimeSeconds);
};
