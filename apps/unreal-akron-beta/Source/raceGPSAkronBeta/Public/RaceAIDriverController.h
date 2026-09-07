#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ClevelandShowcaseTypes.h"
#include "RaceAIDriverController.generated.h"

class AChaosVehiclePawn;
class URacingLineComponent;
class URaceSessionManager;
class ARaceGridManager;

/**
 * Deterministic racing-line follower. Possesses AChaosVehiclePawn (not ATrafficVehicle).
 * Inputs are gated to zero unless URaceSessionManager is in Racing.
 */
UCLASS()
class RACEGPSAKRONBETA_API ARaceAIDriverController : public AAIController
{
	GENERATED_BODY()

public:
	ARaceAIDriverController();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|AI")
	FRaceAIPersonality Personality;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|AI")
	FRaceAIControlGains Gains;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Cleveland|Telemetry")
	FRaceAITelemetry Telemetry;

	UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Cleveland|Telemetry")
	float CurrentSplineDistance = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Cleveland|Telemetry")
	float TargetSplineDistance = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Cleveland|Telemetry")
	float CrossTrackError = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Cleveland|Telemetry")
	float HeadingError = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Cleveland|Telemetry")
	float CurrentSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Cleveland|Telemetry")
	float TargetSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Cleveland|Telemetry")
	float ThrottleCommand = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Cleveland|Telemetry")
	float BrakeCommand = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Cleveland|Telemetry")
	float SteeringCommand = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Cleveland|Telemetry")
	ERaceRecoveryState RecoveryState = ERaceRecoveryState::None;

	UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Cleveland|Telemetry")
	float LapProgress = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Cleveland|AI")
	int32 LapIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Cleveland|AI")
	int32 SlotIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Cleveland|AI")
	bool bFinished = false;

	UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Cleveland|AI")
	float LastLapTimeSec = 0.f;

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland|AI")
	void ConfigureDriver(URacingLineComponent* InLine, URaceSessionManager* InSession, int32 InSlot, const FRaceAIPersonality& InPersonality);

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland|AI")
	void SetGridManager(ARaceGridManager* InGrid);

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland|AI")
	float GetRaceProgress() const;

	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Drive any Chaos pawn with the racing-line law (used for AI-as-player playtest). */
	void TickDriveForPawn(AChaosVehiclePawn* Vehicle, float DeltaSeconds);

protected:
	UPROPERTY()
	TObjectPtr<URacingLineComponent> RacingLine;

	UPROPERTY()
	TObjectPtr<URaceSessionManager> SessionManager;

	UPROPERTY()
	TObjectPtr<ARaceGridManager> GridManager;

	float StuckTimer = 0.f;
	float RecoveryTimer = 0.f;
	float PrevS = 0.f;
	bool bHavePrevS = false;
	float LapStartWorldTime = 0.f;
	float LiveElapsed = 0.f;
	float PeakSpeedKmh = 0.f;
	bool bLeftStartZone = false;
	bool bLoggedSkipRecovery = false;
	int32 NoiseSeed = 1;

	bool IsRaceLive() const;
	void ZeroVehicleInputs(AChaosVehiclePawn* Vehicle);
	void ApplyCommands(AChaosVehiclePawn* Vehicle, float Steer, float Throttle, float Brake, bool bHandbrake);
	void TickRecovery(AChaosVehiclePawn* Vehicle, float DeltaSeconds, float AbsCteCm);
	void SnapToNearestSpline(AChaosVehiclePawn* Vehicle);
	void DetectLapWrap(float NewS, float TrackLength);
	void PublishTelemetry();
};
