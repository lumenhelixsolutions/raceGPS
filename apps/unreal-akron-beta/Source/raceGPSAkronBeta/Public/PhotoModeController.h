// Copyright raceGPS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PhotoModeController.generated.h"

/**
 * Free camera photo mode controller.
 */
UCLASS()
class RACEGPSAKRONBETA_API APhotoModeController : public AActor
{
    GENERATED_BODY()

public:
    APhotoModeController(const FObjectInitializer& ObjectInitializer);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|PhotoMode")
    float CameraMoveSpeed = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|PhotoMode")
    float CameraRotateSpeed = 90.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|PhotoMode")
    TMap<FString, FLinearColor> FilterPresets;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|PhotoMode")
    void EnterPhotoMode();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|PhotoMode")
    void ExitPhotoMode();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|PhotoMode")
    void SetCameraPosition(const FVector& Pos, const FRotator& Rot);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|PhotoMode")
    void ApplyFilter(const FString& FilterName);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|PhotoMode")
    void CaptureScreenshot();

    UFUNCTION(BlueprintPure, Category = "raceGPS|PhotoMode")
    bool IsInPhotoMode() const { return bPhotoModeActive; }

    UFUNCTION(BlueprintPure, Category = "raceGPS|PhotoMode")
    FVector GetCameraLocation() const;

    UFUNCTION(BlueprintPure, Category = "raceGPS|PhotoMode")
    FRotator GetCameraRotation() const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|PhotoMode")
    TObjectPtr<class UCameraComponent> FreeCamera;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    UPROPERTY()
    bool bPhotoModeActive = false;

    UPROPERTY()
    FVector CachedPlayerLocation = FVector::ZeroVector;

    UPROPERTY()
    FRotator CachedPlayerRotation = FRotator::ZeroRotator;

    UPROPERTY()
    FString CurrentFilter = TEXT("None");

    void InitializeFilters();
    void ResetPostProcess();
};
