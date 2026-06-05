#include "CheckpointGate.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

ACheckpointGate::ACheckpointGate(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = false;

    OverlapBox = CreateDefaultSubobject<UBoxComponent>(TEXT("OverlapBox"));
    OverlapBox->SetBoxExtent(FVector(GateDepth * 0.5f, GateWidth * 0.5f, GateHeight));
    OverlapBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    OverlapBox->SetGenerateOverlapEvents(true);
    RootComponent = OverlapBox;

    // Placeholder meshes — replace with actual assets in editor
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (CylinderMesh.Succeeded())
    {
        // Left post
        UStaticMeshComponent* LeftPost = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftPost"));
        LeftPost->SetStaticMesh(CylinderMesh.Object);
        LeftPost->SetRelativeLocation(FVector(0.0f, -GateWidth * 0.5f, GateHeight * 0.5f));
        LeftPost->SetRelativeScale3D(FVector(0.1f, 0.1f, GateHeight * 0.01f));
        LeftPost->SetupAttachment(RootComponent);
        VisualComponents.Add(LeftPost);

        // Right post
        UStaticMeshComponent* RightPost = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightPost"));
        RightPost->SetStaticMesh(CylinderMesh.Object);
        RightPost->SetRelativeLocation(FVector(0.0f, GateWidth * 0.5f, GateHeight * 0.5f));
        RightPost->SetRelativeScale3D(FVector(0.1f, 0.1f, GateHeight * 0.01f));
        RightPost->SetupAttachment(RootComponent);
        VisualComponents.Add(RightPost);
    }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        // Arch beam
        UStaticMeshComponent* Arch = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArchBeam"));
        Arch->SetStaticMesh(CubeMesh.Object);
        Arch->SetRelativeLocation(FVector(0.0f, 0.0f, GateHeight));
        Arch->SetRelativeScale3D(FVector(0.1f, GateWidth * 0.01f, 0.05f));
        Arch->SetupAttachment(RootComponent);
        VisualComponents.Add(Arch);
    }
}

void ACheckpointGate::BeginPlay()
{
    Super::BeginPlay();

    OverlapBox->OnComponentBeginOverlap.AddDynamic(this, &ACheckpointGate::OnOverlapBegin);
    UpdateVisuals();
}

void ACheckpointGate::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                      UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                      bool bFromSweep, const FHitResult& SweepResult)
{
    if (!bIsActive || !OtherActor)
        return;

    // Check if overlapping actor is the player vehicle
    APawn* PlayerPawn = GetWorld()->GetFirstPlayerController()->GetPawn();
    if (OtherActor == PlayerPawn)
    {
        DeactivateGate();
        OnCheckpointReached.Broadcast(CheckpointIndex);
    }
}

void ACheckpointGate::ActivateGate()
{
    bIsActive = true;
    UpdateVisuals();
}

void ACheckpointGate::DeactivateGate()
{
    bIsActive = false;
    UpdateVisuals();
}

void ACheckpointGate::SetGateColor(const FLinearColor& Color)
{
    GateColor = Color;
    UpdateVisuals();
}

void ACheckpointGate::UpdateVisuals()
{
    FLinearColor ActiveColor = bIsActive ? GateColor : FLinearColor::Gray;
    ActiveColor.A = bIsActive ? 1.0f : 0.3f;

    for (UStaticMeshComponent* Visual : VisualComponents)
    {
        if (!Visual)
            continue;

        UMaterialInstanceDynamic* DynMat = Visual->CreateAndSetMaterialInstanceDynamic(0);
        if (DynMat)
        {
            DynMat->SetVectorParameterValue(TEXT("Color"), ActiveColor);
        }
        else
        {
            // Fallback: tint the whole mesh
            Visual->SetRenderCustomDepth(bIsActive);
            Visual->SetCustomDepthStencilValue(bIsActive ? 1 : 0);
        }
    }
}

void ACheckpointGate::BuildVisuals()
{
    // Visuals are built in constructor; this is a hook for runtime rebuilds
    UpdateVisuals();
}
