#include "AkronXodrImporter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Math/UnrealMathUtility.h"
#include "XmlFile.h"
#include "XmlNode.h"

const float EARTH_RADIUS_M = 6371000.0f;

/** Built-in fallback city: the shipped Akron beta pack. */
static const TCHAR* const RACEGPS_DEFAULT_CITY_ID = TEXT("akron-oh-beta-001");

/** Runtime override for the active city, e.g. "racegps.CityId cleveland_5.0km". */
static TAutoConsoleVariable<FString> CVarRaceGPSCityId(
    TEXT("racegps.CityId"),
    TEXT(""),
    TEXT("Active citypack city id (e.g. akron-oh-beta-001). Overrides DefaultGame.ini [RaceGPS.CitySelection] CityId."),
    ECVF_Default);

FString UAkronXodrImporter::GetActiveCityId()
{
    // 1. Command line: -racegps.city=<city_id>
    FString CityId;
    if (FParse::Value(FCommandLine::Get(), TEXT("racegps.city="), CityId) && !CityId.IsEmpty())
    {
        return CityId;
    }

    // 2. Console variable (runtime / [ConsoleVariables] ini override)
    CityId = CVarRaceGPSCityId.GetValueOnAnyThread();
    if (!CityId.IsEmpty())
    {
        return CityId;
    }

    // 3. DefaultGame.ini [RaceGPS.CitySelection] CityId
    if (GConfig && GConfig->GetString(TEXT("RaceGPS.CitySelection"), TEXT("CityId"), CityId, GGameIni) && !CityId.IsEmpty())
    {
        return CityId;
    }

    // 4. Built-in default keeps existing Akron behavior unchanged.
    return RACEGPS_DEFAULT_CITY_ID;
}

bool UAkronXodrImporter::LoadJsonObjectFile(const FString& ProjectRelativePath, TSharedPtr<FJsonObject>& OutRoot)
{
    const FString FullPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / ProjectRelativePath);
    FString Content;
    if (!FFileHelper::LoadFileToString(Content, *FullPath))
    {
        return false;
    }

    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
    return FJsonSerializer::Deserialize(Reader, OutRoot) && OutRoot.IsValid();
}

FString UAkronXodrImporter::GetManifestDir(const FString& ManifestPath)
{
    return FPaths::GetPath(ManifestPath);
}

bool UAkronXodrImporter::GetManifestFileRef(const TSharedPtr<FJsonObject>& Root, const FString& FieldName, FString& OutFileName)
{
    if (!Root.IsValid())
    {
        return false;
    }

    // Dialect A (legacy Akron): flat filename string at the top level.
    if (Root->TryGetStringField(FieldName, OutFileName) && !OutFileName.IsEmpty())
    {
        return true;
    }

    // Dialect B (universal compiler v2): filename strings under "files".
    const TSharedPtr<FJsonObject>* FilesObj = nullptr;
    if (Root->TryGetObjectField(TEXT("files"), FilesObj) && FilesObj && FilesObj->IsValid())
    {
        if ((*FilesObj)->TryGetStringField(FieldName, OutFileName) && !OutFileName.IsEmpty())
        {
            return true;
        }
    }
    return false;
}

bool UAkronXodrImporter::ResolveManifestDataFile(const FString& ManifestPath, const FString& FieldName, FString& OutPath)
{
    TSharedPtr<FJsonObject> Root;
    if (!LoadJsonObjectFile(ManifestPath, Root))
    {
        return false;
    }

    FString FileName;
    if (!GetManifestFileRef(Root, FieldName, FileName))
    {
        return false;
    }

    OutPath = GetManifestDir(ManifestPath) / FileName;
    return true;
}

static bool FindSingleFileBySuffix(const FString& ProjectRelativeDir, const FString& Suffix, FString& OutPath)
{
    const FString FullDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / ProjectRelativeDir);
    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files, *(FullDir / TEXT("*") + Suffix), true, false);
    if (Files.Num() == 0)
    {
        return false;
    }
    Files.Sort();
    OutPath = ProjectRelativeDir / Files[0];
    return true;
}

