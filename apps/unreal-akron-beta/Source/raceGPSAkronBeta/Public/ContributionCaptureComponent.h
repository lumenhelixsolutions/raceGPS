#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ContributionCaptureComponent.generated.h"

USTRUCT(BlueprintType)
struct FPendingContribution
{
    GENERATED_BODY()

    UPROPERTY()
    FString Type; // "building_annotation", "route_feedback", "traffic_report"

    UPROPERTY()
    FString JsonPayload;

    UPROPERTY()
    FString Timestamp;

    UPROPERTY()
    bool bSynced = false;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RACEGPSAKRONBETA_API UContributionCaptureComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UContributionCaptureComponent();

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "raceGPS|Contribution")
    FString SupabaseUrl;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "raceGPS|Contribution")
    FString AnonKey;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "raceGPS|Contribution")
    FString PlayerId;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "raceGPS|Contribution")
    FString CityId = TEXT("akron-oh-beta-001");

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Contribution")
    TArray<FPendingContribution> PendingQueue;

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Contribution")
    int32 TotalSynced = 0;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Contribution")
    void SubmitBuildingAnnotation(const FString& BuildingOsmId, const FString& GuessedType,
                                  float Lat, float Lon, const FString& GuessedName = TEXT(""), int32 Confidence = 3);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Contribution")
    void SubmitRouteFeedback(const FString& RouteId, const FString& FeedbackType,
                             const FString& Description = TEXT(""),
                             int32 CheckpointIndex = -1, float SuggestedLat = 0.0f, float SuggestedLon = 0.0f);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Contribution")
    void SubmitTrafficReport(float Lat, float Lon, int32 TrafficDensity,
                             const FString& TimeOfDay = TEXT(""), int32 DayOfWeek = 0);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Contribution")
    void SubmitRouteCreation(const FString& RouteId, const FString& RouteName,
                             const TArray<FVector>& Waypoints, const TArray<FVector>& Checkpoints,
                             float DistanceMeters, const FString& Difficulty = TEXT("medium"));

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Contribution")
    void SyncPendingContributions();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Contribution")
    void LoadQueueFromDisk();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Contribution")
    void SaveQueueToDisk();

    UFUNCTION(BlueprintPure, Category = "raceGPS|Contribution")
    int32 GetPendingCount() const { return PendingQueue.Num(); }

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void AddToQueue(const FString& Type, const FString& JsonPayload);

    UFUNCTION()
    void OnSyncComplete(bool bSuccess, const FString& Response);

    FString GetQueuePath() const;
};
