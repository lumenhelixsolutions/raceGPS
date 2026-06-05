#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "RaceReplayPlayer.generated.h"

class AGhostVehicle;
class URaceReplayRecorder;

UCLASS()
class RACEGPSAKRONBETA_API URaceReplayPlayer : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "raceGPS|Replay")
    void SetRecorder(URaceReplayRecorder* InRecorder);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Replay")
    void SetGhostActor(AGhostVehicle* InGhost);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Replay")
    void StartPlayback(float DelaySeconds);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Replay")
    void StopPlayback();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Replay")
    void SeekToTime(float TimeSeconds);

    UFUNCTION(BlueprintPure, Category = "raceGPS|Replay")
    bool IsPlaying() const { return bPlaying; }

    UFUNCTION(BlueprintPure, Category = "raceGPS|Replay")
    float GetCurrentTime() const { return CurrentTime; }

    UFUNCTION(BlueprintPure, Category = "raceGPS|Replay")
    float GetTotalDuration() const;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Replay")
    float PlaybackSpeed = 1.0f;

protected:
    UPROPERTY()
    TObjectPtr<URaceReplayRecorder> Recorder;

    UPROPERTY()
    TObjectPtr<AGhostVehicle> GhostActor;

    void TickPlayback(float DeltaTime);

private:
    bool bPlaying = false;
    float CurrentTime = 0.0f;
    int32 CurrentFrameIndex = 0;

    friend class URaceReplayManager;
};
