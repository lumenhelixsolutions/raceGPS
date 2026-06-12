#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "RaceGPSBackendClient.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBackendSessionReady, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBackendRoomsListed, const FString&, RoomsJson);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBackendMessage, const FString&, Payload);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBackendConnected, bool, bSuccess);

/**
 * HTTP session + WebSocket client for the raceGPS Node backend (port 8787).
 * Replaces LAN-only discovery when bPreferNodeBackend is enabled.
 */
UCLASS(BlueprintType)
class RACEGPSAKRONBETA_API URaceGPSBackendClient : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Backend")
    FString BackendBaseUrl = TEXT("http://127.0.0.1:8787");

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Backend")
    FString SessionCookie;

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Backend")
    FString PlayerId;

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Backend")
    FString DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Backend")
    FString ActiveRoomId;

    UPROPERTY(BlueprintAssignable, Category = "raceGPS|Backend")
    FOnBackendSessionReady OnSessionReady;

    UPROPERTY(BlueprintAssignable, Category = "raceGPS|Backend")
    FOnBackendRoomsListed OnRoomsListed;

    UPROPERTY(BlueprintAssignable, Category = "raceGPS|Backend")
    FOnBackendMessage OnMessage;

    UPROPERTY(BlueprintAssignable, Category = "raceGPS|Backend")
    FOnBackendConnected OnConnected;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Backend")
    void CreateSession(const FString& InDisplayName);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Backend")
    void ListRooms();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Backend")
    void CreateRoom(const FString& Title, const FString& Mode = TEXT("cruise"));

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Backend")
    void ConnectWebSocket(const FString& RoomId);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Backend")
    void SendChat(const FString& Message);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Backend")
    void Disconnect();

    UFUNCTION(BlueprintPure, Category = "raceGPS|Backend")
    bool IsConnected() const { return bConnected; }

private:
    void HandleSessionResponse(bool bOk, const FString& ResponseBody, const FString& SetCookie);
    void HandleRoomsResponse(bool bOk, const FString& ResponseBody);
    void HandleCreateRoomResponse(bool bOk, const FString& ResponseBody);
    void OnWebSocketConnected();
    void OnWebSocketConnectionError(const FString& Error);
    void OnWebSocketClosed(int32 StatusCode, const FString& Reason, bool bWasClean);
    void OnWebSocketMessage(const FString& Message);

    TSharedPtr<class IWebSocket> WebSocket;
    bool bConnected = false;
    bool bSessionPending = false;
    FString PendingRoomId;
};