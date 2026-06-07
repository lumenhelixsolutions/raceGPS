// Copyright raceGPS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "DriftScoreSystem.generated.h"

USTRUCT(BlueprintType)
struct FDriftSession
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    float StartTime = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float EndTime = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float TotalAngle = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float PeakAngle = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float AverageSpeed = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float Score = 0.0f;
};

/**
 * Scores drifting based on angle, speed, and duration.
 */
UCLASS()
class RACEGPSAKRONBETA_API UDriftScoreSystem : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "raceGPS|Drift")
    void StartDrift();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Drift")
    void UpdateDrift(float Angle, float Speed);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Drift")
    FDriftSession EndDrift();

    UFUNCTION(BlueprintPure, Category = "raceGPS|Drift")
    float GetCurrentScore() const;

    UFUNCTION(BlueprintPure, Category = "raceGPS|Drift")
    float GetMultiplier() const;

    UFUNCTION(BlueprintPure, Category = "raceGPS|Drift")
    bool IsDrifting() const { return bDrifting; }

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Drift")
    void ResetScore();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Drift")
    float AngleThreshold = 10.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Drift")
    float SpeedThreshold = 30.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Drift")
    float ScoreMultiplierBase = 1.0f;

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Drift")
    float TotalScore = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Drift")
    TArray<FDriftSession> DriftHistory;

protected:
    bool bDrifting = false;
    float DriftStartTime = 0.0f;
    float CurrentDriftScore = 0.0f;
    float CurrentMaxAngle = 0.0f;
    float CurrentSpeedSum = 0.0f;
    int32 CurrentSampleCount = 0;

    float CalculateMultiplier(float Angle, float Speed) const;
};
