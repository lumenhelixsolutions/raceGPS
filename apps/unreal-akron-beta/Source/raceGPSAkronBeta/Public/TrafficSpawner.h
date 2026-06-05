#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrafficSpawner.generated.h"

UCLASS()
class RACEGPSAKRONBETA_API ATrafficSpawner : public AActor
{
    GENERATED_BODY()

public:
    ATrafficSpawner(const FObjectInitializer& ObjectInitializer);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Traffic")
    int32 MaxTrafficVehicles = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Traffic")
    float SpawnRadius = 2000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Traffic")
    float DespawnRadius = 3000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Traffic")
    float SpawnInterval = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Traffic")
    FString XodrPath = TEXT("citypacks/akron-oh-beta-001/akron.xodr");

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Traffic")
    void SpawnTraffic();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Traffic")
    void ClearTraffic();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    UPROPERTY()
    TArray<TObjectPtr<class ATrafficVehicle>> ActiveVehicles;

    UPROPERTY()
    TArray<struct FAkronRoadSegment> RoadSegments;

    float SpawnTimer = 0.0f;

    void TrySpawnVehicle();
    void DespawnDistantVehicles();
    TArray<FVector> GetRandomRoadPoints() const;
};
