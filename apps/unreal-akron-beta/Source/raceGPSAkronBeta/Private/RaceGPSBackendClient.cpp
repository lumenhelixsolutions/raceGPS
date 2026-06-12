#include "RaceGPSBackendClient.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "WebSocketsModule.h"
#include "IWebSocket.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
    FString ExtractCookieSid(const FString& SetCookie)
    {
        int32 Start = SetCookie.Find(TEXT("racegps_sid="));
        if (Start == INDEX_NONE)
        {
            return FString();
        }
        Start += 12;
        int32 End = SetCookie.Find(TEXT(";"), ESearchCase::IgnoreCase, ESearchDir::FromStart, Start);
        if (End == INDEX_NONE)
        {
            End = SetCookie.Len();
        }
        return SetCookie.Mid(Start, End - Start);
    }

    FString HttpToWs(const FString& BaseUrl)
    {
        FString Ws = BaseUrl;
        Ws.ReplaceInline(TEXT("https://"), TEXT("wss://"));
        Ws.ReplaceInline(TEXT("http://"), TEXT("ws://"));
        return Ws;
    }
}

void URaceGPSBackendClient::CreateSession(const FString& InDisplayName)
{
    DisplayName = InDisplayName.IsEmpty() ? TEXT("UE5-Driver") : InDisplayName;
    bSessionPending = true;

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(BackendBaseUrl + TEXT("/api/session"));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("displayName"), DisplayName);
    FString BodyStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
    FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);
    Request->SetContentAsString(BodyStr);

    Request->OnProcessRequestComplete().BindLambda(
        [this](FHttpRequestPtr, FHttpResponsePtr Response, bool bOk)
        {
            const FString Cookie = Response.IsValid() ? Response->GetHeader(TEXT("Set-Cookie")) : FString();
            const FString BodyText = Response.IsValid() ? Response->GetContentAsString() : FString();
            HandleSessionResponse(bOk && Response.IsValid() && Response->GetResponseCode() == 200, BodyText, Cookie);
        });

    Request->ProcessRequest();
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Creating backend session for %s"), *DisplayName);
}

void URaceGPSBackendClient::ListRooms()
{
    if (SessionCookie.IsEmpty())
    {
        OnRoomsListed.Broadcast(TEXT("[]"));
        return;
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(BackendBaseUrl + TEXT("/api/rooms"));
    Request->SetVerb(TEXT("GET"));
    Request->SetHeader(TEXT("Cookie"), FString::Printf(TEXT("racegps_sid=%s"), *SessionCookie));

    Request->OnProcessRequestComplete().BindLambda(
        [this](FHttpRequestPtr, FHttpResponsePtr Response, bool bOk)
        {
            HandleRoomsResponse(bOk && Response.IsValid() && Response->GetResponseCode() == 200,
                Response.IsValid() ? Response->GetContentAsString() : TEXT("[]"));
        });

    Request->ProcessRequest();
}

void URaceGPSBackendClient::CreateRoom(const FString& Title, const FString& Mode)
{
    if (SessionCookie.IsEmpty())
    {
        OnConnected.Broadcast(false);
        return;
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(BackendBaseUrl + TEXT("/api/rooms"));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("Cookie"), FString::Printf(TEXT("racegps_sid=%s"), *SessionCookie));

    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("title"), Title);
    Body->SetStringField(TEXT("mode"), Mode);
    FString BodyStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
    FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);
    Request->SetContentAsString(BodyStr);

    Request->OnProcessRequestComplete().BindLambda(
        [this](FHttpRequestPtr, FHttpResponsePtr Response, bool bOk)
        {
            HandleCreateRoomResponse(bOk && Response.IsValid() && Response->GetResponseCode() == 200,
                Response.IsValid() ? Response->GetContentAsString() : FString());
        });

    Request->ProcessRequest();
}

