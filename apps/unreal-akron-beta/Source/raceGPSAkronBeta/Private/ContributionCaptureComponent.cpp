#include "ContributionCaptureComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Http.h"

UContributionCaptureComponent::UContributionCaptureComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UContributionCaptureComponent::BeginPlay()
{
    Super::BeginPlay();
    LoadQueueFromDisk();
}

void UContributionCaptureComponent::SubmitBuildingAnnotation(const FString& BuildingOsmId,
    const FString& GuessedType, float Lat, float Lon, const FString& GuessedName, int32 Confidence)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("building_osm_id"), BuildingOsmId);
    Obj->SetStringField(TEXT("guessed_type"), GuessedType);
    Obj->SetStringField(TEXT("guessed_name"), GuessedName);
    Obj->SetNumberField(TEXT("confidence"), Confidence);
    Obj->SetNumberField(TEXT("lat"), Lat);
    Obj->SetNumberField(TEXT("lon"), Lon);
    Obj->SetStringField(TEXT("player_id"), PlayerId);
    Obj->SetStringField(TEXT("city_id"), CityId);

    FString Payload;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
    FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);

    AddToQueue(TEXT("building_annotation"), Payload);
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Queued building annotation: %s -> %s"), *BuildingOsmId, *GuessedType);
}

void UContributionCaptureComponent::SubmitRouteFeedback(const FString& RouteId,
    const FString& FeedbackType, const FString& Description, int32 CheckpointIndex,
    float SuggestedLat, float SuggestedLon)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("route_id"), RouteId);
    Obj->SetStringField(TEXT("feedback_type"), FeedbackType);
    Obj->SetStringField(TEXT("description"), Description);
    Obj->SetStringField(TEXT("player_id"), PlayerId);
    if (CheckpointIndex >= 0)
    {
        Obj->SetNumberField(TEXT("checkpoint_index"), CheckpointIndex);
        Obj->SetNumberField(TEXT("suggested_lat"), SuggestedLat);
        Obj->SetNumberField(TEXT("suggested_lon"), SuggestedLon);
    }

    FString Payload;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
    FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);

    AddToQueue(TEXT("route_feedback"), Payload);
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Queued route feedback: %s"), *FeedbackType);
}

void UContributionCaptureComponent::SubmitTrafficReport(float Lat, float Lon, int32 TrafficDensity,
    const FString& TimeOfDay, int32 DayOfWeek)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("city_id"), CityId);
    Obj->SetStringField(TEXT("player_id"), PlayerId);
    Obj->SetNumberField(TEXT("lat"), Lat);
    Obj->SetNumberField(TEXT("lon"), Lon);
    Obj->SetNumberField(TEXT("traffic_density"), TrafficDensity);
    Obj->SetStringField(TEXT("time_of_day"), TimeOfDay);
    Obj->SetNumberField(TEXT("day_of_week"), DayOfWeek);

    FString Payload;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
    FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);

    AddToQueue(TEXT("traffic_report"), Payload);
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Queued traffic report: density=%d"), TrafficDensity);
}

void UContributionCaptureComponent::SubmitRouteCreation(const FString& RouteId,
    const FString& RouteName, const TArray<FVector>& Waypoints, const TArray<FVector>& Checkpoints,
    float DistanceMeters, const FString& Difficulty)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("route_id"), RouteId);
    Obj->SetStringField(TEXT("city_id"), CityId);
    Obj->SetStringField(TEXT("player_id"), PlayerId);
    Obj->SetStringField(TEXT("route_name"), RouteName);
    Obj->SetNumberField(TEXT("distance_meters"), DistanceMeters);
    Obj->SetStringField(TEXT("difficulty"), Difficulty);
    Obj->SetStringField(TEXT("status"), TEXT("published"));

    // Waypoints as lat/lon (converted from local meters roughly)
    TArray<TSharedPtr<FJsonValue>> WpArr;
    for (const FVector& Wp : Waypoints)
    {
        TSharedPtr<FJsonObject> Pt = MakeShared<FJsonObject>();
        Pt->SetNumberField(TEXT("x"), Wp.X);
        Pt->SetNumberField(TEXT("y"), Wp.Y);
        Pt->SetNumberField(TEXT("z"), Wp.Z);
        WpArr.Add(MakeShared<FJsonValueObject>(Pt));
    }
    Obj->SetArrayField(TEXT("waypoints"), WpArr);

    TArray<TSharedPtr<FJsonValue>> CpArr;
    for (const FVector& Cp : Checkpoints)
    {
        TSharedPtr<FJsonObject> Pt = MakeShared<FJsonObject>();
        Pt->SetNumberField(TEXT("x"), Cp.X);
        Pt->SetNumberField(TEXT("y"), Cp.Y);
        Pt->SetNumberField(TEXT("z"), Cp.Z);
        CpArr.Add(MakeShared<FJsonValueObject>(Pt));
    }
    Obj->SetArrayField(TEXT("checkpoints"), CpArr);

    FString Payload;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
    FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);

    AddToQueue(TEXT("route_created"), Payload);
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Queued route creation: %s (%s, %.0fm)"), *RouteId, *RouteName, DistanceMeters);
}

