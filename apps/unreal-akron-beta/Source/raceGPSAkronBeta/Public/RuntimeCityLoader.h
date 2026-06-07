// Copyright raceGPS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RuntimeCityLoader.generated.h"

/**
 * Runtime citypack loader for open-world streaming.
 * Loads citypack JSON at runtime and spawns actors dynamically.
 */
UCLASS()
class RACEGPSAKRONBETA_API ARuntimeCityLoader : public AActor
{
    GENERATED_BODY()

public:
    ARuntimeCityLoader();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citypack")
    FString CitypackPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citypack")
    bool bLoadOnBeginPlay = true;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Citypack")
    bool bLoaded = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Citypack")
    FString LoadedCityId;

    UFUNCTION(BlueprintCallable, Category = "Citypack")
    bool LoadCitypack(const FString& Path);

    UFUNCTION(BlueprintCallable, Category = "Citypack")
    bool UnloadCitypack();

    UFUNCTION(BlueprintCallable, Category = "Citypack")
    void StreamChunk(const FVector& PlayerLocation, float RadiusMeters);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    void SpawnRoadSpline(const TArray<FVector>& Points, const FString& RouteId);
    void SpawnCheckpoint(const FVector& Location, float Radius, const FString& Id);
    void SpawnPOI(const FVector& Location, const FString& Type, const FString& Name);
    void SpawnWaterBody(const TArray<FVector>& Points, const FString& Type);
    void SpawnVegetationZone(const FVector& Center, float Radius, const FString& Type, float Density);

    UPROPERTY()
    TArray<AActor*> SpawnedActors;

    FTimerHandle StreamTimer;
    void OnStreamTick();
};