void URaceGPSBackendClient::ConnectWebSocket(const FString& RoomId)
{
    if (SessionCookie.IsEmpty())
    {
        OnConnected.Broadcast(false);
        return;
    }

    PendingRoomId = RoomId;
    const FString WsUrl = FString::Printf(TEXT("%s/ws/rooms/%s"), *HttpToWs(BackendBaseUrl), *RoomId);

    if (!FModuleManager::Get().IsModuleLoaded(TEXT("WebSockets")))
    {
        FModuleManager::LoadModuleChecked<FWebSocketsModule>(TEXT("WebSockets"));
    }

    TMap<FString, FString> Headers;
    Headers.Add(TEXT("Cookie"), FString::Printf(TEXT("racegps_sid=%s"), *SessionCookie));

    WebSocket = FWebSocketsModule::Get().CreateWebSocket(WsUrl, TEXT(""), Headers);
    WebSocket->OnConnected().AddUObject(this, &URaceGPSBackendClient::OnWebSocketConnected);
    WebSocket->OnConnectionError().AddUObject(this, &URaceGPSBackendClient::OnWebSocketConnectionError);
    WebSocket->OnClosed().AddUObject(this, &URaceGPSBackendClient::OnWebSocketClosed);
    WebSocket->OnMessage().AddUObject(this, &URaceGPSBackendClient::OnWebSocketMessage);
    WebSocket->Connect();

    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Connecting WebSocket to %s"), *WsUrl);
}

void URaceGPSBackendClient::SendChat(const FString& Message)
{
    if (!WebSocket.IsValid() || !bConnected || ActiveRoomId.IsEmpty())
    {
        return;
    }

    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("type"), TEXT("chat_message"));
    Obj->SetStringField(TEXT("roomId"), ActiveRoomId);
    Obj->SetStringField(TEXT("message"), Message);
    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
    WebSocket->Send(Out);
}

void URaceGPSBackendClient::Disconnect()
{
    if (WebSocket.IsValid())
    {
        WebSocket->Close();
        WebSocket.Reset();
    }
    bConnected = false;
    ActiveRoomId.Empty();
}

void URaceGPSBackendClient::HandleSessionResponse(bool bOk, const FString& ResponseBody, const FString& SetCookie)
{
    bSessionPending = false;
    if (!bOk)
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] Backend session failed"));
        OnSessionReady.Broadcast(false);
        return;
    }

    SessionCookie = ExtractCookieSid(SetCookie);
    TSharedPtr<FJsonObject> Json;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
    if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
    {
        Json->TryGetStringField(TEXT("playerId"), PlayerId);
        FString Name;
        if (Json->TryGetStringField(TEXT("displayName"), Name))
        {
            DisplayName = Name;
        }
    }

    const bool Ready = !SessionCookie.IsEmpty();
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Backend session ready player=%s"), *PlayerId);
    OnSessionReady.Broadcast(Ready);
}

void URaceGPSBackendClient::HandleRoomsResponse(bool bOk, const FString& ResponseBody)
{
    OnRoomsListed.Broadcast(bOk ? ResponseBody : TEXT("[]"));
}

void URaceGPSBackendClient::HandleCreateRoomResponse(bool bOk, const FString& ResponseBody)
{
    if (!bOk)
    {
        OnConnected.Broadcast(false);
        return;
    }

    TSharedPtr<FJsonObject> Json;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
    if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
    {
        FString RoomId;
        if (Json->TryGetStringField(TEXT("roomId"), RoomId))
        {
            ConnectWebSocket(RoomId);
            return;
        }
    }
    OnConnected.Broadcast(false);
}

void URaceGPSBackendClient::OnWebSocketConnected()
{
    bConnected = true;
    ActiveRoomId = PendingRoomId;
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] WebSocket connected room=%s"), *ActiveRoomId);
    OnConnected.Broadcast(true);
}

void URaceGPSBackendClient::OnWebSocketConnectionError(const FString& Error)
{
    UE_LOG(LogTemp, Warning, TEXT("[raceGPS] WebSocket error: %s"), *Error);
    bConnected = false;
    OnConnected.Broadcast(false);
}

void URaceGPSBackendClient::OnWebSocketClosed(int32 StatusCode, const FString& Reason, bool bWasClean)
{
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] WebSocket closed %d %s clean=%d"), StatusCode, *Reason, bWasClean);
    bConnected = false;
}

void URaceGPSBackendClient::OnWebSocketMessage(const FString& Message)
{
    OnMessage.Broadcast(Message);
}