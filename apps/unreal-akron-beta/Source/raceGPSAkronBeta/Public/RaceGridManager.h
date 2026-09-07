#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ClevelandShowcaseTypes.h"
#include "RaceGridManager.generated.h"

class AChaosVehiclePawn;
class ARaceAIDriverController;
class URacingLineComponent;
class URaceSessionManager;
class APlayerController;

/**
 * 3-slot PLAYER/AI/AI grid. Spawns Chaos vehicle pawns (physics) before countdown.
 * Standings sort by LapIndex * TrackLength + CurrentSplineDistance (not Euclidean).
 */
UCLASS()
class RACEGPSAKRONBETA_API ARaceGridManager : public AActor
{
	GENERATED_BODY()

public:
	ARaceGridManager();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Cleveland|Grid")
	TObjectPtr<URacingLineComponent> RacingLine;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|Grid")
	TSubclassOf<AChaosVehiclePawn> VehicleClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|Grid")
	TSubclassOf<ARaceAIDriverController> AIControllerClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|Grid")
	float SlotSpacingCm = 650.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|Grid")
	float StaggerLateralCm = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|Grid")
	int32 TargetLaps = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|Grid")
	int32 NumSlots = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|Grid")
	TArray<EVehicleLook> SlotLooks;

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland|Grid")
	void BindSession(URaceSessionManager* InSession);

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland|Grid")
	bool SpawnGrid(APlayerController* PlayerPC);

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland|Grid")
	void RespawnGridAndRestart(APlayerController* PlayerPC);

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland|Grid")
	void DestroyGrid();

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland|Grid")
	TArray<FRaceStandingEntry> GetStandings() const;

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland|Grid")
	int32 GetPlayerPlace() const;

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland|Grid")
	AChaosVehiclePawn* GetPlayerPawn() const { return PlayerPawn; }

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland|Grid")
	void NotifyVehicleLapComplete(int32 SlotIndex, float LapTimeSec);

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland|Grid")
	bool HasPlayerFinished() const { return bPlayerFinished; }

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland|Grid")
	bool HaveAllFinished() const;

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland|Grid")
	void TickStandings(float WorldTimeSec);

	void DumpGridDriveState(const TCHAR* Tag);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|Grid")
	bool bAutoDrivePlayer = false;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	float PlayerPrevS = 0.f;
	bool bHavePlayerPrevS = false;

	UPROPERTY()
	TObjectPtr<URaceSessionManager> SessionManager;

	UPROPERTY()
	TObjectPtr<AChaosVehiclePawn> PlayerPawn;

	UPROPERTY()
	TArray<TObjectPtr<AChaosVehiclePawn>> Pawns;

	UPROPERTY()
	TArray<TObjectPtr<ARaceAIDriverController>> AIControllers;

	UPROPERTY()
	TArray<ERaceGridSlotRole> SlotRoles;

	UPROPERTY()
	TArray<float> FinishTimes;

	UPROPERTY()
	TArray<int32> SlotLaps;

	bool bPlayerFinished = false;
	bool bRaceEnded = false;
	int32 FinishedCount = 0;

	FTransform ComputeGridPose(int32 SlotIndex) const;
	void PossessPlayer(APlayerController* PlayerPC, AChaosVehiclePawn* Pawn);
	void SnapPawnToGround(AChaosVehiclePawn* Pawn);
	void HoldPlayerForCountdown();
	void ReleaseAllForRacing();

	UPROPERTY()
	TObjectPtr<ARaceAIDriverController> PlayerAutoDriver;

	bool bReleasedPlayerHandbrake = false;
	float RaceLiveElapsed = 0.f;
	bool bPlayerLeftStartZone = false;
};
