#include "TrafficSpawner.h"
#include "TrafficVehicle.h"
#include "AkronXodrImporter.h"
#include "Kismet/GameplayStatics.h"
#include "ChaosVehiclePawn.h"

ATrafficSpawner::ATrafficSpawner(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;
}

void ATrafficSpawner::BeginPlay()
{
    Super::BeginPlay();

    UAkronXodrImporter::ImportXodr(XodrPath, RoadSegments);
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] TrafficSpawner loaded %d road segments"), RoadSegments.Num());
}

void ATrafficSpawner::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    SpawnTimer += DeltaTime;
    if (SpawnTimer >= SpawnInterval)
    {
        SpawnTimer = 0.0f;
        TrySpawnVehicle();
    }

    DespawnDistantVehicles();
}

void ATrafficSpawner::SpawnTraffic()
{
    for (int32 i = 0; i < MaxTrafficVehicles; ++i)
    {
        TrySpawnVehicle();
    }
}

void ATrafficSpawner::ClearTraffic()
{
    for (ATrafficVehicle* Vehicle : ActiveVehicles)
    {
        if (Vehicle)
        {
            Vehicle->Destroy();
        }
    }
    ActiveVehicles.Empty();
}

void ATrafficSpawner::TrySpawnVehicle()
{
    if (ActiveVehicles.Num() >= MaxTrafficVehicles)
        return;

    AChaosVehiclePawn* Player = Cast<AChaosVehiclePawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
    if (!Player)
        return;

    TArray<FVector> Points = GetRandomRoadPoints();
    if (Points.Num() < 2)
        return;

    FVector SpawnLoc = Points[0];
    float DistToPlayer = FVector::Distance(SpawnLoc, Player->GetActorLocation());
    if (DistToPlayer > SpawnRadius)
        return;

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ATrafficVehicle* Vehicle = GetWorld()->SpawnActor<ATrafficVehicle>(
        ATrafficVehicle::StaticClass(), SpawnLoc, FRotator::ZeroRotator, Params);

    if (Vehicle)
    {
        Vehicle->SetRoadPoints(Points);
        Vehicle->StartDriving();
        ActiveVehicles.Add(Vehicle);
    }
}

void ATrafficSpawner::DespawnDistantVehicles()
{
    AChaosVehiclePawn* Player = Cast<AChaosVehiclePawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
    if (!Player)
        return;

    for (int32 i = ActiveVehicles.Num() - 1; i >= 0; --i)
    {
        ATrafficVehicle* Vehicle = ActiveVehicles[i];
        if (!Vehicle)
        {
            ActiveVehicles.RemoveAt(i);
            continue;
        }

        float Dist = FVector::Distance(Vehicle->GetActorLocation(), Player->GetActorLocation());
        if (Dist > DespawnRadius)
        {
            Vehicle->Destroy();
            ActiveVehicles.RemoveAt(i);
        }
    }
}

TArray<FVector> ATrafficSpawner::GetRandomRoadPoints() const
{
    TArray<FVector> Empty;
    if (RoadSegments.Num() == 0)
        return Empty;

    int32 Index = FMath::RandRange(0, RoadSegments.Num() - 1);
    return RoadSegments[Index].WorldPoints;
}
