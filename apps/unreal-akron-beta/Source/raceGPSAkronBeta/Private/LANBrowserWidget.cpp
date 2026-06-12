#include "LANBrowserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ListView.h"
#include "Components/Slider.h"
#include "LANSessionManager.h"
#include "RaceGPSBackendClient.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void ULANBrowserWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (HostButton)
    {
        HostButton->OnClicked.AddDynamic(this, &ULANBrowserWidget::OnHostClicked);
    }
    if (JoinButton)
    {
        JoinButton->OnClicked.AddDynamic(this, &ULANBrowserWidget::OnJoinClicked);
    }
    if (RefreshButton)
    {
        RefreshButton->OnClicked.AddDynamic(this, &ULANBrowserWidget::OnRefreshClicked);
    }
    if (BackButton)
    {
        BackButton->OnClicked.AddDynamic(this, &ULANBrowserWidget::OnBackClicked);
    }
    if (MaxPlayersSlider)
    {
        MaxPlayersSlider->OnValueChanged.AddDynamic(this, &ULANBrowserWidget::UpdateMaxPlayersDisplay);
        MaxPlayersSlider->SetValue(4.0f);
    }

    LANManager = NewObject<ULANSessionManager>(this);
    if (LANManager)
    {
        LANManager->OnSessionsFound.AddDynamic(this, &ULANBrowserWidget::OnSessionsFound);
        LANManager->OnSessionJoined.AddDynamic(this, &ULANBrowserWidget::OnJoinComplete);
        LANManager->OnSessionCreated.AddDynamic(this, &ULANBrowserWidget::OnHostComplete);
    }

    if (bPreferNodeBackend)
    {
        BackendClient = NewObject<URaceGPSBackendClient>(this);
        if (BackendClient)
        {
            BackendClient->OnSessionReady.AddDynamic(this, &ULANBrowserWidget::OnBackendSessionReady);
            BackendClient->OnRoomsListed.AddDynamic(this, &ULANBrowserWidget::OnBackendRoomsListed);
            BackendClient->OnConnected.AddDynamic(this, &ULANBrowserWidget::OnBackendConnected);
            BackendClient->CreateSession(TEXT("UE5-Driver"));
        }
    }

    UpdateMaxPlayersDisplay();

    if (StatusText)
    {
        StatusText->SetText(FText::FromString(
            bPreferNodeBackend
                ? TEXT("Connecting to Node backend (8787)...")
                : TEXT("Click Refresh to find LAN sessions")));
    }
}

void ULANBrowserWidget::OnHostClicked()
{
    if (bPreferNodeBackend && BackendClient)
    {
        if (StatusText)
        {
            StatusText->SetText(FText::FromString(TEXT("Creating Node backend room...")));
        }
        BackendClient->CreateRoom(TEXT("UE5 Host Room"), TEXT("cruise"));
        return;
    }

    if (!LANManager)
        return;

    int32 MaxPlayers = MaxPlayersSlider ? FMath::RoundToInt(MaxPlayersSlider->GetValue()) : 4;
    if (StatusText)
    {
        StatusText->SetText(FText::FromString(TEXT("Creating LAN session...")));
    }
    LANManager->HostSession(MaxPlayers);
}

void ULANBrowserWidget::OnJoinClicked()
{
    if (!LANManager || SelectedSessionIndex < 0 || SelectedSessionIndex >= FoundSessions.Num())
        return;

    if (StatusText)
    {
        StatusText->SetText(FText::FromString(TEXT("Joining session...")));
    }
    LANManager->JoinSession(FoundSessions[SelectedSessionIndex]);
}

void ULANBrowserWidget::OnRefreshClicked()
{
    if (bPreferNodeBackend && BackendClient)
    {
        if (StatusText)
        {
            StatusText->SetText(FText::FromString(TEXT("Listing Node backend rooms...")));
        }
        BackendClient->ListRooms();
        return;
    }

    if (!LANManager)
        return;

    FoundSessions.Empty();
    SelectedSessionIndex = -1;

    if (StatusText)
    {
        StatusText->SetText(FText::FromString(TEXT("Searching for LAN sessions...")));
    }
    LANManager->FindSessions();
}

