// Copyright raceGPS. All Rights Reserved.

#include "MinimapRenderer.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneComponent.h"
#include "Engine/TextureRenderTarget2D.h"

AMinimapRenderer::AMinimapRenderer(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;

    CameraPivot = CreateDefaultSubobject<USceneComponent>(TEXT("CameraPivot"));
    RootComponent = CameraPivot;

    MinimapCamera = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("MinimapCamera"));
    MinimapCamera->SetupAttachment(CameraPivot);
    MinimapCamera->ProjectionType = ECameraProjectionMode::Orthographic;
    MinimapCamera->OrthoWidth = MapSizeWorldUnits;
    MinimapCamera->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    MinimapCamera->bCaptureEveryFrame = true;
    MinimapCamera->bCaptureOnMovement = false;

    // Default render target
    UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("MinimapRenderTarget"));
    RenderTarget->InitAutoFormat(512, 512);
    RenderTarget->ClearColor = BackgroundColor;
    MinimapCamera->TextureTarget = RenderTarget;
}

void AMinimapRenderer::BeginPlay()
{
    Super::BeginPlay();
    UpdateCameraTransform();
}

void AMinimapRenderer::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    UpdateCameraTransform();
}

void AMinimapRenderer::UpdatePlayerPosition(const FVector& WorldPos)
{
    PlayerPosition = WorldPos;
}

void AMinimapRenderer::SetZoomLevel(float Zoom)
{
    ZoomLevel = FMath::Clamp(Zoom, 0.1f, 10.0f);
    if (MinimapCamera)
    {
        MinimapCamera->OrthoWidth = MapSizeWorldUnits / ZoomLevel;
    }
}

void AMinimapRenderer::AddPOIMarker(const FVector& Location, const FString& IconName)
{
    FPOIMarker Marker;
    Marker.WorldLocation = Location;
    Marker.IconName = IconName;
    POIMarkers.Add(Marker);
}

void AMinimapRenderer::ClearPOIMarkers()
{
    POIMarkers.Empty();
}

void AMinimapRenderer::UpdateCameraTransform()
{
    if (!CameraPivot || !MinimapCamera)
        return;

    FVector CamPos = PlayerPosition;
    CamPos.Z += CameraHeight;
    CameraPivot->SetWorldLocation(CamPos);

    // Keep camera looking straight down, but rotate with player yaw
    FRotator CamRot = FRotator(-90.0f, 0.0f, 0.0f);
    MinimapCamera->SetWorldRotation(CamRot);
}
