#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AkronXodrImporter.generated.h"

USTRUCT(BlueprintType)
struct FAkronRoadSegment
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString RoadId;

    UPROPERTY(BlueprintReadOnly)
    TArray<FVector> WorldPoints;

    UPROPERTY(BlueprintReadOnly)
    float WidthMeters = 7.0f;

    UPROPERTY(BlueprintReadOnly)
    int32 NumLanes = 2;
};

USTRUCT(BlueprintType)
struct FAkronRouteSpline
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString RouteId;

    UPROPERTY(BlueprintReadOnly)
    TArray<FVector> Waypoints;

    UPROPERTY(BlueprintReadOnly)
    float TotalDistanceMeters = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    TArray<FVector> CheckpointLocations;
};

USTRUCT(BlueprintType)
struct FAkronSpawnPoint
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString SpawnId;

    UPROPERTY(BlueprintReadOnly)
    FVector Location;

    UPROPERTY(BlueprintReadOnly)
    FRotator Rotation;
};

USTRUCT(BlueprintType)
struct FAkronPOI
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString PoiId;

    UPROPERTY(BlueprintReadOnly)
    FString DisplayName;

    UPROPERTY(BlueprintReadOnly)
    FString Type;

    UPROPERTY(BlueprintReadOnly)
    FVector Location;
};

UCLASS()
class RACEGPSAKRONBETA_API UAkronXodrImporter : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "raceGPS|Akron")
    static bool ImportXodr(const FString& XodrPath, TArray<FAkronRoadSegment>& OutRoads);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Akron")
    static bool LoadManifest(const FString& ManifestPath, float& OutWorldOriginX, float& OutWorldOriginY);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Akron")
    static bool LoadRouteSplines(const FString& RouteDir, TArray<FAkronRouteSpline>& OutRoutes);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Akron")
    static bool LoadSpawnPoints(const FString& ManifestPath, TArray<FAkronSpawnPoint>& OutSpawns);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Akron")
    static bool LoadPOIs(const FString& ManifestPath, TArray<FAkronPOI>& OutPOIs);

    UFUNCTION(BlueprintPure, Category = "raceGPS|Akron")
    static FVector GeoToWorld(float Lat, float Lon, float OriginLat, float OriginLon);

private:
    static float MetersPerDegreeLon(float Lat);
    static float MetersPerDegreeLat();
};
