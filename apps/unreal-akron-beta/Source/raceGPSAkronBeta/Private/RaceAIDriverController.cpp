#include "RaceAIDriverController.h"
#include "ClevelandModuleCompat.h"
#include "RacingLineComponent.h"
#include "RaceGridManager.h"

ARaceAIDriverController::ARaceAIDriverController()
{
	bWantsPlayerState = false;
	PrimaryActorTick.bCanEverTick = true;
	Personality = FRaceAIPersonality::ConservativeAI01();
}

void ARaceAIDriverController::ConfigureDriver(URacingLineComponent* InLine, URaceSessionManager* InSession, int32 InSlot, const FRaceAIPersonality& InPersonality)
{
	RacingLine = InLine;
	SessionManager = InSession;
	SlotIndex = InSlot;
	Personality = InPersonality;
	NoiseSeed = 17 + InSlot * 91;
	LapIndex = 0;
	bFinished = false;
	bHavePrevS = false;
	bLeftStartZone = false;
	LiveElapsed = 0.f;
	PeakSpeedKmh = 0.f;
	StuckTimer = 0.f;
	RecoveryTimer = 0.f;
	RecoveryState = ERaceRecoveryState::None;
	bLoggedSkipRecovery = false;
}

void ARaceAIDriverController::SetGridManager(ARaceGridManager* InGrid)
{
	GridManager = InGrid;
}

void ARaceAIDriverController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	LapStartWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

bool ARaceAIDriverController::IsRaceLive() const
{
	return RaceGPS_IsSessionRacing(SessionManager);
}

float ARaceAIDriverController::GetRaceProgress() const
{
	const float Length = (RacingLine && RacingLine->IsValidLine()) ? RacingLine->TrackLength : 0.f;
	return RaceAIControlMath::RaceProgress(LapIndex, Length, CurrentSplineDistance);
}

void ARaceAIDriverController::ZeroVehicleInputs(AChaosVehiclePawn* Vehicle)
{
	if (!Vehicle)
	{
		return;
	}
	Vehicle->SetDriveOverride(0.f, 0.f, 1.f, true);
	ThrottleCommand = 0.f;
	BrakeCommand = 0.f;
	SteeringCommand = 0.f;
	PublishTelemetry();
}

void ARaceAIDriverController::ApplyCommands(AChaosVehiclePawn* Vehicle, float Steer, float Throttle, float Brake, bool bHandbrake)
{
	if (!Vehicle)
	{
		return;
	}
	Vehicle->SetDriveOverride(Steer, Throttle, Brake, bHandbrake);
}

void ARaceAIDriverController::PublishTelemetry()
{
	Telemetry.CurrentSplineDistance = CurrentSplineDistance;
	Telemetry.TargetSplineDistance = TargetSplineDistance;
	Telemetry.CrossTrackError = CrossTrackError;
	Telemetry.HeadingError = HeadingError;
	Telemetry.CurrentSpeed = CurrentSpeed;
	Telemetry.TargetSpeed = TargetSpeed;
	Telemetry.ThrottleCommand = ThrottleCommand;
	Telemetry.BrakeCommand = BrakeCommand;
	Telemetry.SteeringCommand = SteeringCommand;
	Telemetry.RecoveryState = RecoveryState;
	Telemetry.LapProgress = LapProgress;
}

