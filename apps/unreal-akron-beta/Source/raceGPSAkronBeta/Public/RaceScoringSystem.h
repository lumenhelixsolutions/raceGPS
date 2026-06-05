#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "RaceScoringSystem.generated.h"

USTRUCT(BlueprintType)
struct FRaceScore
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    float BaseTime = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float CleanDrivingBonus = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float MissedCheckpointPenalty = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float CollisionPenalty = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float FinalTime = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    FString Medal = TEXT("NONE");

    UPROPERTY(BlueprintReadOnly)
    int32 Collisions = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 MissedCheckpoints = 0;
};

UCLASS()
class RACEGPSAKRONBETA_API URaceScoringSystem : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "raceGPS|Scoring")
    void Reset();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Scoring")
    void OnCollision(float ImpactSpeedKmh);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Scoring")
    void OnMissedCheckpoint();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Scoring")
    void OnCheckpointReached();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Scoring")
    FRaceScore CalculateFinalScore(float ElapsedTime) const;

    UFUNCTION(BlueprintPure, Category = "raceGPS|Scoring")
    float GetCurrentPenaltyTotal() const;

    UFUNCTION(BlueprintPure, Category = "raceGPS|Scoring")
    int32 GetCollisionCount() const { return CollisionCount; }

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Scoring")
    float CollisionPenaltyPerHit = 3.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Scoring")
    float CollisionSpeedMultiplier = 0.1f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Scoring")
    float MissedCheckpointPenalty = 10.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Scoring")
    float CleanDrivingThresholdSeconds = 120.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Scoring")
    float CleanDrivingBonus = 5.0f;

private:
    int32 CollisionCount = 0;
    int32 MissedCheckpointCount = 0;
    int32 ReachedCheckpointCount = 0;
    float TotalCollisionPenalty = 0.0f;
};
