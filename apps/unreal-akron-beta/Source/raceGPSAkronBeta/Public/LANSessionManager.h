#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "FindSessionsCallbackProxy.h"
#include "LANSessionManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLANSessionFound, const TArray<FBlueprintSessionResult>&, Results);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLANSessionJoined, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLANSessionCreated, bool, bSuccess);

UCLASS()
class RACEGPSAKRONBETA_API ULANSessionManager : public UObject
{
    GENERATED_BODY()

public:
    ULANSessionManager();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|LAN")
    void HostSession(int32 MaxPlayers = 4, const FString& SessionName = TEXT("raceGPS LAN"));

    UFUNCTION(BlueprintCallable, Category = "raceGPS|LAN")
    void FindSessions(int32 MaxResults = 20);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|LAN")
    void JoinSession(const FBlueprintSessionResult& SessionResult);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|LAN")
    void DestroySession();

    UFUNCTION(BlueprintPure, Category = "raceGPS|LAN")
    bool IsHosting() const { return bIsHosting; }

    UFUNCTION(BlueprintPure, Category = "raceGPS|LAN")
    bool IsSearching() const { return bIsSearching; }

    UPROPERTY(BlueprintAssignable, Category = "raceGPS|LAN")
    FOnLANSessionFound OnSessionsFound;

    UPROPERTY(BlueprintAssignable, Category = "raceGPS|LAN")
    FOnLANSessionJoined OnSessionJoined;

    UPROPERTY(BlueprintAssignable, Category = "raceGPS|LAN")
    FOnLANSessionCreated OnSessionCreated;

protected:
    void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
    void OnFindSessionsComplete(bool bWasSuccessful);
    void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
    void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);

    FOnCreateSessionCompleteDelegate CreateSessionCompleteDelegate;
    FOnFindSessionsCompleteDelegate FindSessionsCompleteDelegate;
    FOnJoinSessionCompleteDelegate JoinSessionCompleteDelegate;
    FOnDestroySessionCompleteDelegate DestroySessionCompleteDelegate;

    FDelegateHandle CreateSessionCompleteDelegateHandle;
    FDelegateHandle FindSessionsCompleteDelegateHandle;
    FDelegateHandle JoinSessionCompleteDelegateHandle;
    FDelegateHandle DestroySessionCompleteDelegateHandle;

    TSharedPtr<FOnlineSessionSearch> SessionSearch;

    bool bIsHosting = false;
    bool bIsSearching = false;
};