void ARaceAIDriverController::DetectLapWrap(float NewS, float TrackLength)
{
	if (TrackLength <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	if (!bHavePrevS)
	{
		PrevS = NewS;
		bHavePrevS = true;
		return;
	}
	if (NewS > 0.15f * TrackLength && NewS < 0.90f * TrackLength)
	{
		bLeftStartZone = true;
	}
	const bool bWrapped =
		bLeftStartZone &&
		(PrevS > 0.80f * TrackLength) &&
		(NewS < 0.20f * TrackLength);
	if (bWrapped)
	{
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		LastLapTimeSec = Now - LapStartWorldTime;
		LapStartWorldTime = Now;
		LapIndex += 1;
		if (GridManager)
		{
			GridManager->NotifyVehicleLapComplete(SlotIndex, LastLapTimeSec);
		}
		if (LapIndex >= 1)
		{
			bFinished = true;
		}
	}
	PrevS = NewS;
}

void ARaceAIDriverController::SnapToNearestSpline(AChaosVehiclePawn* Vehicle)
{
	if (!Vehicle || !RacingLine || !RacingLine->IsValidLine())
	{
		return;
	}
	const float S = RacingLine->GetNearestS(Vehicle->GetActorLocation());
	const float Lat = RaceAIControlMath::LateralOffsetCm(Personality.Aggression, Gains.MaxLateralOffsetCm);
	const FTransform Pose = RacingLine->GetPoseAtS(S, Lat);
	Vehicle->ResetVehicle();
	Vehicle->SetActorTransform(Pose, false, nullptr, ETeleportType::TeleportPhysics);
	CurrentSplineDistance = S;
	RecoveryState = ERaceRecoveryState::None;
	RecoveryTimer = 0.f;
	StuckTimer = 0.f;
}

void ARaceAIDriverController::TickRecovery(AChaosVehiclePawn* Vehicle, float DeltaSeconds, float AbsCteCm)
{
	if (!Vehicle)
	{
		return;
	}

	const bool bTriggered = RaceAIControlMath::RecoveryTrigger(
		CurrentSpeed,
		ThrottleCommand,
		StuckTimer,
		AbsCteCm,
		Gains.RecoverySpeedKmh,
		Gains.RecoveryThrottleThresh,
		Gains.RecoveryStuckDelaySec,
		Gains.RecoveryMaxCteCm);

	if (RecoveryState == ERaceRecoveryState::None && bTriggered)
	{
		RecoveryState = ERaceRecoveryState::SteerBrake;
		RecoveryTimer = 0.f;
	}

	if (RecoveryState == ERaceRecoveryState::None)
	{
		return;
	}

	RecoveryTimer += DeltaSeconds;
	const FVector Tangent = RacingLine ? RacingLine->GetTangentAtS(CurrentSplineDistance) : Vehicle->GetActorForwardVector();
	HeadingError = RaceAIControlMath::SignedHeadingErrorRad(Vehicle->GetActorForwardVector(), Tangent);
	const float CteM = CrossTrackError * 0.01f;
	SteeringCommand = RaceAIControlMath::SteeringCommand(HeadingError, CteM, Gains.KHeading, Gains.KCrossTrack);

	if (RecoveryState == ERaceRecoveryState::SteerBrake)
	{
		// Skip full-stop brake for Cleveland playtest — keep moving on the line.
		ThrottleCommand = 1.f;
		BrakeCommand = 0.f;
		ApplyCommands(Vehicle, SteeringCommand, 1.f, 0.f, false);
		if (RecoveryTimer >= Gains.RecoverySteerBrakeSec)
		{
			RecoveryState = ERaceRecoveryState::Reverse;
			RecoveryTimer = 0.f;
		}
		return;
	}

	if (RecoveryState == ERaceRecoveryState::Reverse)
	{
		// Cleveland arcade: NEVER crawl at thr=0.35 / reverse. Punch forward thr=1 then snap.
		ThrottleCommand = 1.f;
		BrakeCommand = 0.f;
		ApplyCommands(Vehicle, SteeringCommand, 1.f, 0.f, false);
		if (RecoveryTimer >= Gains.RecoveryReverseSec)
		{
			RecoveryState = ERaceRecoveryState::ResetSnap;
			RecoveryTimer = 0.f;
		}
		return;
	}

	if (RecoveryState == ERaceRecoveryState::ResetSnap)
	{
		SnapToNearestSpline(Vehicle);
	}
}

void ARaceAIDriverController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickDriveForPawn(Cast<AChaosVehiclePawn>(GetPawn()), DeltaSeconds);
}

