#include "AkronXodrImporter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Math/UnrealMathUtility.h"

const float EARTH_RADIUS_M = 6371000.0f;

float UAkronXodrImporter::MetersPerDegreeLon(float Lat)
{
    float Rad = FMath::DegreesToRadians(Lat);
    return 111320.0f * FMath::Cos(Rad);
}

float UAkronXodrImporter::MetersPerDegreeLat()
{
    return 110540.0f;
}

FVector UAkronXodrImporter::GeoToWorld(float Lat, float Lon, float OriginLat, float OriginLon)
{
    float MetersPerLon = MetersPerDegreeLon(OriginLat);
    float MetersPerLat = MetersPerDegreeLat();
    float X = (Lon - OriginLon) * MetersPerLon;
    float Y = 0.0f;
    float Z = -(Lat - OriginLat) * MetersPerLat;
    return FVector(X, Y, Z);
}

bool UAkronXodrImporter::ImportXodr(const FString& XodrPath, TArray<FAkronRoadSegment>& OutRoads)
{
    FString FullPath = FPaths::ProjectDir() / XodrPath;
    FString Content;
    if (!FFileHelper::LoadFileToString(Content, *FullPath))
    {
        UE_LOG(LogTemp, Error, TEXT("[raceGPS] Failed to load XODR: %s"), *FullPath);
        return false;
    }

    // TODO: Parse OpenDRIVE XML properly via pugixml or rapidxml.
    // For now we stub with a single representative road so the level loads.
    UE_LOG(LogTemp, Warning, TEXT("[raceGPS] XODR XML parser not yet implemented; using stub road data."));

    // Stub: one long straight road segment for validation
    FAkronRoadSegment Stub;
    Stub.RoadId = TEXT("stub_road_001");
    Stub.WidthMeters = 7.0f;
    Stub.NumLanes = 2;
    Stub.WorldPoints.Add(FVector(0.0f, 0.0f, 0.0f));
    Stub.WorldPoints.Add(FVector(1000.0f, 0.0f, 0.0f));
    OutRoads.Add(Stub);

    return true;
}

bool UAkronXodrImporter::LoadManifest(const FString& ManifestPath, float& OutWorldOriginX, float& OutWorldOriginY)
{
    FString FullPath = FPaths::ProjectDir() / ManifestPath;
    FString Content;
    if (!FFileHelper::LoadFileToString(Content, *FullPath))
    {
        UE_LOG(LogTemp, Error, TEXT("[raceGPS] Failed to load manifest: %s"), *FullPath);
        return false;
    }

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
    if (!FJsonSerializer::Deserialize(Reader, Root))
    {
        UE_LOG(LogTemp, Error, TEXT("[raceGPS] Failed to parse manifest JSON"));
        return false;
    }

    const TSharedPtr<FJsonObject>* BoundsObj;
    if (Root->TryGetObjectField(TEXT("bounds"), BoundsObj))
    {
        double OriginLat = 0.0, OriginLon = 0.0;
        (*BoundsObj)->TryGetNumberField(TEXT("lat_min"), OriginLat);
        (*BoundsObj)->TryGetNumberField(TEXT("lon_min"), OriginLon);
        OutWorldOriginX = static_cast<float>(OriginLon);
        OutWorldOriginY = static_cast<float>(OriginLat);
    }

    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Manifest loaded. Origin: (%f, %f)"), OutWorldOriginX, OutWorldOriginY);
    return true;
}