void ULANBrowserWidget::OnSessionSelected(int32 Index)
{
    SelectedSessionIndex = Index;
    if (StatusText && Index >= 0 && Index < FoundSessions.Num())
    {
        const FOnlineSessionSearchResult& Result = FoundSessions[Index].OnlineResult;
        FString SessionName = TEXT("Unknown");
        Result.Session.SessionSettings.Get(FName(TEXT("SESSIONNAME")), SessionName);
        FString Ping = FString::Printf(TEXT("%dms"), Result.PingInMs);
        StatusText->SetText(FText::FromString(FString::Printf(TEXT("Selected: %s (Ping: %s)"), *SessionName, *Ping)));
    }
}

void ULANBrowserWidget::OnBackClicked()
{
    RemoveFromParent();
}

void ULANBrowserWidget::OnSessionsFound(const TArray<FBlueprintSessionResult>& Results)
{
    FoundSessions = Results;

    if (StatusText)
    {
        if (Results.Num() == 0)
        {
            StatusText->SetText(FText::FromString(TEXT("No LAN sessions found. Try hosting one!")));
        }
        else
        {
            StatusText->SetText(FText::FromString(FString::Printf(TEXT("Found %d session(s)"), Results.Num())));
        }
    }

    // Build display list
    // Note: In a real implementation, you'd populate a ListView with a data source
    // For pure C++, we just log the sessions
    for (int32 i = 0; i < Results.Num(); ++i)
    {
        const FOnlineSessionSearchResult& Result = Results[i].OnlineResult;
        FString SessionName = TEXT("Unknown");
        Result.Session.SessionSettings.Get(FName(TEXT("SESSIONNAME")), SessionName);
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] LAN Session %d: %s (Ping: %dms, Players: %d/%d)"),
            i, *SessionName, Result.PingInMs,
            Result.Session.NumOpenPublicConnections,
            Result.Session.SessionSettings.NumPublicConnections);
    }
}

void ULANBrowserWidget::OnJoinComplete(bool bSuccess)
{
    if (StatusText)
    {
        StatusText->SetText(FText::FromString(bSuccess ? TEXT("Joined! Traveling...") : TEXT("Failed to join session.")));
    }
}

void ULANBrowserWidget::OnHostComplete(bool bSuccess)
{
    if (StatusText)
    {
        StatusText->SetText(FText::FromString(bSuccess ? TEXT("Hosting! Starting server...") : TEXT("Failed to host session.")));
    }
}

void ULANBrowserWidget::UpdateMaxPlayersDisplay()
{
    if (MaxPlayersText && MaxPlayersSlider)
    {
        int32 Val = FMath::RoundToInt(MaxPlayersSlider->GetValue());
        MaxPlayersText->SetText(FText::FromString(FString::Printf(TEXT("Max Players: %d"), Val)));
    }
}

void ULANBrowserWidget::OnBackendSessionReady(bool bSuccess)
{
    if (StatusText)
    {
        StatusText->SetText(FText::FromString(
            bSuccess ? TEXT("Backend session ready — Host or Refresh rooms")
            : TEXT("Backend session failed — start apps/backend (port 8787)")));
    }
}

void ULANBrowserWidget::OnBackendRoomsListed(const FString& RoomsJson)
{
    TArray<TSharedPtr<FJsonValue>> Rooms;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RoomsJson);
    if (!FJsonSerializer::Deserialize(Reader, Rooms))
    {
        if (StatusText)
        {
            StatusText->SetText(FText::FromString(TEXT("Failed to parse room list")));
        }
        return;
    }

    FString Summary;
    for (const TSharedPtr<FJsonValue>& Val : Rooms)
    {
        const TSharedPtr<FJsonObject> Obj = Val->AsObject();
        if (!Obj)
        {
            continue;
        }
        FString RoomId, Title;
        Obj->TryGetStringField(TEXT("roomId"), RoomId);
        Obj->TryGetStringField(TEXT("title"), Title);
        Summary += FString::Printf(TEXT("%s (%s)\n"), *Title, *RoomId);
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Backend room: %s — %s"), *Title, *RoomId);
    }

    if (StatusText)
    {
        StatusText->SetText(FText::FromString(
            Rooms.Num() > 0
                ? FString::Printf(TEXT("Backend rooms (%d):\n%s"), Rooms.Num(), *Summary)
                : TEXT("No backend rooms — click Host to create one")));
    }
}

void ULANBrowserWidget::OnBackendConnected(bool bSuccess)
{
    if (StatusText)
    {
        StatusText->SetText(FText::FromString(
            bSuccess ? TEXT("Connected to Node backend — WebSocket live")
            : TEXT("WebSocket connection failed")));
    }
    if (bSuccess)
    {
        OnHostComplete(true);
    }
}
