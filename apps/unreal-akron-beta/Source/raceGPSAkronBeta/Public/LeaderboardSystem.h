#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "LeaderboardEntry.h"
#include "LeaderboardSystem.generated.h"

UCLASS()
class RACEGPSAKRONBETA_API ULeaderboardSystem : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "raceGPS|Leaderboard")
    void AddEntry(const FString& RouteId, const FLeaderboardEntry& Entry);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Leaderboard")
    TArray<FLeaderboardEntry> GetTopEntries(const FString& RouteId, int32 Count = 10) const;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Leaderboard")
    int32 GetPlayerRank(const FString& RouteId, float TimeSeconds) const;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Leaderboard")
    bool SaveLeaderboard(const FString& RouteId);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Leaderboard")
    bool LoadLeaderboard(const FString& RouteId);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Leaderboard")
    void ClearLeaderboard(const FString& RouteId);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Leaderboard")
    void SeedDefaultEntries(const FString& RouteId, float GoldTime, float SilverTime, float BronzeTime);

    UFUNCTION(BlueprintPure, Category = "raceGPS|Leaderboard")
    bool HasLeaderboard(const FString& RouteId) const;

protected:
    TMap<FString, TArray<FLeaderboardEntry>> Leaderboards;

    FString GetLeaderboardPath(const FString& RouteId) const;
    void SortEntries(TArray<FLeaderboardEntry>& Entries) const;
};