bool UAkronXodrImporter::ResolveCityLayout(FRaceGPSCityLayout& OutLayout)
{
    OutLayout = FRaceGPSCityLayout();
    OutLayout.CityId = GetActiveCityId();

    // Optional config overrides (project-relative paths).
    FString ConfigValue;
    const bool bHasPackDirOverride = GConfig &&
        GConfig->GetString(TEXT("RaceGPS.CitySelection"), TEXT("CitypackDir"), ConfigValue, GGameIni) &&
        !ConfigValue.IsEmpty();
    OutLayout.CitypackDir = bHasPackDirOverride ? ConfigValue : (FString(TEXT("../../citypacks")) / OutLayout.CityId);

    // Manifest: <pack>/*_semantic_manifest.json (filename does not always embed the city id).
    if (!FindSingleFileBySuffix(OutLayout.CitypackDir, TEXT("_semantic_manifest.json"), OutLayout.ManifestPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] No *_semantic_manifest.json found for city '%s' in %s"),
            *OutLayout.CityId, *OutLayout.CitypackDir);
        return false;
    }

    TSharedPtr<FJsonObject> Root;
    if (!LoadJsonObjectFile(OutLayout.ManifestPath, Root))
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] Failed to parse manifest: %s"), *OutLayout.ManifestPath);
        return false;
    }

    // display_name (Dialect A) or name (Dialect B).
    if (!Root->TryGetStringField(TEXT("display_name"), OutLayout.DisplayName))
    {
        Root->TryGetStringField(TEXT("name"), OutLayout.DisplayName);
    }

    const FString PackDir = GetManifestDir(OutLayout.ManifestPath);
    FString FileName;

    // OpenDRIVE: manifest ref (Dialect A) or any *.xodr in the pack.
    if (GetManifestFileRef(Root, TEXT("opendrive_file"), FileName))
    {
        OutLayout.XodrPath = PackDir / FileName;
    }
    else
    {
        FindSingleFileBySuffix(PackDir, TEXT(".xodr"), OutLayout.XodrPath);
    }

    // Routes: single array file (both dialects) or legacy per-route routes/ directory.
    if (GetManifestFileRef(Root, TEXT("routes"), FileName))
    {
        OutLayout.RoutesPath = PackDir / FileName;
    }
    else
    {
        const FString RoutesDir = PackDir / TEXT("routes");
        const FString FullRoutesDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / RoutesDir);
        TArray<FString> RouteFiles;
        IFileManager::Get().FindFiles(RouteFiles, *(FullRoutesDir / TEXT("*.json")), true, false);
        if (RouteFiles.Num() > 0)
        {
            OutLayout.RoutesPath = RoutesDir;
        }
    }

    // Spawn points / POIs: filename refs in either dialect; when the manifest embeds
    // the arrays directly the paths stay empty and the loaders read the manifest.
    if (GetManifestFileRef(Root, TEXT("spawn_points"), FileName))
    {
        OutLayout.SpawnPointsPath = PackDir / FileName;
    }
    if (GetManifestFileRef(Root, TEXT("pois"), FileName))
    {
        OutLayout.POIsPath = PackDir / FileName;
    }

    // Road graph: manifest ref or *_road_graph.json glob.
    if (GetManifestFileRef(Root, TEXT("road_graph"), FileName))
    {
        OutLayout.RoadGraphPath = PackDir / FileName;
    }
    else
    {
        FindSingleFileBySuffix(PackDir, TEXT("_road_graph.json"), OutLayout.RoadGraphPath);
    }

    // Buildings (optional; absent from the shipped Akron pack).
    if (GetManifestFileRef(Root, TEXT("buildings"), FileName))
    {
        const FString Candidate = PackDir / FileName;
        if (FPaths::FileExists(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / Candidate)))
        {
            OutLayout.BuildingsPath = Candidate;
        }
    }
    if (OutLayout.BuildingsPath.IsEmpty())
    {
        FindSingleFileBySuffix(PackDir, TEXT("_buildings.json"), OutLayout.BuildingsPath);
    }

    // Level spec: config override, else generated/*_LevelSpec.json matching city_id.
    const bool bHasSpecOverride = GConfig &&
        GConfig->GetString(TEXT("RaceGPS.CitySelection"), TEXT("LevelSpecFile"), ConfigValue, GGameIni) &&
        !ConfigValue.IsEmpty();
    if (bHasSpecOverride)
    {
        OutLayout.LevelSpecPath = ConfigValue;
    }
    else
    {
        const FString GeneratedDir = TEXT("../../generated");
        const FString FullGeneratedDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / GeneratedDir);
        TArray<FString> SpecFiles;
        IFileManager::Get().FindFiles(SpecFiles, *(FullGeneratedDir / TEXT("*_LevelSpec.json")), true, false);
        SpecFiles.Sort();
        for (const FString& SpecFile : SpecFiles)
        {
            const FString Candidate = GeneratedDir / SpecFile;
            TSharedPtr<FJsonObject> SpecRoot;
            if (LoadJsonObjectFile(Candidate, SpecRoot))
            {
                FString SpecCityId;
                if (SpecRoot->TryGetStringField(TEXT("city_id"), SpecCityId) && SpecCityId == OutLayout.CityId)
                {
                    OutLayout.LevelSpecPath = Candidate;
                    SpecRoot->TryGetStringField(TEXT("level_name"), OutLayout.LevelName);
                    break;
                }
            }
        }
    }
    if (!OutLayout.LevelSpecPath.IsEmpty() && OutLayout.LevelName.IsEmpty())
    {
        TSharedPtr<FJsonObject> SpecRoot;
        if (LoadJsonObjectFile(OutLayout.LevelSpecPath, SpecRoot))
        {
            SpecRoot->TryGetStringField(TEXT("level_name"), OutLayout.LevelName);
        }
    }

    // UE package/asset names cannot contain '.', spaces, etc. Spec level_names
    // are derived from city_id (e.g. "cleveland_5.0km" -> "Cleveland5.0KmWorld"),
    // which can be illegal. Normalize here — the single choke point every
    // LevelName consumer (MainMenuWidget OpenLevel) goes through — so the
    // runtime always opens the legal asset name ("Cleveland5_0KmWorld").
    // Already-legal names (e.g. "AkronWorld") pass through unchanged.
    if (!OutLayout.LevelName.IsEmpty())
    {
        FString Sanitized;
        Sanitized.Reserve(OutLayout.LevelName.Len());
        for (const TCHAR Ch : OutLayout.LevelName)
        {
            Sanitized.AppendChar((FChar::IsAlnum(Ch) || Ch == TEXT('_')) ? Ch : TEXT('_'));
        }
        if (Sanitized != OutLayout.LevelName)
        {
            UE_LOG(LogTemp, Log, TEXT("[raceGPS] Level name '%s' normalized to legal UE package name '%s'"),
                *OutLayout.LevelName, *Sanitized);
            OutLayout.LevelName = MoveTemp(Sanitized);
        }
    }

    return true;
}

