#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AkronXodrImporter.generated.h"

class FJsonObject;
class FJsonValue;

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

/**
 * Resolved on-disk layout of the active citypack. All paths are project-relative
 * (same convention as ACruiseSprintGameMode::CityPackPath, e.g. "../../citypacks/...").
 * Empty strings mean "not present / not applicable" (e.g. embedded arrays, missing
 * optional files such as buildings).
 */
USTRUCT(BlueprintType)
struct FRaceGPSCityLayout
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString CityId;

    UPROPERTY(BlueprintReadOnly)
    FString DisplayName;

    UPROPERTY(BlueprintReadOnly)
    FString CitypackDir;

    UPROPERTY(BlueprintReadOnly)
    FString ManifestPath;

    /** Route data: either a single JSON array file (shipped packs) or a directory of per-route files (legacy). */
    UPROPERTY(BlueprintReadOnly)
    FString RoutesPath;

    /** Spawn/POI file paths; empty when the manifest embeds the arrays directly. */
    UPROPERTY(BlueprintReadOnly)
    FString SpawnPointsPath;

    UPROPERTY(BlueprintReadOnly)
    FString POIsPath;

    UPROPERTY(BlueprintReadOnly)
    FString RoadGraphPath;

    UPROPERTY(BlueprintReadOnly)
    FString XodrPath;

    UPROPERTY(BlueprintReadOnly)
    FString BuildingsPath;

    UPROPERTY(BlueprintReadOnly)
    FString LevelSpecPath;

    /** level_name from the level spec; empty when no level spec was found. */
    UPROPERTY(BlueprintReadOnly)
    FString LevelName;
};

UCLASS()
class RACEGPSAKRONBETA_API UAkronXodrImporter : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "raceGPS|Akron")
    static bool ImportXodr(const FString& XodrPath, TArray<FAkronRoadSegment>& OutRoads);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Akron")
    static bool LoadRoadGraphJson(const FString& JsonPath, TArray<FAkronRoadSegment>& OutRoads);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Akron")
    static bool LoadManifest(const FString& ManifestPath, float& OutWorldOriginLat, float& OutWorldOriginLon);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Akron")
    static bool LoadRouteSplines(const FString& RoutePathOrDir, TArray<FAkronRouteSpline>& OutRoutes);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Akron")
    static bool LoadSpawnPoints(const FString& ManifestPath, TArray<FAkronSpawnPoint>& OutSpawns);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Akron")
    static bool LoadPOIs(const FString& ManifestPath, TArray<FAkronPOI>& OutPOIs);

    /**
     * Active city id. Resolution order (first non-empty wins):
     *   1. command line:  -racegps.city=<city_id>
     *   2. console var:   racegps.CityId (also settable via [ConsoleVariables] ini)
     *   3. DefaultGame.ini [RaceGPS.CitySelection] CityId
     *   4. built-in default "akron-oh-beta-001"
     */
    UFUNCTION(BlueprintPure, Category = "raceGPS|City")
    static FString GetActiveCityId();

    /**
     * Resolve the on-disk layout of the active citypack, tolerating both manifest
     * dialects (flat refs / "files" dict). Returns false when no manifest is found.
     */
    UFUNCTION(BlueprintCallable, Category = "raceGPS|City")
    static bool ResolveCityLayout(FRaceGPSCityLayout& OutLayout);

    /**
     * Resolve a manifest file reference (flat field or files.<field>) to a
     * project-relative path. Returns false when the field is absent or not a string
     * (e.g. an embedded array).
     */
    UFUNCTION(BlueprintCallable, Category = "raceGPS|City")
    static bool ResolveManifestDataFile(const FString& ManifestPath, const FString& FieldName, FString& OutPath);

    UFUNCTION(BlueprintPure, Category = "raceGPS|Akron")
    static FVector GeoToWorld(float Lat, float Lon, float OriginLat, float OriginLon);

private:
    static float MetersPerDegreeLon(float Lat);
    static float MetersPerDegreeLat();
    static FVector XodrToWorld(float X, float Y, float OriginLat, float OriginLon);

    /** Shared helpers for the dialect-tolerant loaders. */
    static bool LoadJsonObjectFile(const FString& ProjectRelativePath, TSharedPtr<FJsonObject>& OutRoot);
    static bool GetManifestFileRef(const TSharedPtr<FJsonObject>& Root, const FString& FieldName, FString& OutFileName);
    static FString GetManifestDir(const FString& ManifestPath);
    static void ParseRouteObject(const TSharedPtr<FJsonObject>& RouteObj, const FString& FallbackRouteId, FAkronRouteSpline& OutRoute);
    static void ParseSpawnArray(const TArray<TSharedPtr<FJsonValue>>& Spawns, TArray<FAkronSpawnPoint>& OutSpawns);
    static void ParsePOIArray(const TArray<TSharedPtr<FJsonValue>>& Pois, TArray<FAkronPOI>& OutPOIs);
};
