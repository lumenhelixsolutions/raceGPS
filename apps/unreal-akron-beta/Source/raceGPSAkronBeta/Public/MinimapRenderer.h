// Copyright raceGPS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MinimapRenderer.generated.h"

USTRUCT(BlueprintType)
struct FPOIMarker
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FVector WorldLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    FString IconName = TEXT("Default");
};

/**
 * C++ backend for minimap rendering.
 * Tracks player position, rotates minimap camera, renders POI icons.
 */
UCLASS()
class RACEGPSAKRONBETA_API AMinimapRenderer : public AActor
{
    GENERATED_BODY()

public:
    AMinimapRenderer(const FObjectInitializer& ObjectInitializer);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Minimap")
    float ZoomLevel = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Minimap")
    float MapSizeWorldUnits = 5000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Minimap")
    float CameraHeight = 5000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Minimap")
    FLinearColor BackgroundColor = FLinearColor(0.02f, 0.02f, 0.05f, 0.8f);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Minimap")
    void UpdatePlayerPosition(const FVector& WorldPos);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Minimap")
    void SetZoomLevel(float Zoom);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Minimap")
    void AddPOIMarker(const FVector& Location, const FString& IconName);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Minimap")
    void ClearPOIMarkers();

    UFUNCTION(BlueprintPure, Category = "raceGPS|Minimap")
    FVector GetPlayerPosition() const { return PlayerPosition; }

    UFUNCTION(BlueprintPure, Category = "raceGPS|Minimap")
    TArray<FPOIMarker> GetPOIMarkers() const { return POIMarkers; }

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Minimap")
    TObjectPtr<class USceneCaptureComponent2D> MinimapCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Minimap")
    TObjectPtr<class USceneComponent> CameraPivot;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    UPROPERTY()
    FVector PlayerPosition = FVector::ZeroVector;

    UPROPERTY()
    TArray<FPOIMarker> POIMarkers;

    void UpdateCameraTransform();
};