void ARaceAIDriverController::TickDriveForPawn(AChaosVehiclePawn* Vehicle, float DeltaSeconds)
{
	if (!Vehicle || !RacingLine || !RacingLine->IsValidLine())
	{
		return;
	}

	CurrentSpeed = Vehicle->GetSpeedKmh();
	float CteCm = 0.f;
	CurrentSplineDistance = RacingLine->GetNearestSWithCte(Vehicle->GetActorLocation(), CteCm);
	const float Length = RacingLine->TrackLength;
	// Start==finish nearest-S: sitting on the grid reports s~TrackLength. Treat as 0 until we have raced.
	if (LapIndex == 0 && LiveElapsed < 20.f && Length > KINDA_SMALL_NUMBER && CurrentSplineDistance > 0.85f * Length)
	{
		CurrentSplineDistance = 0.f;
		CteCm = 0.f;
	}
	CrossTrackError = CteCm;
	DetectLapWrap(CurrentSplineDistance, Length);
	LapProgress = (Length > KINDA_SMALL_NUMBER)
		? (CurrentSplineDistance / Length)
		: 0.f;

	if (!IsRaceLive() || bFinished)
	{
		LiveElapsed = 0.f;
		ZeroVehicleInputs(Vehicle);
		return;
	}
	LiveElapsed += DeltaSeconds;
	PeakSpeedKmh = FMath::Max(PeakSpeedKmh, CurrentSpeed);
	if (CurrentSpeed < 0.5f && LiveElapsed < 8.f)
	{
		Vehicle->WakeForDrive();
	}

	if (CurrentSpeed < Gains.RecoverySpeedKmh && ThrottleCommand > Gains.RecoveryThrottleThresh)
	{
		StuckTimer += DeltaSeconds;
	}
	else
	{
		StuckTimer = 0.f;
	}

	// Do not reverse-recover a car that has never actually rolled. On a crawl
	// (speed~0) reverse every ~8s undoes the few centimeters of progress.
	const bool bEverRolled = PeakSpeedKmh >= Gains.RecoverySpeedKmh;
	if (!bEverRolled)
	{
		if (RecoveryState != ERaceRecoveryState::None)
		{
			RecoveryState = ERaceRecoveryState::None;
			RecoveryTimer = 0.f;
		}
		if (LiveElapsed > 8.f && !bLoggedSkipRecovery)
		{
			bLoggedSkipRecovery = true;
			UE_LOG(LogTemp, Warning,
				TEXT("raceGPS Cleveland: skip stuck recovery slot=%d peak=%.2f km/h (need >= %.1f) - reverse would fight crawl"),
				SlotIndex, PeakSpeedKmh, Gains.RecoverySpeedKmh);
		}
	}
	const bool bAllowRecovery = LiveElapsed > 8.f && bEverRolled;
	if (bAllowRecovery && (RecoveryState != ERaceRecoveryState::None
		|| RaceAIControlMath::RecoveryTrigger(
			CurrentSpeed, ThrottleCommand, StuckTimer, FMath::Abs(CteCm),
			Gains.RecoverySpeedKmh, Gains.RecoveryThrottleThresh,
			Gains.RecoveryStuckDelaySec, Gains.RecoveryMaxCteCm)))
	{
		TickRecovery(Vehicle, DeltaSeconds, FMath::Abs(CteCm));
		PublishTelemetry();
		return;
	}

	const float LookAheadCm = (Gains.LookAheadMeters + Gains.LookAheadSpeedGain * CurrentSpeed) * 100.f;
	TargetSplineDistance = RaceAIControlMath::WrapS(CurrentSplineDistance + LookAheadCm, RacingLine->TrackLength);

	const float Lateral = RaceAIControlMath::LateralOffsetCm(Personality.Aggression, Gains.MaxLateralOffsetCm);
	const FVector TargetPos = RacingLine->GetPoseAtS(TargetSplineDistance, Lateral).GetLocation();
	const FVector Dir = TargetPos - Vehicle->GetActorLocation();
	HeadingError = RaceAIControlMath::SignedHeadingErrorRad(Vehicle->GetActorForwardVector(), Dir);

	const float CteMeters = CteCm * 0.01f;
	const float WorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const float Noise = Personality.ReactionNoise * FMath::Sin((WorldTime * 7.31f) + static_cast<float>(NoiseSeed));
	SteeringCommand = RaceAIControlMath::SteeringCommand(
		HeadingError + Noise * 0.15f,
		CteMeters,
		Gains.KHeading * (0.75f + 0.25f * Personality.Skill),
		Gains.KCrossTrack);

	const float Kappa = RacingLine->GetCurvatureAtS(TargetSplineDistance);
	const float Vmax = Gains.VmaxKmh * (0.85f + 0.15f * Personality.Skill);
	const float Vmin = Gains.VminKmh * Personality.CornerSpeed;
	TargetSpeed = RaceAIControlMath::TargetSpeedKmh(FMath::Abs(Kappa), Vmax, Vmin, Gains.KCurve);

	const float SpeedError = TargetSpeed - CurrentSpeed;
	if (SpeedError > 0.f)
	{
		ThrottleCommand = FMath::Clamp(SpeedError / FMath::Max(Gains.ThrottleTauKmh, KINDA_SMALL_NUMBER), 0.f, 1.f);
		BrakeCommand = 0.f;
	}
	else
	{
		// Soft coast only when well over target — never hard brake on racing-line playtest.
		ThrottleCommand = 0.f;
		BrakeCommand = FMath::Clamp((-SpeedError) / FMath::Max(Gains.BrakeTauKmh, KINDA_SMALL_NUMBER), 0.f, 0.35f);
	}
	// Cleveland arcade default: stay pinned to thr=1 on the racing line (GTA/MC leave-grid feel).
	// Prevents SpeedError/recovery from starving drive (thr=0.35 stall).
	ThrottleCommand = 1.f;
	BrakeCommand = 0.f;

	ApplyCommands(Vehicle, SteeringCommand, ThrottleCommand, BrakeCommand, false);
	PublishTelemetry();
}
