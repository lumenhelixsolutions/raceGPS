#include "TrafficVehicle.h"
#include "Components/StaticMeshComponent.h"

ATrafficVehicle::ATrafficVehicle(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;

    VehicleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VehicleMesh"));
    VehicleMesh->SetCollisionProfileName(TEXT("Vehicle"));
    VehicleMesh->SetSimulatePhysics(false);
    RootComponent = VehicleMesh;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        VehicleMesh->SetStaticMesh(CubeMesh.Object);
        VehicleMesh->SetRelativeScale3D(FVector(2.0f, 1.0f, 0.8f));
    }
}

void ATrafficVehicle::BeginPlay()
{
    Super::BeginPlay();
}

void ATrafficVehicle::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bDriving)
    {
        MoveAlongRoad(DeltaTime);
    }
}

void ATrafficVehicle::SetRoadPoints(const TArray<FVector>& Points)
{
    RoadPoints = Points;
    CurrentPointIndex = 0;
    if (RoadPoints.Num() > 0)
    {
        SetActorLocation(RoadPoints[0]);
    }
}

void ATrafficVehicle::StartDriving()
{
    bDriving = true;
    CurrentPointIndex = 0;
    CurrentSpeed = 0.0f;
}

void ATrafficVehicle::StopDriving()
{
    bDriving = false;
    CurrentSpeed = 0.0f;
}

void ATrafficVehicle::MoveAlongRoad(float DeltaTime)
{
    if (CurrentPointIndex >= RoadPoints.Num() - 1)
    {
        CurrentPointIndex = 0;
        if (RoadPoints.Num() > 0)
        {
            SetActorLocation(RoadPoints[0]);
        }
        return;
    }

    FVector CurrentPos = GetActorLocation();
    FVector TargetPos = RoadPoints[CurrentPointIndex + 1];
    FVector ToTarget = TargetPos - CurrentPos;
    float DistToTarget = ToTarget.Size();

    float TargetSpeedMs = TargetSpeedKmh / 3.6f;
    CurrentSpeed = FMath::FInterpConstantTo(CurrentSpeed, TargetSpeedMs, DeltaTime, Acceleration);

    float MoveDist = CurrentSpeed * DeltaTime;
    if (MoveDist >= DistToTarget)
    {
        CurrentPointIndex++;
        if (CurrentPointIndex >= RoadPoints.Num() - 1)
        {
            SetActorLocation(TargetPos);
            CurrentPointIndex = 0;
            return;
        }
    }
    else
    {
        FVector Direction = ToTarget.GetSafeNormal();
        FVector NewPos = CurrentPos + Direction * MoveDist;
        FRotator TargetRot = Direction.Rotation();
        SetActorLocationAndRotation(NewPos, TargetRot);
    }
}
