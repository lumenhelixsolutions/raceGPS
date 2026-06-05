#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "RaceReplayManager.generated.h"

class URaceReplayRecorder;
class URaceReplayPlayer;
class AGhostVehicle;

UCLASS()
class RACEGPSAKRONBETA_API URaceReplayManager : public UObject
{
    GENERATED_BODY()

public:
    URaceReplayManager();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Replay")
    void BeginRaceRecording();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Replay")
    void EndRaceRecording();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Replay")
    void PlayBestReplay(AGhostVehicle* GhostActor, float DelaySeconds);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Replay")
    void StopReplay();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Replay")
    void TickRecording(float DeltaTime);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Replay")
    void TickPlayback(float DeltaTime);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Replay")
    bool SaveBestReplay(const FString& RouteId);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Replay")
    bool LoadBestReplay(const FString& RouteId);

    UFUNCTION(BlueprintPure, Category = "raceGPS|Replay")
    URaceReplayRecorder* GetRecorder() const { return Recorder; }

    UFUNCTION(BlueprintPure, Category = "raceGPS|Replay")
    URaceReplayPlayer* GetPlayer() const { return Player; }

    UFUNCTION(BlueprintPure, Category = "raceGPS|Replay")
    bool HasBestReplay(const FString& RouteId) const;

protected:
    UPROPERTY()
    TObjectPtr<URaceReplayRecorder> Recorder;

    UPROPERTY()
    TObjectPtr<URaceReplayPlayer> Player;

    UPROPERTY()
    TObjectPtr<class AChaosVehiclePawn> TargetVehicle;

    FString GetReplayPath(const FString& RouteId) const;
    void UpdateRecorderFromVehicle();
};
