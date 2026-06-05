#include "AkronXodrImporter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Math/UnrealMathUtility.h"
#include "XmlParser/Public/XmlFile.h"
#include "XmlParser/Public/XmlNode.h"

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

FVector UAkronXodrImporter::XodrToWorld(float X, float Y, float OriginLat, float OriginLon)
{
    // OpenDRIVE: X=east, Y=north
    // Unreal:    X=east, Z=-north (south is positive Z)
    // Origin already baked into XODR local coords, so just remap axes
    (void)OriginLat;
    (void)OriginLon;
    return FVector(X, 0.0f, -Y);
}

bool UAkronXodrImporter::ImportXodr(const FString& XodrPath, TArray<FAkronRoadSegment>& OutRoads)
{
    FString FullPath = FPaths::ProjectDir() / XodrPath;
    if (!FPaths::FileExists(FullPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] XODR not found: %s. Falling back to road_graph.json"), *FullPath);
        FString JsonPath = FPaths::GetPath(XodrPath) / TEXT("akron_road_graph.json");
        return LoadRoadGraphJson(JsonPath, OutRoads);
    }

    FXmlFile XmlFile;
    if (!XmlFile.LoadFile(FullPath))
    {
        UE_LOG(LogTemp, Error, TEXT("[raceGPS] Failed to parse XODR XML: %s"), *FullPath);
        return false;
    }

    FXmlNode* RootNode = XmlFile.GetRootNode();
    if (!RootNode)
    {
        UE_LOG(LogTemp, Error, TEXT("[raceGPS] XODR has no root node"));
        return false;
    }

    // Parse geoReference from header to extract origin
    float OriginLat = 41.08f;
    float OriginLon = -81.52f;
    const FXmlNode* HeaderNode = RootNode->FindChildNode(TEXT("header"));
    if (HeaderNode)
    {
        const FXmlNode* GeoRefNode = HeaderNode->FindChildNode(TEXT("geoReference"));
        if (GeoRefNode)
        {
            FString GeoRef = GeoRefNode->GetContent();
            // Extract +lat_0 and +lon_0 from PROJ string
            int32 LatIdx = GeoRef.Find(TEXT("+lat_0="));
            int32 LonIdx = GeoRef.Find(TEXT("+lon_0="));
            if (LatIdx != INDEX_NONE)
            {
                FString LatStr = GeoRef.Mid(LatIdx + 7);
                int32 SpaceIdx = LatStr.Find(TEXT(" "));
                if (SpaceIdx != INDEX_NONE) LatStr = LatStr.Left(SpaceIdx);
                OriginLat = FCString::Atof(*LatStr);
            }
            if (LonIdx != INDEX_NONE)
            {
                FString LonStr = GeoRef.Mid(LonIdx + 7);
                int32 SpaceIdx = LonStr.Find(TEXT(" "));
                if (SpaceIdx != INDEX_NONE) LonStr = LonStr.Left(SpaceIdx);
                OriginLon = FCString::Atof(*LonStr);
            }
        }
    }

    // Parse roads
    const TArray<FXmlNode*>& ChildNodes = RootNode->GetChildrenNodes();
    for (const FXmlNode* Child : ChildNodes)
    {
        if (Child->GetTag() != TEXT("road"))
            continue;

        FAkronRoadSegment Segment;
        Segment.RoadId = Child->GetAttribute(TEXT("id"));
        Segment.WidthMeters = 7.0f;
        Segment.NumLanes = 2;

        // Parse lanes for width
        const FXmlNode* LanesNode = Child->FindChildNode(TEXT("lanes"));
        if (LanesNode)
        {
            const FXmlNode* LaneSectionNode = LanesNode->FindChildNode(TEXT("laneSection"));
            if (LaneSectionNode)
            {
                const FXmlNode* RightNode = LaneSectionNode->FindChildNode(TEXT("right"));
                if (RightNode)
                {
                    const FXmlNode* LaneNode = RightNode->FindChildNode(TEXT("lane"));
                    if (LaneNode)
                    {
                        const FXmlNode* WidthNode = LaneNode->FindChildNode(TEXT("width"));
                        if (WidthNode)
                        {
                            FString WidthStr = WidthNode->GetAttribute(TEXT("a"));
                            Segment.WidthMeters = FCString::Atof(*WidthStr) * 2.0f; // Both sides
                        }
                    }
                }
                const FXmlNode* LeftNode = LaneSectionNode->FindChildNode(TEXT("left"));
                Segment.NumLanes = LeftNode ? 2 : 1;
            }
        }

        // Parse planView geometry
        const FXmlNode* PlanViewNode = Child->FindChildNode(TEXT("planView"));
        if (PlanViewNode)
        {
            const TArray<FXmlNode*>& GeomNodes = PlanViewNode->GetChildrenNodes();
            for (const FXmlNode* Geom : GeomNodes)
            {
                if (Geom->GetTag() != TEXT("geometry"))
                    continue;

                FString XStr = Geom->GetAttribute(TEXT("x"));
                FString YStr = Geom->GetAttribute(TEXT("y"));
                float X = FCString::Atof(*XStr);
                float Y = FCString::Atof(*YStr);
                Segment.WorldPoints.Add(XodrToWorld(X, Y, OriginLat, OriginLon));
            }
        }

        if (Segment.WorldPoints.Num() >= 2)
        {
            OutRoads.Add(Segment);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Imported %d roads from XODR"), OutRoads.Num());
    return OutRoads.Num() > 0;
}

bool UAkronXodrImporter::LoadRoadGraphJson(const FString& JsonPath, TArray<FAkronRoadSegment>& OutRoads)
{
    FString FullPath = FPaths::ProjectDir() / JsonPath;
    FString Content;
    if (!FFileHelper::LoadFileToString(Content, *FullPath))
    {
        UE_LOG(LogTemp, Error, TEXT("[raceGPS] Failed to load road graph: %s"), *FullPath);
        return false;
    }

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
    if (!FJsonSerializer::Deserialize(Reader, Root))
    {
        UE_LOG(LogTemp, Error, TEXT("[raceGPS] Failed to parse road graph JSON"));
        return false;
    }

    float OriginLat = 41.08f;
    float OriginLon = -81.52f;

    const TArray<TSharedPtr<FJsonValue>>* RoadsArr;
    if (Root->TryGetArrayField(TEXT("roads"), RoadsArr))
    {
        for (const TSharedPtr<FJsonValue>& Val : *RoadsArr)
        {
            const TSharedPtr<FJsonObject>* RoadObj;
            if (!Val->TryGetObject(RoadObj)) continue;

            FAkronRoadSegment Segment;
            (*RoadObj)->TryGetStringField(TEXT("id"), Segment.RoadId);
            double Width = 7.0;
            (*RoadObj)->TryGetNumberField(TEXT("width"), Width);
            Segment.WidthMeters = static_cast<float>(Width);

            bool bOneWay = false;
            (*RoadObj)->TryGetBoolField(TEXT("oneway"), bOneWay);
            Segment.NumLanes = bOneWay ? 1 : 2;

            const TArray<TSharedPtr<FJsonValue>>* PointsArr;
            if ((*RoadObj)->TryGetArrayField(TEXT("points"), PointsArr))
            {
                for (const TSharedPtr<FJsonValue>& PtVal : *PointsArr)
                {
                    const TSharedPtr<FJsonObject>* PtObj;
                    if (!PtVal->TryGetObject(PtObj)) continue;

                    double Lat = 0.0, Lon = 0.0;
                    (*PtObj)->TryGetNumberField(TEXT("lat"), Lat);
                    (*PtObj)->TryGetNumberField(TEXT("lon"), Lon);
                    Segment.WorldPoints.Add(GeoToWorld(static_cast<float>(Lat), static_cast<float>(Lon), OriginLat, OriginLon));
                }
            }

            if (Segment.WorldPoints.Num() >= 2)
            {
                OutRoads.Add(Segment);
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Loaded %d roads from road graph JSON"), OutRoads.Num());
    return OutRoads.Num() > 0;
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
