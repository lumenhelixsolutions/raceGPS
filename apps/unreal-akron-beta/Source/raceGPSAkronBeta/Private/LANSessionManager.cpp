#include "LANSessionManager.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

ULANSessionManager::ULANSessionManager()
{
    CreateSessionCompleteDelegate = FOnCreateSessionCompleteDelegate::CreateUObject(this, &ULANSessionManager::OnCreateSessionComplete);
    FindSessionsCompleteDelegate = FOnFindSessionsCompleteDelegate::CreateUObject(this, &ULANSessionManager::OnFindSessionsComplete);
    JoinSessionCompleteDelegate = FOnJoinSessionCompleteDelegate::CreateUObject(this, &ULANSessionManager::OnJoinSessionComplete);
    DestroySessionCompleteDelegate = FOnDestroySessionCompleteDelegate::CreateUObject(this, &ULANSessionManager::OnDestroySessionComplete);
}

void ULANSessionManager::HostSession(int32 MaxPlayers, const FString& SessionName)
{
    IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
    if (!OnlineSub)
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] OnlineSubsystem not available"));
        OnSessionCreated.Broadcast(false);
        return;
    }

    IOnlineSessionPtr SessionInterface = OnlineSub->GetSessionInterface();
    if (!SessionInterface.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] Session interface not available"));
        OnSessionCreated.Broadcast(false);
        return;
    }

    CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);

    FOnlineSessionSettings SessionSettings;
    SessionSettings.bIsLANMatch = true;
    SessionSettings.NumPublicConnections = MaxPlayers;
    SessionSettings.bShouldAdvertise = true;
    SessionSettings.bAllowJoinInProgress = true;
    SessionSettings.bIsDedicated = false;
    SessionSettings.bUsesPresence = true;
    SessionSettings.bAllowInvites = true;
    SessionSettings.bAllowJoinViaPresence = true;
    SessionSettings.bAllowJoinViaPresenceFriendsOnly = false;

    SessionSettings.Set(FName(TEXT("SESSIONNAME")), SessionName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

    UWorld* World = GEngine->GetWorldFromContextObjectChecked(this);
    APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
    FUniqueNetIdRepl UserId = PC ? PC->GetLocalPlayer()->GetPreferredUniqueNetId() : FUniqueNetIdRepl();

    SessionInterface->CreateSession(*UserId, FName(TEXT("raceGPS")), SessionSettings);
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Creating LAN session: %s (max %d players)"), *SessionName, MaxPlayers);
}

void ULANSessionManager::FindSessions(int32 MaxResults)
{
    IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
    if (!OnlineSub)
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] OnlineSubsystem not available"));
        return;
    }

    IOnlineSessionPtr SessionInterface = OnlineSub->GetSessionInterface();
    if (!SessionInterface.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] Session interface not available"));
        return;
    }

    FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);

    SessionSearch = MakeShareable(new FOnlineSessionSearch());
    SessionSearch->MaxSearchResults = MaxResults;
    SessionSearch->bIsLanQuery = true;
    SessionSearch->QuerySettings.Set(FName(TEXT("SEARCHPRESENCE")), true, EOnlineComparisonOp::Equals);

    UWorld* World = GEngine->GetWorldFromContextObjectChecked(this);
    APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
    FUniqueNetIdRepl UserId = PC ? PC->GetLocalPlayer()->GetPreferredUniqueNetId() : FUniqueNetIdRepl();

    bIsSearching = true;
    SessionInterface->FindSessions(*UserId, SessionSearch.ToSharedRef());
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Searching for LAN sessions..."));
}

void ULANSessionManager::JoinSession(const FBlueprintSessionResult& SessionResult)
{
    IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
    if (!OnlineSub)
    {
        OnSessionJoined.Broadcast(false);
        return;
    }

    IOnlineSessionPtr SessionInterface = OnlineSub->GetSessionInterface();
    if (!SessionInterface.IsValid())
    {
        OnSessionJoined.Broadcast(false);
        return;
    }

    JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);

    UWorld* World = GEngine->GetWorldFromContextObjectChecked(this);
    APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
    FUniqueNetIdRepl UserId = PC ? PC->GetLocalPlayer()->GetPreferredUniqueNetId() : FUniqueNetIdRepl();

    SessionInterface->JoinSession(*UserId, FName(TEXT("raceGPS")), SessionResult.OnlineResult);
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Joining LAN session..."));
}

void ULANSessionManager::DestroySession()
{
    IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
    if (!OnlineSub)
        return;

    IOnlineSessionPtr SessionInterface = OnlineSub->GetSessionInterface();
    if (!SessionInterface.IsValid())
        return;

    DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegate);
    SessionInterface->DestroySession(FName(TEXT("raceGPS")));
}

void ULANSessionManager::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
    IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
    if (OnlineSub)
    {
        IOnlineSessionPtr SessionInterface = OnlineSub->GetSessionInterface();
        if (SessionInterface.IsValid())
        {
            SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
        }
    }

    if (bWasSuccessful)
    {
        bIsHosting = true;
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] LAN session created: %s"), *SessionName.ToString());

        UWorld* World = GEngine->GetWorldFromContextObjectChecked(this);
        World->ServerTravel(TEXT("/Game/Maps/AkronWorld?listen"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] Failed to create LAN session"));
    }

    OnSessionCreated.Broadcast(bWasSuccessful);
}

void ULANSessionManager::OnFindSessionsComplete(bool bWasSuccessful)
{
    IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
    if (OnlineSub)
    {
        IOnlineSessionPtr SessionInterface = OnlineSub->GetSessionInterface();
        if (SessionInterface.IsValid())
        {
            SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
        }
    }

    bIsSearching = false;

    TArray<FBlueprintSessionResult> Results;
    if (bWasSuccessful && SessionSearch.IsValid())
    {
        for (const FOnlineSessionSearchResult& SearchResult : SessionSearch->SearchResults)
        {
            FBlueprintSessionResult BPResult;
            BPResult.OnlineResult = SearchResult;
            Results.Add(BPResult);
        }
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Found %d LAN sessions"), Results.Num());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] LAN session search failed or no results"));
    }

    OnSessionsFound.Broadcast(Results);
}

void ULANSessionManager::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
    IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
    if (OnlineSub)
    {
        IOnlineSessionPtr SessionInterface = OnlineSub->GetSessionInterface();
        if (SessionInterface.IsValid())
        {
            SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
        }
    }

    bool bSuccess = (Result == EOnJoinSessionCompleteResult::Success);

    if (bSuccess)
    {
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Joined LAN session: %s"), *SessionName.ToString());

        FString ConnectString;
        if (OnlineSub->GetSessionInterface()->GetResolvedConnectString(SessionName, ConnectString))
        {
            APlayerController* PC = UGameplayStatics::GetPlayerController(GEngine->GetWorldFromContextObjectChecked(this), 0);
            if (PC)
            {
                PC->ClientTravel(ConnectString, TRAVEL_Absolute);
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] Failed to join LAN session: %d"), static_cast<int32>(Result));
    }

    OnSessionJoined.Broadcast(bSuccess);
}

void ULANSessionManager::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
    IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
    if (OnlineSub)
    {
        IOnlineSessionPtr SessionInterface = OnlineSub->GetSessionInterface();
        if (SessionInterface.IsValid())
        {
            SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
        }
    }

    bIsHosting = false;
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] LAN session destroyed: %s"), *SessionName.ToString());
}
