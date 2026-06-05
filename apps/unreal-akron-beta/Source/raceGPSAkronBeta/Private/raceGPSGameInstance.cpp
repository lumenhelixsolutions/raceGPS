#include "raceGPSGameInstance.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

UraceGPSGameInstance::UraceGPSGameInstance(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UraceGPSGameInstance::Init()
{
    Super::Init();
    LoadSettings();
}

FString UraceGPSGameInstance::GetSaveSlotName() const
{
    return FPaths::ProjectSavedDir() / TEXT("raceGPS_settings.json");
}

void UraceGPSGameInstance::SaveSettings()
{
    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetNumberField(TEXT("MasterVolume"), MasterVolume);
    Root->SetNumberField(TEXT("MusicVolume"), MusicVolume);
    Root->SetNumberField(TEXT("SFXVolume"), SFXVolume);
    Root->SetBoolField(TEXT("InvertSteering"), bInvertSteering);
    Root->SetNumberField(TEXT("SteeringSensitivity"), SteeringSensitivity);
    Root->SetStringField(TEXT("LastSelectedRoute"), LastSelectedRoute);
    Root->SetStringField(TEXT("LastSelectedVehicle"), LastSelectedVehicle);

    TSharedPtr<FJsonObject> BestTimesObj = MakeShared<FJsonObject>();
    for (const auto& Pair : BestTimes)
    {
        BestTimesObj->SetNumberField(Pair.Key, Pair.Value);
    }
    Root->SetObjectField(TEXT("BestTimes"), BestTimesObj);

    FString Content;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Content);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    FFileHelper::SaveStringToFile(Content, *GetSaveSlotName());
}

void UraceGPSGameInstance::LoadSettings()
{
    FString Content;
    if (!FFileHelper::LoadFileToString(Content, *GetSaveSlotName()))
        return;

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
    if (!FJsonSerializer::Deserialize(Reader, Root))
        return;

    double Val = 0.0;
    if (Root->TryGetNumberField(TEXT("MasterVolume"), Val)) MasterVolume = static_cast<float>(Val);
    if (Root->TryGetNumberField(TEXT("MusicVolume"), Val)) MusicVolume = static_cast<float>(Val);
    if (Root->TryGetNumberField(TEXT("SFXVolume"), Val)) SFXVolume = static_cast<float>(Val);
    Root->TryGetBoolField(TEXT("InvertSteering"), bInvertSteering);
    if (Root->TryGetNumberField(TEXT("SteeringSensitivity"), Val)) SteeringSensitivity = static_cast<float>(Val);
    Root->TryGetStringField(TEXT("LastSelectedRoute"), LastSelectedRoute);
    Root->TryGetStringField(TEXT("LastSelectedVehicle"), LastSelectedVehicle);

    TSharedPtr<FJsonObject> BestTimesObj;
    if (Root->TryGetObjectField(TEXT("BestTimes"), BestTimesObj))
    {
        BestTimes.Empty();
        for (const auto& Pair : BestTimesObj->Values)
        {
            double Time = 0.0;
            if (Pair.Value->TryGetNumber(Time))
            {
                BestTimes.Add(Pair.Key, static_cast<float>(Time));
            }
        }
    }
}

void UraceGPSGameInstance::UpdateBestTime(const FString& RouteId, float TimeSeconds)
{
    float* Existing = BestTimes.Find(RouteId);
    if (!Existing || TimeSeconds < *Existing)
    {
        BestTimes.Add(RouteId, TimeSeconds);
        SaveSettings();
    }
}

float UraceGPSGameInstance::GetBestTime(const FString& RouteId) const
{
    const float* Existing = BestTimes.Find(RouteId);
    return Existing ? *Existing : -1.0f;
}

FString UraceGPSGameInstance::GetMedalForTime(float TimeSeconds) const
{
    if (TimeSeconds <= 0.0f)
        return TEXT("NONE");
    if (TimeSeconds <= 120.0f)
        return TEXT("GOLD");
    if (TimeSeconds <= 150.0f)
        return TEXT("SILVER");
    if (TimeSeconds <= 200.0f)
        return TEXT("BRONZE");
    return TEXT("NONE");
}
