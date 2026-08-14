#include "RoadMeshGenerator.h"
#include "AkronXodrImporter.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

ARoadMeshGenerator::ARoadMeshGenerator(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
}

void ARoadMeshGenerator::BeginPlay()
{
    Super::BeginPlay();
}

void ARoadMeshGenerator::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bAsyncGenerating && PendingIndex < PendingRoads.Num())
    {
        ProcessBatch();
    }
    else if (bAsyncGenerating)
    {
        bAsyncGenerating = false;
        bGenerationComplete = true;
        PrimaryActorTick.bStartWithTickEnabled = false;
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Road mesh generation complete: %d roads"), TotalRoadsGenerated);
    }
}

void ARoadMeshGenerator::GenerateRoadMeshes()
{
    ClearRoadMeshes();

    TArray<FAkronRoadSegment> Roads;
    if (!UAkronXodrImporter::ImportXodr(XodrPath, Roads))
    {
        UE_LOG(LogTemp, Error, TEXT("[raceGPS] Failed to import roads for mesh generation"));
        return;
    }

    for (const FAkronRoadSegment& Segment : Roads)
    {
        if (Segment.WorldPoints.Num() < 2)
            continue;

        UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
        Mesh->RegisterComponent();
        Mesh->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
        Mesh->bUseAsyncCooking = true;
        Mesh->bUseComplexAsSimpleCollision = true;

        GenerateRoadMesh(Segment, Mesh);
        RoadMeshes.Add(Mesh);
    }

    TotalRoadsGenerated = RoadMeshes.Num();
    bGenerationComplete = true;
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Generated %d road meshes"), TotalRoadsGenerated);
}

void ARoadMeshGenerator::GenerateRoadMeshAsync()
{
    ClearRoadMeshes();

    PendingRoads.Empty();
    if (!UAkronXodrImporter::ImportXodr(XodrPath, PendingRoads))
    {
        UE_LOG(LogTemp, Error, TEXT("[raceGPS] Failed to import roads for async mesh generation"));
        return;
    }

    PendingIndex = 0;
    bAsyncGenerating = true;
    bGenerationComplete = false;
    PrimaryActorTick.bStartWithTickEnabled = true;

    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Starting async road mesh generation: %d roads"), PendingRoads.Num());
}

void ARoadMeshGenerator::ProcessBatch()
{
    const int32 EndIndex = FMath::Min(PendingIndex + MaxRoadsPerFrame, PendingRoads.Num());

    for (int32 i = PendingIndex; i < EndIndex; ++i)
    {
        const FAkronRoadSegment& Segment = PendingRoads[i];
        if (Segment.WorldPoints.Num() < 2)
            continue;

        UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
        Mesh->RegisterComponent();
        Mesh->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
        Mesh->bUseAsyncCooking = true;
        Mesh->bUseComplexAsSimpleCollision = true;

        GenerateRoadMesh(Segment, Mesh);
        RoadMeshes.Add(Mesh);
        TotalRoadsGenerated++;
    }

    PendingIndex = EndIndex;
}

void ARoadMeshGenerator::GenerateRoadMesh(const FAkronRoadSegment& Segment, UProceduralMeshComponent* Mesh)
{
    const TArray<FVector>& Points = Segment.WorldPoints;
    const int32 NumPoints = Points.Num();
    const float HalfWidth = Segment.WidthMeters * 0.5f;

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FProcMeshTangent> Tangents;
    TArray<FColor> VertexColors;

    Vertices.Reserve(NumPoints * 2);
    Triangles.Reserve((NumPoints - 1) * 6);
    Normals.Reserve(NumPoints * 2);
    UVs.Reserve(NumPoints * 2);
    Tangents.Reserve(NumPoints * 2);

    float CumulativeLength = 0.0f;

    for (int32 i = 0; i < NumPoints; ++i)
    {
        FVector Forward;
        if (i == 0)
        {
            Forward = (Points[1] - Points[0]).GetSafeNormal();
        }
        else if (i == NumPoints - 1)
        {
            Forward = (Points[NumPoints - 1] - Points[NumPoints - 2]).GetSafeNormal();
        }
        else
        {
            Forward = ((Points[i] - Points[i - 1]) + (Points[i + 1] - Points[i])).GetSafeNormal();
        }

        FVector Right = FVector::CrossProduct(Forward, FVector::UpVector).GetSafeNormal();
        FVector Left = -Right;

        FVector Center = Points[i];
        Center.Z += RoadHeightOffset;

        FVector LeftPos = Center + Left * HalfWidth;
        FVector RightPos = Center + Right * HalfWidth;

        Vertices.Add(LeftPos);
        Vertices.Add(RightPos);

        FVector Normal = bUseFlatShading ? FVector::UpVector : FVector::CrossProduct(Right, Forward).GetSafeNormal();
        Normals.Add(Normal);
        Normals.Add(Normal);

        UVs.Add(FVector2D(0.0f, CumulativeLength * 0.01f));
        UVs.Add(FVector2D(1.0f, CumulativeLength * 0.01f));

        Tangents.Add(FProcMeshTangent(Forward, false));
        Tangents.Add(FProcMeshTangent(Forward, false));

        VertexColors.Add(FColor::White);
        VertexColors.Add(FColor::White);

        if (i > 0)
        {
            const int32 Base = (i - 1) * 2;
            // Triangle 1: left_prev, left_curr, right_prev
            Triangles.Add(Base);
            Triangles.Add(Base + 2);
            Triangles.Add(Base + 1);
            // Triangle 2: left_curr, right_curr, right_prev
            Triangles.Add(Base + 2);
            Triangles.Add(Base + 3);
            Triangles.Add(Base + 1);

            CumulativeLength += FVector::Distance(Points[i - 1], Points[i]);
        }
    }

    Mesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, true);

    // Simple asphalt material
    static ConstructorHelpers::FObjectFinder<UMaterial> RoadMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (RoadMat.Succeeded())
    {
        UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(RoadMat.Object, this);
        if (DynMat)
        {
            DynMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.15f, 0.15f, 0.15f));
            Mesh->SetMaterial(0, DynMat);
        }
    }
}

void ARoadMeshGenerator::ClearRoadMeshes()
{
    for (UProceduralMeshComponent* Mesh : RoadMeshes)
    {
        if (Mesh)
        {
            Mesh->DestroyComponent();
        }
    }
    RoadMeshes.Empty();
    PendingRoads.Empty();
    PendingIndex = 0;
    bAsyncGenerating = false;
    bGenerationComplete = false;
    TotalRoadsGenerated = 0;
}