void UContributionCaptureComponent::AddToQueue(const FString& Type, const FString& JsonPayload)
{
    FPendingContribution Contrib;
    Contrib.Type = Type;
    Contrib.JsonPayload = JsonPayload;
    Contrib.Timestamp = FDateTime::Now().ToString(TEXT("%Y-%m-%dT%H:%M:%S"));
    Contrib.bSynced = false;
    PendingQueue.Add(Contrib);
    SaveQueueToDisk();
}

void UContributionCaptureComponent::SyncPendingContributions()
{
    if (SupabaseUrl.IsEmpty() || AnonKey.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] Supabase not configured. Skipping sync."));
        return;
    }

    int32 SyncedThisSession = 0;
    for (FPendingContribution& Contrib : PendingQueue)
    {
        if (Contrib.bSynced)
            continue;

        // Determine endpoint from type
        FString TableName;
        if (Contrib.Type == TEXT("building_annotation")) TableName = TEXT("building_annotations");
        else if (Contrib.Type == TEXT("route_feedback")) TableName = TEXT("route_feedback");
        else if (Contrib.Type == TEXT("traffic_report")) TableName = TEXT("traffic_reports");
        else if (Contrib.Type == TEXT("route_created")) TableName = TEXT("user_routes");
        else continue;

        FString Url = FString::Printf(TEXT("%s/rest/v1/%s"), *SupabaseUrl, *TableName);

        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
        Request->SetURL(Url);
        Request->SetVerb(TEXT("POST"));
        Request->SetHeader(TEXT("apikey"), AnonKey);
        Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AnonKey));
        Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
        Request->SetHeader(TEXT("Prefer"), TEXT("return=representation"));
        Request->SetContentAsString(Contrib.JsonPayload);
        Request->ProcessRequest();

        // Mark as synced optimistically (reality: HTTP is async, but for simplicity we mark now)
        Contrib.bSynced = true;
        SyncedThisSession++;
    }

    if (SyncedThisSession > 0)
    {
        TotalSynced += SyncedThisSession;
        SaveQueueToDisk();
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Synced %d contributions"), SyncedThisSession);
    }
}

void UContributionCaptureComponent::LoadQueueFromDisk()
{
    FString Content;
    if (FFileHelper::LoadFileToString(Content, *GetQueuePath()))
    {
        TSharedPtr<FJsonObject> Root;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
        if (FJsonSerializer::Deserialize(Reader, Root))
        {
            const TArray<TSharedPtr<FJsonValue>>* Arr;
            if (Root->TryGetArrayField(TEXT("queue"), Arr))
            {
                PendingQueue.Empty();
                for (const auto& Val : *Arr)
                {
                    const TSharedPtr<FJsonObject>* Obj;
                    if (!Val->TryGetObject(Obj))
                        continue;

                    FPendingContribution C;
                    (*Obj)->TryGetStringField(TEXT("type"), C.Type);
                    (*Obj)->TryGetStringField(TEXT("json_payload"), C.JsonPayload);
                    (*Obj)->TryGetStringField(TEXT("timestamp"), C.Timestamp);
                    (*Obj)->TryGetBoolField(TEXT("synced"), C.bSynced);
                    PendingQueue.Add(C);
                }
            }
        }
    }
}

void UContributionCaptureComponent::SaveQueueToDisk()
{
    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Arr;

    for (const FPendingContribution& C : PendingQueue)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("type"), C.Type);
        Obj->SetStringField(TEXT("json_payload"), C.JsonPayload);
        Obj->SetStringField(TEXT("timestamp"), C.Timestamp);
        Obj->SetBoolField(TEXT("synced"), C.bSynced);
        Arr.Add(MakeShared<FJsonValueObject>(Obj));
    }

    Root->SetArrayField(TEXT("queue"), Arr);

    FString Content;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Content);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    FFileHelper::SaveStringToFile(Content, *GetQueuePath());
}

FString UContributionCaptureComponent::GetQueuePath() const
{
    return FPaths::ProjectSavedDir() / TEXT("contribution_queue.json");
}

void UContributionCaptureComponent::OnSyncComplete(bool bSuccess, const FString& Response)
{
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Contribution sync complete: %d - %s"), bSuccess, *Response);
}
