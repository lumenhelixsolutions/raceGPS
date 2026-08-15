#include "RouteSplineActor.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"

ARouteSplineActor::ARouteSplineActor(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = false;

    Spline = CreateDefaultSubobject<USplineComponent>(TEXT("RouteSpline"));
    Spline->SetupAttachment(RootComponent);
    Spline->SetMobility(EComponentMobility::Static);
    Spline->SetClosedLoop(false);
    Spline->SetUnselectedSplineSegmentColor(FLinearColor::Gray);
    Spline->SetSelectedSplineSegmentColor(FLinearColor::Yellow);
}

void ARouteSplineActor::BeginPlay()
{
    Super::BeginPlay();
}

void ARouteSplineActor::BuildSplineFromWaypoints(const TArray<FVector>& Waypoints)
{
    if (Waypoints.Num() < 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] Route %s has fewer than 2 waypoints"), *RouteId);
        return;
    }

    ClearRoute();

    // Add spline points
    for (int32 i = 0; i < Waypoints.Num(); ++i)
    {
        Spline->AddSplinePoint(Waypoints[i], ESplineCoordinateSpace::World, false);
    }

    // Set spline type for smooth curves
    Spline->SetSplinePointType(0, ESplinePointType::Curve, false);
    for (int32 i = 1; i < Waypoints.Num() - 1; ++i)
    {
        Spline->SetSplinePointType(i, ESplinePointType::Curve, false);
    }
    Spline->SetSplinePointType(Waypoints.Num() - 1, ESplinePointType::Curve, true);

    UpdateSplineTangents();

    if (bGenerateRibbonMesh)
    {
        GenerateRibbonMesh();
    }

    if (bShowCheckpointArrows)
    {
        GenerateCheckpointArrows(Waypoints);
    }

    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Built route spline %s with %d points, length %.0fm"),
        *RouteId, Waypoints.Num(), Spline->GetSplineLength());
}

void ARouteSplineActor::ClearRoute()
{
    Spline->ClearSplinePoints(false);

    for (USplineMeshComponent* Seg : RibbonSegments)
    {
        if (Seg)
        {
            Seg->DestroyComponent();
        }
    }
    RibbonSegments.Empty();

    for (UStaticMeshComponent* Arrow : CheckpointArrows)
    {
        if (Arrow)
        {
            Arrow->DestroyComponent();
        }
    }
    CheckpointArrows.Empty();
}

void ARouteSplineActor::UpdateSplineTangents()
{
    const int32 NumPoints = Spline->GetNumberOfSplinePoints();
    if (NumPoints < 2)
        return;

    // Auto-compute tangents for smooth curves
    for (int32 i = 0; i < NumPoints; ++i)
    {
        FVector Tangent = Spline->GetTangentAtSplinePoint(i, ESplineCoordinateSpace::World);
        // Scale tangents to create smooth curves between points
        float TangentScale = SplinePointSpacing * 0.5f;
        Tangent.Normalize();
        Spline->SetTangentAtSplinePoint(i, Tangent * TangentScale, ESplineCoordinateSpace::World, false);
    }
    Spline->UpdateSpline();
}

