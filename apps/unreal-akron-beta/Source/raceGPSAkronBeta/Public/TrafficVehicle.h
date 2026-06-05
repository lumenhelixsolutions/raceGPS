#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrafficVehicle.generated.h"

UCLASS()
class RACEGPSAKRONBETA_API ATrafficVehicle : public AActor
{
    GENERATED_BODY()

public:
    ATrafficVehicle(const FObjectInitializer& ObjectInitializer);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Traffic")
    float TargetSpeedKmh = 45.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Traffic")
    float Acceleration = 5.0f;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Traffic")
    void SetRoadPoints(const TArray<FVector>& Points);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Traffic")
    void StartDriving();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Traffic")
    void StopDriving();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Traffic")
    TObjectPtr<class UStaticMeshComponent> VehicleMesh;

    UPROPERTY()
    TArray<FVector> RoadPoints;

    int32 CurrentPointIndex = 0;
    float CurrentSpeed = 0.0f;
    bool bDriving = false;

    void MoveAlongRoad(float DeltaTime);
};
