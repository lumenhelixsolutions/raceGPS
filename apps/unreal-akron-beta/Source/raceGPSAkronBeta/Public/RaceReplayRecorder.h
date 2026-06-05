#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "RaceReplayRecorder.generated.h"

USTRUCT(BlueprintType)
struct FReplayFrame
{
    GENERATED_BODY()

    UPROPERTY()
    float Timestamp = 0.0f;

    UPROPERTY()
    FVector Location = FVector::ZeroVector;

    UPROPERTY()
    FRotator Rotation = FRotator::ZeroRotator;

    UPROPERTY()
    float SpeedKmh = 0.0f;

    UPROPERTY()
    float Throttle = 0.0f;

    UPROPERTY()
    float Steering = 0.0f;

    UPROPERTY()
    float Brake = 0.0f;

    UPROPERTY()
    bool bHandbrake = false;
};

UCLASS()
class RACEGPSAKRONBETA_API URaceReplayRecorder : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "raceGPS|Replay")
    void StartRecording();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Replay")
    void StopRecording();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Replay")
    void RecordFrame(const FReplayFrame& Frame);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Replay")
    void ClearRecording();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Replay")
    bool SaveToFile(const FString& Filename) const;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Replay")
    bool LoadFromFile(const FString& Filename);

    UFUNCTION(BlueprintPure, Category = "raceGPS|Replay")
    bool IsRecording() const { return bRecording; }

    UFUNCTION(BlueprintPure, Category = "raceGPS|Replay")
    int32 GetFrameCount() const { return Frames.Num(); }

    UFUNCTION(BlueprintPure, Category = "raceGPS|Replay")
    float GetRecordingDuration() const { return RecordingDuration; }

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Replay")
    TArray<FReplayFrame> Frames;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Replay")
    float RecordInterval = 0.033f; // ~30fps

private:
    bool bRecording = false;
    float RecordingDuration = 0.0f;
    float LastRecordTime = 0.0f;
};
