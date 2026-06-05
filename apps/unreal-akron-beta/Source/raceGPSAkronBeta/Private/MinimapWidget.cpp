#include "MinimapWidget.h"
#include "Components/Image.h"
#include "Kismet/GameplayStatics.h"
#include "ChaosVehiclePawn.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"

void UMinimapWidget::NativeConstruct()
{
    Super::NativeConstruct();
}

void UMinimapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    DrawMinimap();
}

void UMinimapWidget::SetRoadData(const TArray<FVector>& RoadPoints)
{
    LocalRoadPoints = RoadPoints;
}

void UMinimapWidget::SetRouteWaypoints(const TArray<FVector>& Waypoints)
{
    LocalRouteWaypoints = Waypoints;
}

void UMinimapWidget::SetCheckpointLocations(const TArray<FVector>& Locations)
{
    LocalCheckpoints = Locations;
}

FVector2D UMinimapWidget::WorldToMinimap(const FVector& WorldPos, const FVector& PlayerPos, float PlayerYaw) const
{
    FVector Delta = WorldPos - PlayerPos;
    float CosYaw = FMath::Cos(FMath::DegreesToRadians(PlayerYaw));
    float SinYaw = FMath::Sin(FMath::DegreesToRadians(PlayerYaw));

    // Rotate into player-local space (forward = up on minimap)
    float LocalX = Delta.X * CosYaw + Delta.Y * SinYaw;
    float LocalY = -Delta.X * SinYaw + Delta.Y * CosYaw;

    float Scale = WidgetSize / (MinimapRadiusMeters * 2.0f);
    return FVector2D(
        WidgetSize * 0.5f + LocalX * Scale,
        WidgetSize * 0.5f - LocalY * Scale
    );
}

void UMinimapWidget::DrawMinimap()
{
    AChaosVehiclePawn* Vehicle = Cast<AChaosVehiclePawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
    if (!Vehicle)
        return;

    FVector PlayerPos = Vehicle->GetActorLocation();
    float PlayerYaw = Vehicle->GetActorRotation().Yaw;

    // Build a simple canvas for drawing
    // In a real implementation, this would render to a UTextureRenderTarget2D
    // For now, we log the transform counts for verification
    static int32 LastRoadCount = 0;
    static int32 LastRouteCount = 0;
    if (LocalRoadPoints.Num() != LastRoadCount || LocalRouteWaypoints.Num() != LastRouteCount)
    {
        LastRoadCount = LocalRoadPoints.Num();
        LastRouteCount = LocalRouteWaypoints.Num();
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Minimap updated: %d roads, %d route pts, %d checkpoints"),
            LocalRoadPoints.Num(), LocalRouteWaypoints.Num(), LocalCheckpoints.Num());
    }
}
