#pragma once

#include "CoreMinimal.h"
#include "ClevelandShowcaseTypes.generated.h"

/**
 * Cleveland Historic Circuit — shared telemetry / personality / standings types.
 * Category prefix: raceGPS|Cleveland
 */

UENUM(BlueprintType)
enum class ERaceRecoveryState : uint8
{
	None UMETA(DisplayName = "None"),
	SteerBrake UMETA(DisplayName = "SteerBrake"),
	Reverse UMETA(DisplayName = "Reverse"),
	ResetSnap UMETA(DisplayName = "ResetSnap")
};


UENUM(BlueprintType)
enum class EVehicleLook : uint8
{
    Hellcat UMETA(DisplayName = "Hellcat"),
    ChargerAsphalt UMETA(DisplayName = "Charger Asphalt"),
    ChargerSilver UMETA(DisplayName = "Charger Silver")
};

UENUM(BlueprintType)
enum class ERaceGridSlotRole : uint8
{
	Player UMETA(DisplayName = "PLAYER"),
	AI UMETA(DisplayName = "AI")
};

/** Deterministic personality. ReactionNoise is a bounded amplitude, not RL. */
USTRUCT(BlueprintType)
struct RACEGPSAKRONBETA_API FRaceAIPersonality
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|AI")
	float Skill = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|AI")
	float Aggression = 0.40f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|AI")
	float CornerSpeed = 0.88f;

	/** 0..1 amplitude of deterministic heading dither (low≈0.05, moderate≈0.15). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|AI")
	float ReactionNoise = 0.05f;

	static FRaceAIPersonality ConservativeAI01()
	{
		FRaceAIPersonality P;
		P.Skill = 0.75f;
		P.Aggression = 0.40f;
		P.CornerSpeed = 0.88f;
		P.ReactionNoise = 0.05f; // low
		return P;
	}

	static FRaceAIPersonality AggressiveAI02()
	{
		FRaceAIPersonality P;
		P.Skill = 0.88f;
		P.Aggression = 0.70f;
		P.CornerSpeed = 0.96f;
		P.ReactionNoise = 0.15f; // moderate
		return P;
	}
};

/**
 * Control-law constants shared with tests/test_race_ai_control.py.
 * KEEP IN SYNC with the Python reference (see CLEVELAND_CPP_WIREUP.md).
 *
 * Heading error is radians. Cross-track used by the steering law is meters
 * (Unreal stores CTE in cm on telemetry; divide by 100 before the law).
 */
USTRUCT(BlueprintType)
struct RACEGPSAKRONBETA_API FRaceAIControlGains
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|AI")
	float KHeading = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|AI")
	float KCrossTrack = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|AI")
	float VmaxKmh = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|AI")
	float VminKmh = 40.f;

	/** Curvature units: 1/meter (samples converted from 1/cm if needed). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|AI")
	float KCurve = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|AI")
	float LookAheadMeters = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|AI")
	float LookAheadSpeedGain = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|AI")
	float ThrottleTauKmh = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|AI")
	float BrakeTauKmh = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|AI")
	float RecoverySpeedKmh = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|AI")
	float RecoveryThrottleThresh = 0.50f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|AI")
	float RecoveryStuckDelaySec = 1.50f;

	/** Max |CTE| in centimeters before recovery. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|AI")
	float RecoveryMaxCteCm = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|AI")
	float RecoverySteerBrakeSec = 1.20f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|AI")
	float RecoveryReverseSec = 1.80f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|AI")
	float MaxLateralOffsetCm = 250.f;
};

USTRUCT(BlueprintType)
struct RACEGPSAKRONBETA_API FRaceAITelemetry
{
	GENERATED_BODY()

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
};

USTRUCT(BlueprintType)
struct RACEGPSAKRONBETA_API FRaceStandingEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Cleveland|HUD")
	int32 Place = 0;

	UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Cleveland|HUD")
	int32 SlotIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Cleveland|HUD")
	ERaceGridSlotRole Role = ERaceGridSlotRole::AI;

	UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Cleveland|HUD")
	int32 LapIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Cleveland|HUD")
	float CurrentSplineDistance = 0.f;

	/** RaceProgress = LapIndex * TrackLength + CurrentSplineDistance (cm). */
	UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Cleveland|HUD")
	float RaceProgress = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Cleveland|HUD")
	float LastLapTimeSec = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Cleveland|HUD")
	bool bFinished = false;
};

USTRUCT(BlueprintType)
struct RACEGPSAKRONBETA_API FRacingLineSample
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|Line")
	double Lat = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|Line")
	double Lon = 0.0;

	/** Arc length along the line, centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|Line")
	float S = 0.f;

	/** Curvature in 1/meter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|Line")
	float Curvature = 0.f;

	/** Local UE cm, Z-up (1uu = 1cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|Line")
	FVector WorldPos = FVector::ZeroVector;
};

/** Pure helpers — identical math lives in tests/test_race_ai_control.py. */
namespace RaceAIControlMath
{
	inline float Clamp11(float V)
	{
		return FMath::Clamp(V, -1.f, 1.f);
	}

	inline float WrapS(float S, float TrackLength)
	{
		if (TrackLength <= KINDA_SMALL_NUMBER)
		{
			return 0.f;
		}
		float W = FMath::Fmod(S, TrackLength);
		if (W < 0.f)
		{
			W += TrackLength;
		}
		return W;
	}

	inline float TargetSpeedKmh(float AbsCurvaturePerMeter, float Vmax, float Vmin, float KCurve)
	{
		const float Denom = 1.f + KCurve * FMath::Abs(AbsCurvaturePerMeter);
		const float Raw = Vmax / FMath::Max(Denom, KINDA_SMALL_NUMBER);
		return FMath::Clamp(Raw, Vmin, Vmax);
	}

	/** Signed heading error (rad): atan2(forward × dir, forward · dir) in XY, Z-up. */
	inline float SignedHeadingErrorRad(const FVector& ForwardXY, const FVector& DirToTargetXY)
	{
		const FVector F = FVector(ForwardXY.X, ForwardXY.Y, 0.f).GetSafeNormal();
		const FVector D = FVector(DirToTargetXY.X, DirToTargetXY.Y, 0.f).GetSafeNormal();
		if (F.IsNearlyZero() || D.IsNearlyZero())
		{
			return 0.f;
		}
		const float CrossZ = F.X * D.Y - F.Y * D.X;
		const float Dot = F.X * D.X + F.Y * D.Y;
		return FMath::Atan2(CrossZ, Dot);
	}

	inline float SteeringCommand(float HeadingErrorRad, float CteMeters, float KHeading, float KCrossTrack)
	{
		return Clamp11(KHeading * HeadingErrorRad + KCrossTrack * CteMeters);
	}

	inline bool RecoveryTrigger(float SpeedKmh, float ThrottleCmd, float StuckTimeSec,
		float AbsCteCm, float SpeedThresh, float ThrottleThresh, float DelaySec, float MaxCteCm)
	{
		const bool bStuck =
			(SpeedKmh < SpeedThresh) &&
			(ThrottleCmd > ThrottleThresh) &&
			(StuckTimeSec > DelaySec);
		const bool bOffLine = AbsCteCm > MaxCteCm;
		return bStuck || bOffLine;
	}

	inline float RaceProgress(int32 LapIndex, float TrackLengthCm, float CurrentSCm)
	{
		return static_cast<float>(LapIndex) * TrackLengthCm + CurrentSCm;
	}

	inline float LateralOffsetCm(float Aggression, float MaxOffsetCm)
	{
		return (Aggression - 0.5f) * 2.f * MaxOffsetCm;
	}
}
