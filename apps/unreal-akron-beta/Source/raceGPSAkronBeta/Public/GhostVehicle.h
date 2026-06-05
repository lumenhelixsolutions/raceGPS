#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GhostVehicle.generated.h"

UCLASS()
class RACEGPSAKRONBETA_API AGhostVehicle : public AActor
{
    GENERATED_BODY()

public:
    AGhostVehicle(const FObjectInitializer& ObjectInitializer);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Ghost")
    float TargetSpeedKmh = 80.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Ghost")
    float Acceleration = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Ghost")
    float TurnRate = 90.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Ghost")
    float PathFollowDistance = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Ghost")
    float GhostAlpha = 0.4f;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Ghost")
    void SetRouteWaypoints(const TArray<FVector>& Waypoints);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Ghost")
    void StartGhostRun(float DelaySeconds);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Ghost")
    void StopGhostRun();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Ghost")
    bool IsRunning() const { return bRunning; }

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Ghost")
    float GetDistanceAlongRoute() const { return DistanceAlongRoute; }

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Ghost")
    TObjectPtr<class USkeletalMeshComponent> GhostMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Ghost")
    TObjectPtr<class UNiagaraComponent> TrailEffect;

    UPROPERTY()
    TArray<FVector> RouteWaypoints;

    UPROPERTY()
    int32 CurrentWaypointIndex = 0;

    float DistanceAlongRoute = 0.0f;
    float CurrentSpeed = 0.0f;
    bool bRunning = false;

    void MoveAlongRoute(float DeltaTime);
    void UpdateVisuals();
    FVector GetCurrentTarget() const;
    float GetDistanceToTarget() const;
};
