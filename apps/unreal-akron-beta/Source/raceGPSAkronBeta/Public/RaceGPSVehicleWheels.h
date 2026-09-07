#pragma once

#include "CoreMinimal.h"
#include "ChaosVehicleWheel.h"
#include "RaceGPSVehicleWheels.generated.h"

/** Distinct CDOs so AxleType Front/Rear never thrash a shared WheelClass. */
UCLASS()
class RACEGPSAKRONBETA_API URaceGPSVehicleWheelFront : public UChaosVehicleWheel
{
	GENERATED_BODY()
public:
	URaceGPSVehicleWheelFront()
	{
		AxleType = EAxleType::Front;
		bAffectedByEngine = true;
		bAffectedByBrake = true;
		bAffectedBySteering = true;
		bAffectedByHandbrake = false;
		MaxSteerAngle = 40.f;
		WheelRadius = 35.f;
	}
};

UCLASS()
class RACEGPSAKRONBETA_API URaceGPSVehicleWheelRear : public UChaosVehicleWheel
{
	GENERATED_BODY()
public:
	URaceGPSVehicleWheelRear()
	{
		AxleType = EAxleType::Rear;
		bAffectedByEngine = true;
		bAffectedByBrake = true;
		bAffectedBySteering = false;
		bAffectedByHandbrake = true;
		MaxSteerAngle = 0.f;
		WheelRadius = 35.f;
	}
};