void ARouteSplineActor::GenerateRibbonMesh()
{
    if (!Spline)
        return;

    const float SplineLength = Spline->GetSplineLength();
    if (SplineLength <= 0.0f)
        return;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
    if (!PlaneMesh.Succeeded())
        return;

    const int32 NumSegments = FMath::Max(1, FMath::FloorToInt(SplineLength / SplinePointSpacing));
    const float SegmentLength = SplineLength / NumSegments;

    for (int32 i = 0; i < NumSegments; ++i)
    {
        float DistStart = i * SegmentLength;
        float DistEnd = (i + 1) * SegmentLength;

        FVector StartPos = Spline->GetLocationAtDistanceAlongSpline(DistStart, ESplineCoordinateSpace::World);
        FVector EndPos = Spline->GetLocationAtDistanceAlongSpline(DistEnd, ESplineCoordinateSpace::World);
        FVector StartTangent = Spline->GetTangentAtDistanceAlongSpline(DistStart, ESplineCoordinateSpace::World).GetSafeNormal();
        FVector EndTangent = Spline->GetTangentAtDistanceAlongSpline(DistEnd, ESplineCoordinateSpace::World).GetSafeNormal();

        // Lift ribbon slightly above ground
        StartPos.Z += RibbonThickness * 0.5f;
        EndPos.Z += RibbonThickness * 0.5f;

        USplineMeshComponent* SplineMesh = NewObject<USplineMeshComponent>(this);
        SplineMesh->RegisterComponent();
        SplineMesh->SetStaticMesh(PlaneMesh.Object);
        SplineMesh->SetForwardAxis(ESplineMeshAxis::X);

        SplineMesh->SetStartAndEnd(StartPos, StartTangent * SegmentLength, EndPos, EndTangent * SegmentLength, true);
        SplineMesh->SetStartScale(FVector2D(1.0f, RibbonWidth / 100.0f));
        SplineMesh->SetEndScale(FVector2D(1.0f, RibbonWidth / 100.0f));

        // Material
        UMaterialInstanceDynamic* DynMat = SplineMesh->CreateAndSetMaterialInstanceDynamic(0);
        if (DynMat)
        {
            DynMat->SetVectorParameterValue(TEXT("Color"), RouteColor);
            DynMat->SetScalarParameterValue(TEXT("Opacity"), 0.6f);
        }

        RibbonSegments.Add(SplineMesh);
    }
}

void ARouteSplineActor::GenerateCheckpointArrows(const TArray<FVector>& Waypoints)
{
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ArrowMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
    if (!ArrowMesh.Succeeded())
        return;

    // Place arrows at every Nth waypoint
    const int32 ArrowInterval = FMath::Max(1, Waypoints.Num() / 10);
    for (int32 i = ArrowInterval; i < Waypoints.Num(); i += ArrowInterval)
    {
        FVector Location = Waypoints[i];
        Location.Z += 300.0f;

        UStaticMeshComponent* Arrow = NewObject<UStaticMeshComponent>(this);
        Arrow->RegisterComponent();
        Arrow->SetStaticMesh(ArrowMesh.Object);
        Arrow->SetWorldLocation(Location);
        Arrow->SetWorldScale3D(FVector(0.5f, 0.5f, 1.0f));
        Arrow->SetWorldRotation(FRotator(-90.0f, 0.0f, 0.0f));

        UMaterialInstanceDynamic* DynMat = Arrow->CreateAndSetMaterialInstanceDynamic(0);
        if (DynMat)
        {
            DynMat->SetVectorParameterValue(TEXT("Color"), FLinearColor::Yellow);
        }

        CheckpointArrows.Add(Arrow);
    }
}

FVector ARouteSplineActor::GetClosestPointOnSpline(const FVector& WorldLocation) const
{
    if (!Spline)
        return WorldLocation;
    return Spline->FindLocationClosestToWorldLocation(WorldLocation, ESplineCoordinateSpace::World);
}

float ARouteSplineActor::GetDistanceAlongSpline(const FVector& WorldLocation) const
{
    if (!Spline)
        return 0.0f;
    float InputKey = Spline->FindInputKeyClosestToWorldLocation(WorldLocation);
    return Spline->GetDistanceAlongSplineAtSplineInputKey(InputKey);
}

FVector ARouteSplineActor::GetLocationAtDistance(float Distance) const
{
    if (!Spline)
        return FVector::ZeroVector;
    return Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
}

FRotator ARouteSplineActor::GetRotationAtDistance(float Distance) const
{
    if (!Spline)
        return FRotator::ZeroRotator;
    FVector Tangent = Spline->GetTangentAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
    return Tangent.Rotation();
}