float UAkronXodrImporter::MetersPerDegreeLon(float Lat)
{
    float Rad = FMath::DegreesToRadians(Lat);
    return 111320.0f * FMath::Cos(Rad);
}

float UAkronXodrImporter::MetersPerDegreeLat()
{
    return 110540.0f;
}

// Sprint-2 scale decision: real-world scale everywhere — 1 uu = 1 cm.
// Compiler/spec data is meters; every meter value crossing into the world
// multiplies by this. Keep in sync with the bake choke point
// (tools/ue5-import-level-spec.py::_spec_to_ue).
constexpr float kMetersToUU = UAkronXodrImporter::MetersToUU;

FVector UAkronXodrImporter::GeoToWorld(float Lat, float Lon, float OriginLat, float OriginLon)
{
    // Standard UE Z-up: X = east, Y = north, Z = up. The T10 bake remaps
    // compiler data (x, y, z) -> UE (x, -z, y) into exactly this convention
    // (tools/ue5-import-level-spec.py::_spec_to_ue), so runtime code must use
    // the same. Callers add height via +Z afterwards.
    const float MetersPerLon = MetersPerDegreeLon(OriginLat);
    const float MetersPerLat = MetersPerDegreeLat();
    const float X = (Lon - OriginLon) * MetersPerLon * kMetersToUU;
    const float Y = (Lat - OriginLat) * MetersPerLat * kMetersToUU;
    return FVector(X, Y, 0.0f);
}

