#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StreetFurnitureSpawner.generated.h"

USTRUCT()
struct FFurniturePlacement
{
    GENERATED_BODY()

    UPROPERTY()
    FVector Location;

    UPROPERTY()
    FRotator Rotation;

    UPROPERTY()
    FString Type;
};

UCLASS()
class RACEGPSAKRONBETA_API AStreetFurnitureSpawner : public AActor
{
    GENERATED_BODY()

public:
    AStreetFurnitureSpawner();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Furniture")
    FString RoadGraphJsonPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Furniture")
    int32 FurniturePerFrame = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Furniture")
    float TrafficLightSpacing = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Furniture")
    float SpawnRadius = 2000.0f;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Furniture")
    void SpawnFurnitureAsync();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Furniture")
    int32 GetTotalFurniture() const { return Placements.Num(); }

protected:
    virtual void Tick(float DeltaTime) override;

    UPROPERTY()
    TArray<FFurniturePlacement> Placements;

    int32 CurrentIndex = 0;
    bool bSpawning = false;

    void LoadIntersections();
    void SpawnBatch(int32 Count);
    void SpawnTrafficLight(const FVector& Location, const FRotator& Rotation);
    void SpawnBarrier(const FVector& Location, const FRotator& Rotation);
    void SpawnBench(const FVector& Location, const FRotator& Rotation);
};
