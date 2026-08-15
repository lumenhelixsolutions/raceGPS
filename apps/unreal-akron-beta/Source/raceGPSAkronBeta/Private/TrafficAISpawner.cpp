// Copyright raceGPS. All Rights Reserved.

#include "TrafficAISpawner.h"
#include "TrafficVehicle.h"
#include "Components/SplineComponent.h"

ATrafficAISpawner::ATrafficAISpawner(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;

    // Default traffic vehicle class
    // The BP at /Game/Vehicles/Traffic/BP_TrafficVehicle does not exist yet (only the C++ ATrafficVehicle base).
    // Using the native C++ class directly lets the cooker succeed.
    // Create the BP subclass later for data-driven traffic variants if desired.
    TrafficVehicleClass = ATrafficVehicle::StaticClass();
}

void ATrafficAISpawner::BeginPlay()
{
    Super::BeginPlay();
}

void ATrafficAISpawner::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    SpawnTimer += DeltaTime;
    if (SpawnTimer >= SpawnInterval)
    {
        SpawnTimer = 0.0f;
        TrySpawnVehicle();
    }
}

void ATrafficAISpawner::SpawnTraffic(int32 Density)
{
    int32 ToSpawn = FMath::Clamp(Density, 0, MaxTrafficDensity);
    for (int32 i = 0; i < ToSpawn; ++i)
    {
        TrySpawnVehicle();
        if (ActiveTraffic.Num() >= MaxTrafficDensity)
        {
            break;
        }
    }
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Spawned traffic: %d active vehicles"), ActiveTraffic.Num());
}

void ATrafficAISpawner::DespawnFarTraffic(const FVector& PlayerLocation, float MaxDistance)
{
    for (int32 i = ActiveTraffic.Num() - 1; i >= 0; --i)
    {
        ATrafficVehicle* Vehicle = ActiveTraffic[i];
        if (!Vehicle)
        {
            ActiveTraffic.RemoveAt(i);
            continue;
        }

        float Dist = FVector::Distance(PlayerLocation, Vehicle->GetActorLocation());
        if (Dist > MaxDistance)
        {
            Vehicle->Destroy();
            ActiveTraffic.RemoveAt(i);
        }
    }
}

void ATrafficAISpawner::UpdateTraffic(float DeltaTime)
{
    // Traffic vehicles tick themselves; this hook allows future group behavior
    for (ATrafficVehicle* Vehicle : ActiveTraffic)
    {
        if (Vehicle)
        {
            // Future: traffic light awareness, lane changing, etc.
        }
    }
}

void ATrafficAISpawner::ClearAllTraffic()
{
    for (ATrafficVehicle* Vehicle : ActiveTraffic)
    {
        if (Vehicle)
        {
            Vehicle->Destroy();
        }
    }
    ActiveTraffic.Empty();
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] All traffic cleared"));
}

void ATrafficAISpawner::TrySpawnVehicle()
{
    if (ActiveTraffic.Num() >= MaxTrafficDensity)
        return;

    if (!TrafficVehicleClass)
        return;

    if (RoadSplines.Num() == 0)
        return;

    USplineComponent* Spline = RoadSplines[FMath::RandRange(0, RoadSplines.Num() - 1)];
    if (!Spline)
        return;

    float SplineLength = Spline->GetSplineLength();
    float Distance = FMath::FRandRange(0.0f, SplineLength);
    FVector SpawnPos = Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
    FRotator SpawnRot = Spline->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    ATrafficVehicle* Vehicle = GetWorld()->SpawnActor<ATrafficVehicle>(TrafficVehicleClass, SpawnPos, SpawnRot, Params);
    if (Vehicle)
    {
        AssignSplineToVehicle(Vehicle);
        ActiveTraffic.Add(Vehicle);
    }
}

void ATrafficAISpawner::AssignSplineToVehicle(ATrafficVehicle* Vehicle)
{
    if (!Vehicle)
        return;

    USplineComponent* Spline = RoadSplines[FMath::RandRange(0, RoadSplines.Num() - 1)];
    TArray<FVector> Points = SampleSplinePoints(Spline);
    Vehicle->SetRoadPoints(Points);
    Vehicle->TargetSpeedKmh = GetSpeedForSpline(Spline);
    Vehicle->StartDriving();
}

TArray<FVector> ATrafficAISpawner::SampleSplinePoints(USplineComponent* Spline) const
{
    TArray<FVector> Points;
    if (!Spline)
        return Points;

    float Length = Spline->GetSplineLength();
    int32 NumSamples = FMath::Clamp(FMath::FloorToInt(Length / 500.0f), 2, 50);

    for (int32 i = 0; i <= NumSamples; ++i)
    {
        float Dist = (Length * i) / NumSamples;
        Points.Add(Spline->GetLocationAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World));
    }
    return Points;
}

float ATrafficAISpawner::GetSpeedForSpline(USplineComponent* Spline) const
{
    // Future: vary speed based on road type
    return SpeedLimitKmh + FMath::FRandRange(-5.0f, 5.0f);
}