FVector UAkronXodrImporter::XodrToWorld(float X, float Y, float OriginLat, float OriginLon)
{
    // OpenDRIVE: X=east, Y=north  ->  UE Z-up: X=east, Y=north, Z=up.
    // Same convention as GeoToWorld; origin already baked into XODR local
    // coords, so no remap is needed beyond passing the axes through.
    // XODR local coords are meters -> cm.
    (void)OriginLat;
    (void)OriginLon;
    return FVector(X * kMetersToUU, Y * kMetersToUU, 0.0f);
}

bool UAkronXodrImporter::ImportXodr(const FString& XodrPath, TArray<FAkronRoadSegment>& OutRoads)
{
    FString FullPath = FPaths::ProjectDir() / XodrPath;
    if (!FPaths::FileExists(FullPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] XODR not found: %s. Falling back to road_graph.json"), *FullPath);
        // Road graph filename is city-specific; find any *_road_graph.json next to the XODR.
        FString RoadGraphPath;
        if (!FindSingleFileBySuffix(FPaths::GetPath(XodrPath), TEXT("_road_graph.json"), RoadGraphPath))
        {
            UE_LOG(LogTemp, Error, TEXT("[raceGPS] No *_road_graph.json fallback found next to %s"), *XodrPath);
            return false;
        }
        return LoadRoadGraphJson(RoadGraphPath, OutRoads);
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
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[raceGPS] XODR %s has no header/geoReference; defaulting origin to Akron (%.4f, %.4f)"),
                *FullPath, OriginLat, OriginLon);
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

    // Compiler (Dialect B) road graphs carry a top-level origin; use it when present.
    const TSharedPtr<FJsonObject>* OriginObj = nullptr;
    if (Root->TryGetObjectField(TEXT("origin"), OriginObj) && OriginObj && OriginObj->IsValid())
    {
        double Lat = 0.0, Lon = 0.0;
        if ((*OriginObj)->TryGetNumberField(TEXT("lat"), Lat) && (*OriginObj)->TryGetNumberField(TEXT("lon"), Lon))
        {
            OriginLat = static_cast<float>(Lat);
            OriginLon = static_cast<float>(Lon);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] Road graph %s has no top-level origin; defaulting to Akron (%.4f, %.4f)"),
            *FullPath, OriginLat, OriginLon);
    }

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
            // Both the compiler and the shipped packs write "one_way"; "oneway" kept for back-compat.
            if (!(*RoadObj)->TryGetBoolField(TEXT("one_way"), bOneWay))
            {
                (*RoadObj)->TryGetBoolField(TEXT("oneway"), bOneWay);
            }
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

