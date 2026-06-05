#include "GhostVehicle.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AGhostVehicle::AGhostVehicle(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;

    GhostMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("GhostMesh"));
    GhostMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GhostMesh->SetGenerateOverlapEvents(false);
    GhostMesh->SetVisibility(true);
    RootComponent = GhostMesh;

    static ConstructorHelpers::FObjectFinder<USkeletalMesh> VehicleMesh(TEXT("/Game/Vehicles/Sedan/Sedan_SkelMesh.Sedan_SkelMesh"));
    if (VehicleMesh.Succeeded())
    {
        GhostMesh->SetSkeletalMesh(VehicleMesh.Object);
    }

    // Semi-transparent ghost material
    static ConstructorHelpers::FObjectFinder<UMaterial> GhostMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (GhostMat.Succeeded())
    {
        UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(GhostMat.Object, this);
        if (DynMat)
        {
            DynMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.0f, 0.8f, 1.0f));
            GhostMesh->SetMaterial(0, DynMat);
        }
    }
}

void AGhostVehicle::BeginPlay()
{
    Super::BeginPlay();
    UpdateVisuals();
}

void AGhostVehicle::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bRunning)
    {
        MoveAlongRoute(DeltaTime);
    }
}

void AGhostVehicle::SetRouteWaypoints(const TArray<FVector>& Waypoints)
{
    RouteWaypoints = Waypoints;
    CurrentWaypointIndex = 0;
    DistanceAlongRoute = 0.0f;
    CurrentSpeed = 0.0f;

    if (RouteWaypoints.Num() > 0)
    {
        SetActorLocation(RouteWaypoints[0]);
    }
}

void AGhostVehicle::StartGhostRun(float DelaySeconds)
{
    if (RouteWaypoints.Num() < 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] Ghost has no route waypoints"));
        return;
    }

    CurrentWaypointIndex = 0;
    DistanceAlongRoute = 0.0f;
    CurrentSpeed = 0.0f;
    SetActorLocation(RouteWaypoints[0]);

    FTimerHandle DelayTimer;
    GetWorld()->GetTimerManager().SetTimer(DelayTimer, [this]()
    {
        bRunning = true;
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Ghost run started"));
    }, DelaySeconds, false);
}

void AGhostVehicle::StopGhostRun()
{
    bRunning = false;
    CurrentSpeed = 0.0f;
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Ghost run stopped at %.0fm"), DistanceAlongRoute);
}

void AGhostVehicle::MoveAlongRoute(float DeltaTime)
{
    if (CurrentWaypointIndex >= RouteWaypoints.Num() - 1)
    {
        StopGhostRun();
        return;
    }

    FVector CurrentPos = GetActorLocation();
    FVector TargetPos = RouteWaypoints[CurrentWaypointIndex + 1];
    FVector ToTarget = TargetPos - CurrentPos;
    float DistToTarget = ToTarget.Size();

    // Accelerate towards target speed
    float TargetSpeedMs = TargetSpeedKmh / 3.6f;
    CurrentSpeed = FMath::FInterpConstantTo(CurrentSpeed, TargetSpeedMs, DeltaTime, Acceleration);

    // Move
    float MoveDist = CurrentSpeed * DeltaTime;
    if (MoveDist >= DistToTarget)
    {
        CurrentWaypointIndex++;
        DistanceAlongRoute += DistToTarget;
        if (CurrentWaypointIndex >= RouteWaypoints.Num() - 1)
        {
            SetActorLocation(TargetPos);
            StopGhostRun();
            return;
        }
    }
    else
    {
        FVector Direction = ToTarget.GetSafeNormal();
        FVector NewPos = CurrentPos + Direction * MoveDist;
        DistanceAlongRoute += MoveDist;

        // Smooth rotation
        FRotator TargetRot = Direction.Rotation();
        FRotator NewRot = FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaTime, TurnRate);

        SetActorLocationAndRotation(NewPos, NewRot);
    }
}

void AGhostVehicle::UpdateVisuals()
{
    for (int32 i = 0; i < GhostMesh->GetNumMaterials(); ++i)
    {
        UMaterialInstanceDynamic* DynMat = GhostMesh->CreateAndSetMaterialInstanceDynamic(i);
        if (DynMat)
        {
            DynMat->SetScalarParameterValue(TEXT("Opacity"), GhostAlpha);
            DynMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.0f, 0.8f, 1.0f));
        }
    }
}

FVector AGhostVehicle::GetCurrentTarget() const
{
    if (CurrentWaypointIndex < RouteWaypoints.Num() - 1)
        return RouteWaypoints[CurrentWaypointIndex + 1];
    return GetActorLocation();
}

float AGhostVehicle::GetDistanceToTarget() const
{
    if (CurrentWaypointIndex < RouteWaypoints.Num() - 1)
        return FVector::Distance(GetActorLocation(), RouteWaypoints[CurrentWaypointIndex + 1]);
    return 0.0f;
}
