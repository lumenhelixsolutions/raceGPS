#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VehicleTuningData.generated.h"

USTRUCT(BlueprintType)
struct FWheelTuning
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Wheel")
    float Radius = 35.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Wheel")
    float Width = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Wheel")
    float Mass = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Wheel")
    float SteerAngle = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Wheel")
    bool bDrive = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Wheel")
    bool bHandbrake = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Suspension")
    float SuspensionStiffness = 450.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Suspension")
    float SuspensionDamping = 25.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Suspension")
    float MaxRaise = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Suspension")
    float MaxDrop = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Suspension")
    float SuspensionForceOffset = 0.0f;
};

USTRUCT(BlueprintType)
struct FTransmissionTuning
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Transmission")
    float FinalDriveRatio = 3.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Transmission")
    TArray<float> GearRatios = { 3.5f, 2.0f, 1.3f, 0.9f, 0.7f };

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Transmission")
    float ReverseGearRatio = -3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Transmission")
    float UpShiftRPM = 5500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Transmission")
    float DownShiftRPM = 2000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Transmission")
    float ChangeUpTime = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Transmission")
    float ChangeDownTime = 0.3f;
};

USTRUCT(BlueprintType)
struct FDifferentialTuning
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Differential")
    TEnumAsByte<enum EVehicleDifferential> DifferentialType = EVehicleDifferential::LimitedSlip_4W;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Differential")
    float FrontRearSplit = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Differential")
    float FrontLeftRightSplit = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Differential")
    float RearLeftRightSplit = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Differential")
    float CentreBias = 1.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Differential")
    float FrontBias = 1.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Differential")
    float RearBias = 1.3f;
};

UENUM(BlueprintType)
enum class EVehicleClass : uint8
{
    Sedan       UMETA(DisplayName = "Sedan"),
    Sports      UMETA(DisplayName = "Sports"),
    Truck       UMETA(DisplayName = "Truck"),
    Hatchback   UMETA(DisplayName = "Hatchback"),
    SUV         UMETA(DisplayName = "SUV")
};

UCLASS()
class RACEGPSAKRONBETA_API UVehicleTuningData : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Vehicle|Info")
    FString DisplayName = TEXT("Vehicle");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Vehicle|Info")
    FString Description = TEXT("A vehicle.");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Vehicle|Info")
    EVehicleClass VehicleClass = EVehicleClass::Sedan;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Vehicle")
    TArray<FWheelTuning> Wheels;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Vehicle")
    FTransmissionTuning Transmission;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Vehicle")
    FDifferentialTuning Differential;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Vehicle")
    float VehicleMass = 1500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Vehicle")
    float DragCoefficient = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Vehicle")
    float ChassisWidth = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Vehicle")
    float ChassisHeight = 140.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Engine")
    float MaxEngineRPM = 7000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Engine")
    float IdleRPM = 800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Engine")
    float BrakeTorque = 1500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Engine")
    float HandbrakeTorque = 3000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Aero")
    float DownForceCoefficient = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Aero")
    float DownForceOffset = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Steering")
    float SteeringCurve = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Steering")
    float AckermannAccuracy = 1.0f;
};