bool UAkronXodrImporter::LoadManifest(const FString& ManifestPath, float& OutWorldOriginLat, float& OutWorldOriginLon)
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

    // Origin resolution (both manifest dialects carry "origin"; legacy readers used
    // bounds.lat_min/lon_min which no dialect ships — keep as a back-compat fallback).
    double OriginLat = 0.0, OriginLon = 0.0;
    bool bFoundOrigin = false;

    const TSharedPtr<FJsonObject>* OriginObj = nullptr;
    if (Root->TryGetObjectField(TEXT("origin"), OriginObj) && OriginObj && OriginObj->IsValid())
    {
        bFoundOrigin = (*OriginObj)->TryGetNumberField(TEXT("lat"), OriginLat) &&
                       (*OriginObj)->TryGetNumberField(TEXT("lon"), OriginLon);
    }

    const TSharedPtr<FJsonObject>* BoundsObj = nullptr;
    if (!bFoundOrigin && Root->TryGetObjectField(TEXT("bounds"), BoundsObj) && BoundsObj && BoundsObj->IsValid())
    {
        // Legacy key spellings first, then the contractual west/south corner.
        if ((*BoundsObj)->TryGetNumberField(TEXT("lat_min"), OriginLat) &&
            (*BoundsObj)->TryGetNumberField(TEXT("lon_min"), OriginLon))
        {
            bFoundOrigin = true;
        }
        else if ((*BoundsObj)->TryGetNumberField(TEXT("south"), OriginLat) &&
                 (*BoundsObj)->TryGetNumberField(TEXT("west"), OriginLon))
        {
            bFoundOrigin = true;
        }
    }

    // Only overwrite the caller's origin when we actually found one, so the
    // caller-provided defaults stay in effect otherwise.
    if (bFoundOrigin)
    {
        OutWorldOriginLat = static_cast<float>(OriginLat);
        OutWorldOriginLon = static_cast<float>(OriginLon);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] Manifest %s has no origin or bounds corner; keeping default origin (%.4f, %.4f)"),
            *FullPath, OutWorldOriginLat, OutWorldOriginLon);
    }

    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Manifest loaded. Origin: (lat %f, lon %f)"), OutWorldOriginLat, OutWorldOriginLon);
    return true;
}