bool UAkronXodrImporter::LoadRouteSplines(const FString& RouteDir, TArray<FAkronRouteSpline>& OutRoutes)
{
    FString FullDir = FPaths::ProjectDir() / RouteDir;
    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files, *(FullDir / TEXT("*.json")), true, false);

    for (const FString& File : Files)
    {
        FString Content;
        if (!FFileHelper::LoadFileToString(Content, *(FullDir / File))) continue;

        TSharedPtr<FJsonObject> Root;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
        if (!FJsonSerializer::Deserialize(Reader, Root)) continue;

        FAkronRouteSpline Route;
        Route.RouteId = FPaths::GetBaseFilename(File);
        Root->TryGetNumberField(TEXT("distance_m"), Route.TotalDistanceMeters);

        const TArray<TSharedPtr<FJsonValue>>* Points;
        if (Root->TryGetArrayField(TEXT("points"), Points))
        {
            for (const TSharedPtr<FJsonValue>& Val : *Points)
            {
                const TSharedPtr<FJsonObject>* Pt;
                if (Val->TryGetObject(Pt))
                {
                    double Lat = 0.0, Lon = 0.0;
                    (*Pt)->TryGetNumberField(TEXT("lat"), Lat);
                    (*Pt)->TryGetNumberField(TEXT("lon"), Lon);
                    // Note: GeoToWorld requires origin; caller must offset afterward
                    Route.Waypoints.Add(FVector(static_cast<float>(Lon), 0.0f, -static_cast<float>(Lat)));
                }
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* Cps;
        if (Root->TryGetArrayField(TEXT("checkpoints"), Cps))
        {
            for (const TSharedPtr<FJsonValue>& Val : *Cps)
            {
                const TSharedPtr<FJsonObject>* Cp;
                if (Val->TryGetObject(Cp))
                {
                    double Lat = 0.0, Lon = 0.0;
                    (*Cp)->TryGetNumberField(TEXT("lat"), Lat);
                    (*Cp)->TryGetNumberField(TEXT("lon"), Lon);
                    Route.CheckpointLocations.Add(FVector(static_cast<float>(Lon), 0.0f, -static_cast<float>(Lat)));
                }
            }
        }

        OutRoutes.Add(Route);
    }

    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Loaded %d route splines from %s"), OutRoutes.Num(), *FullDir);
    return OutRoutes.Num() > 0;
}

bool UAkronXodrImporter::LoadSpawnPoints(const FString& ManifestPath, TArray<FAkronSpawnPoint>& OutSpawns)
{
    FString FullPath = FPaths::ProjectDir() / ManifestPath;
    FString Content;
    if (!FFileHelper::LoadFileToString(Content, *FullPath)) return false;

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
    if (!FJsonSerializer::Deserialize(Reader, Root)) return false;

    const TArray<TSharedPtr<FJsonValue>>* Spawns;
    if (Root->TryGetArrayField(TEXT("spawn_points"), Spawns))
    {
        for (const TSharedPtr<FJsonValue>& Val : *Spawns)
        {
            const TSharedPtr<FJsonObject>* Obj;
            if (Val->TryGetObject(Obj))
            {
                FAkronSpawnPoint Sp;
                (*Obj)->TryGetStringField(TEXT("id"), Sp.SpawnId);
                double Lat = 0.0, Lon = 0.0, Heading = 0.0;
                (*Obj)->TryGetNumberField(TEXT("lat"), Lat);
                (*Obj)->TryGetNumberField(TEXT("lon"), Lon);
                (*Obj)->TryGetNumberField(TEXT("heading"), Heading);
                Sp.Location = FVector(static_cast<float>(Lon), 0.0f, -static_cast<float>(Lat));
                Sp.Rotation = FRotator(0.0f, static_cast<float>(Heading), 0.0f);
                OutSpawns.Add(Sp);
            }
        }
    }

    return OutSpawns.Num() > 0;
}

bool UAkronXodrImporter::LoadPOIs(const FString& ManifestPath, TArray<FAkronPOI>& OutPOIs)
{
    FString FullPath = FPaths::ProjectDir() / ManifestPath;
    FString Content;
    if (!FFileHelper::LoadFileToString(Content, *FullPath)) return false;

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
    if (!FJsonSerializer::Deserialize(Reader, Root)) return false;

    const TArray<TSharedPtr<FJsonValue>>* Pois;
    if (Root->TryGetArrayField(TEXT("pois"), Pois))
    {
        for (const TSharedPtr<FJsonValue>& Val : *Pois)
        {
            const TSharedPtr<FJsonObject>* Obj;
            if (Val->TryGetObject(Obj))
            {
                FAkronPOI Poi;
                (*Obj)->TryGetStringField(TEXT("id"), Poi.PoiId);
                (*Obj)->TryGetStringField(TEXT("name"), Poi.DisplayName);
                (*Obj)->TryGetStringField(TEXT("type"), Poi.Type);
                double Lat = 0.0, Lon = 0.0;
                (*Obj)->TryGetNumberField(TEXT("lat"), Lat);
                (*Obj)->TryGetNumberField(TEXT("lon"), Lon);
                Poi.Location = FVector(static_cast<float>(Lon), 0.0f, -static_cast<float>(Lat));
                OutPOIs.Add(Poi);
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Loaded %d POIs"), OutPOIs.Num());
    return OutPOIs.Num() > 0;
}
