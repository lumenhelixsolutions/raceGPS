#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MinimapWidget.generated.h"

UCLASS()
class RACEGPSAKRONBETA_API UMinimapWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual void NativeConstruct() override;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|HUD")
    void SetRoadData(const TArray<FVector>& RoadPoints);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|HUD")
    void SetRouteWaypoints(const TArray<FVector>& Waypoints);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|HUD")
    void SetCheckpointLocations(const TArray<FVector>& Locations);

    UPROPERTY(meta = (BindWidget))
    class UImage* MinimapCanvas;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|HUD")
    float MinimapRadiusMeters = 500.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|HUD")
    float WidgetSize = 256.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|HUD")
    FLinearColor RoadColor = FLinearColor(0.2f, 0.2f, 0.2f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|HUD")
    FLinearColor RouteColor = FLinearColor(0.0f, 0.8f, 1.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|HUD")
    FLinearColor CheckpointColor = FLinearColor(1.0f, 0.8f, 0.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|HUD")
    FLinearColor PlayerColor = FLinearColor::White;

protected:
    UPROPERTY()
    TArray<FVector> LocalRoadPoints;

    UPROPERTY()
    TArray<FVector> LocalRouteWaypoints;

    UPROPERTY()
    TArray<FVector> LocalCheckpoints;

    void DrawMinimap();
    FVector2D WorldToMinimap(const FVector& WorldPos, const FVector& PlayerPos, float PlayerYaw) const;
};