void UAkronXodrImporter::ParseRouteObject(const TSharedPtr<FJsonObject>& RouteObj, const FString& FallbackRouteId, FAkronRouteSpline& OutRoute)
{
    // Route id: explicit field, else derived from the source filename.
    if (!RouteObj->TryGetStringField(TEXT("route_id"), OutRoute.RouteId))
    {
        OutRoute.RouteId = FallbackRouteId;
    }

    // Shipped packs use "distance_meters"; the legacy per-route files used "distance_m".
    double Distance = 0.0;
    if (RouteObj->TryGetNumberField(TEXT("distance_meters"), Distance) ||
        RouteObj->TryGetNumberField(TEXT("distance_m"), Distance))
    {
        OutRoute.TotalDistanceMeters = static_cast<float>(Distance);
    }

    const TArray<TSharedPtr<FJsonValue>>* Points;
    if (RouteObj->TryGetArrayField(TEXT("points"), Points))
    {
        for (const TSharedPtr<FJsonValue>& Val : *Points)
        {
            const TSharedPtr<FJsonObject>* Pt;
            if (Val->TryGetObject(Pt))
            {
                double Lat = 0.0, Lon = 0.0;
                (*Pt)->TryGetNumberField(TEXT("lat"), Lat);
                (*Pt)->TryGetNumberField(TEXT("lon"), Lon);
                OutRoute.Waypoints.Add(FVector(static_cast<float>(Lon), 0.0f, -static_cast<float>(Lat)));
            }
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* Cps;
    if (RouteObj->TryGetArrayField(TEXT("checkpoints"), Cps))
    {
        for (const TSharedPtr<FJsonValue>& Val : *Cps)
        {
            const TSharedPtr<FJsonObject>* Cp;
            if (Val->TryGetObject(Cp))
            {
                double Lat = 0.0, Lon = 0.0;
                (*Cp)->TryGetNumberField(TEXT("lat"), Lat);
                (*Cp)->TryGetNumberField(TEXT("lon"), Lon);
                OutRoute.CheckpointLocations.Add(FVector(static_cast<float>(Lon), 0.0f, -static_cast<float>(Lat)));
            }
        }
    }
}

bool UAkronXodrImporter::LoadRouteSplines(const FString& RoutePathOrDir, TArray<FAkronRouteSpline>& OutRoutes)
{
    FString FullPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / RoutePathOrDir);

    // Single-file form: one JSON file holding either a bare array of route objects
    // (the shipped pack layout) or a single route object.
    if (FPaths::FileExists(FullPath))
    {
        FString Content;
        if (!FFileHelper::LoadFileToString(Content, *FullPath))
        {
            UE_LOG(LogTemp, Error, TEXT("[raceGPS] Failed to load routes file: %s"), *FullPath);
            return false;
        }

        const FString BaseName = FPaths::GetBaseFilename(FullPath);
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);

        // Bare top-level array (contractual shape, CITYPACK_CONTRACT.md §2.3).
        TArray<TSharedPtr<FJsonValue>> RoutesArray;
        if (FJsonSerializer::Deserialize(Reader, RoutesArray))
        {
            for (const TSharedPtr<FJsonValue>& Val : RoutesArray)
            {
                const TSharedPtr<FJsonObject>* RouteObj;
                if (!Val->TryGetObject(RouteObj)) continue;
                FAkronRouteSpline Route;
                ParseRouteObject(*RouteObj, BaseName, Route);
                OutRoutes.Add(Route);
            }
        }
        else
        {
            // Single route object (legacy per-route file shape).
            TSharedPtr<FJsonObject> Root;
            TSharedRef<TJsonReader<>> ObjReader = TJsonReaderFactory<>::Create(Content);
            if (FJsonSerializer::Deserialize(ObjReader, Root) && Root.IsValid())
            {
                FAkronRouteSpline Route;
                ParseRouteObject(Root, BaseName, Route);
                OutRoutes.Add(Route);
            }
        }

        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Loaded %d route splines from %s"), OutRoutes.Num(), *FullPath);
        return OutRoutes.Num() > 0;
    }

    // Directory form (legacy): one JSON object per route file.
    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files, *(FullPath / TEXT("*.json")), true, false);

    for (const FString& File : Files)
    {
        FString Content;
        if (!FFileHelper::LoadFileToString(Content, *(FullPath / File))) continue;

        TSharedPtr<FJsonObject> Root;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
        if (!FJsonSerializer::Deserialize(Reader, Root)) continue;

        FAkronRouteSpline Route;
        ParseRouteObject(Root, FPaths::GetBaseFilename(File), Route);
        OutRoutes.Add(Route);
    }

    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Loaded %d route splines from %s"), OutRoutes.Num(), *FullPath);
    return OutRoutes.Num() > 0;
}

void UAkronXodrImporter::ParseSpawnArray(const TArray<TSharedPtr<FJsonValue>>& Spawns, TArray<FAkronSpawnPoint>& OutSpawns)
{
    for (const TSharedPtr<FJsonValue>& Val : Spawns)
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

bool UAkronXodrImporter::LoadSpawnPoints(const FString& ManifestPath, TArray<FAkronSpawnPoint>& OutSpawns)
{
    TSharedPtr<FJsonObject> Root;
    if (!LoadJsonObjectFile(ManifestPath, Root)) return false;

    // Embedded array (legacy expectation) still supported.
    const TArray<TSharedPtr<FJsonValue>>* Spawns;
    if (Root->TryGetArrayField(TEXT("spawn_points"), Spawns))
    {
        ParseSpawnArray(*Spawns, OutSpawns);
    }
    else
    {
        // Both shipped dialects store a filename reference instead; resolve and load it.
        FString SpawnFile;
        if (GetManifestFileRef(Root, TEXT("spawn_points"), SpawnFile))
        {
            const FString SpawnPath = GetManifestDir(ManifestPath) / SpawnFile;
            const FString FullPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / SpawnPath);
            FString Content;
            if (!FFileHelper::LoadFileToString(Content, *FullPath))
            {
                UE_LOG(LogTemp, Warning, TEXT("[raceGPS] Spawn points file not found: %s"), *FullPath);
                return false;
            }

            // Bare top-level array (contractual shape, CITYPACK_CONTRACT.md §2.4).
            TArray<TSharedPtr<FJsonValue>> SpawnArray;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
            if (FJsonSerializer::Deserialize(Reader, SpawnArray))
            {
                ParseSpawnArray(SpawnArray, OutSpawns);
            }
            else
            {
                // Tolerate an object wrapper, e.g. {"spawn_points": [...]}.
                TSharedPtr<FJsonObject> FileRoot;
                TSharedRef<TJsonReader<>> ObjReader = TJsonReaderFactory<>::Create(Content);
                if (FJsonSerializer::Deserialize(ObjReader, FileRoot) && FileRoot.IsValid())
                {
                    const TArray<TSharedPtr<FJsonValue>>* Wrapped;
                    if (FileRoot->TryGetArrayField(TEXT("spawn_points"), Wrapped))
                    {
                        ParseSpawnArray(*Wrapped, OutSpawns);
                    }
                }
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Loaded %d spawn points"), OutSpawns.Num());
    return OutSpawns.Num() > 0;
}

void UAkronXodrImporter::ParsePOIArray(const TArray<TSharedPtr<FJsonValue>>& Pois, TArray<FAkronPOI>& OutPOIs)
{
    for (const TSharedPtr<FJsonValue>& Val : Pois)
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

bool UAkronXodrImporter::LoadPOIs(const FString& ManifestPath, TArray<FAkronPOI>& OutPOIs)
{
    TSharedPtr<FJsonObject> Root;
    if (!LoadJsonObjectFile(ManifestPath, Root)) return false;

    // Embedded array (legacy expectation) still supported.
    const TArray<TSharedPtr<FJsonValue>>* Pois;
    if (Root->TryGetArrayField(TEXT("pois"), Pois))
    {
        ParsePOIArray(*Pois, OutPOIs);
    }
    else
    {
        // Both shipped dialects store a filename reference instead; resolve and load it.
        FString PoiFile;
        if (GetManifestFileRef(Root, TEXT("pois"), PoiFile))
        {
            const FString PoiPath = GetManifestDir(ManifestPath) / PoiFile;
            const FString FullPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / PoiPath);
            FString Content;
            if (!FFileHelper::LoadFileToString(Content, *FullPath))
            {
                UE_LOG(LogTemp, Warning, TEXT("[raceGPS] POI file not found: %s"), *FullPath);
                return false;
            }

            // Bare top-level array (contractual shape, CITYPACK_CONTRACT.md §2.5).
            TArray<TSharedPtr<FJsonValue>> PoiArray;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
            if (FJsonSerializer::Deserialize(Reader, PoiArray))
            {
                ParsePOIArray(PoiArray, OutPOIs);
            }
            else
            {
                // Tolerate an object wrapper, e.g. {"pois": [...]}.
                TSharedPtr<FJsonObject> FileRoot;
                TSharedRef<TJsonReader<>> ObjReader = TJsonReaderFactory<>::Create(Content);
                if (FJsonSerializer::Deserialize(ObjReader, FileRoot) && FileRoot.IsValid())
                {
                    const TArray<TSharedPtr<FJsonValue>>* Wrapped;
                    if (FileRoot->TryGetArrayField(TEXT("pois"), Wrapped))
                    {
                        ParsePOIArray(*Wrapped, OutPOIs);
                    }
                }
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Loaded %d POIs"), OutPOIs.Num());
    return OutPOIs.Num() > 0;
}
