#include "SettingsSystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "ChaosVehiclePawn.h"
#include "VehicleAudioComponent.h"
#include "GameFramework/GameUserSettings.h"

void USettingsSystem::LoadSettings()
{
    FString FullPath = GetSettingsPath();
    FString Content;
    if (!FFileHelper::LoadFileToString(Content, *FullPath))
    {
        ResetToDefaults();
        return;
    }

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
    if (!FJsonSerializer::Deserialize(Reader, Root))
    {
        ResetToDefaults();
        return;
    }

    const TSharedPtr<FJsonObject>* VideoObj;
    if (Root->TryGetObjectField(TEXT("video"), VideoObj))
    {
        (*VideoObj)->TryGetNumberField(TEXT("resolution_x"), Video.ResolutionX);
        (*VideoObj)->TryGetNumberField(TEXT("resolution_y"), Video.ResolutionY);
        (*VideoObj)->TryGetBoolField(TEXT("fullscreen"), Video.bFullscreen);
        (*VideoObj)->TryGetBoolField(TEXT("vsync"), Video.bVSync);
        (*VideoObj)->TryGetNumberField(TEXT("fov"), Video.FieldOfView);
        (*VideoObj)->TryGetNumberField(TEXT("shadows"), Video.ShadowQuality);
        (*VideoObj)->TryGetNumberField(TEXT("aa"), Video.AntiAliasing);
        (*VideoObj)->TryGetNumberField(TEXT("draw_distance"), Video.DrawDistance);
    }

    const TSharedPtr<FJsonObject>* AudioObj;
    if (Root->TryGetObjectField(TEXT("audio"), AudioObj))
    {
        (*AudioObj)->TryGetNumberField(TEXT("master"), Audio.MasterVolume);
        (*AudioObj)->TryGetNumberField(TEXT("music"), Audio.MusicVolume);
        (*AudioObj)->TryGetNumberField(TEXT("sfx"), Audio.SFXVolume);
        (*AudioObj)->TryGetNumberField(TEXT("engine"), Audio.EngineVolume);
        (*AudioObj)->TryGetBoolField(TEXT("mute_on_focus"), Audio.bMuteOnFocusLost);
    }

    const TSharedPtr<FJsonObject>* ControlObj;
    if (Root->TryGetObjectField(TEXT("controls"), ControlObj))
    {
        (*ControlObj)->TryGetNumberField(TEXT("steer_sens"), Controls.SteeringSensitivity);
        (*ControlObj)->TryGetNumberField(TEXT("throttle_sens"), Controls.ThrottleSensitivity);
        (*ControlObj)->TryGetBoolField(TEXT("invert_steer"), Controls.bInvertSteering);
        (*ControlObj)->TryGetBoolField(TEXT("invert_throttle"), Controls.bInvertThrottle);
        (*ControlObj)->TryGetBoolField(TEXT("manual_trans"), Controls.bManualTransmission);
        (*ControlObj)->TryGetBoolField(TEXT("toggle_handbrake"), Controls.bToggleHandbrake);
    }

    ApplyVideoSettings();
}

void USettingsSystem::SaveSettings()
{
    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();

    TSharedPtr<FJsonObject> VideoObj = MakeShared<FJsonObject>();
    VideoObj->SetNumberField(TEXT("resolution_x"), Video.ResolutionX);
    VideoObj->SetNumberField(TEXT("resolution_y"), Video.ResolutionY);
    VideoObj->SetBoolField(TEXT("fullscreen"), Video.bFullscreen);
    VideoObj->SetBoolField(TEXT("vsync"), Video.bVSync);
    VideoObj->SetNumberField(TEXT("fov"), Video.FieldOfView);
    VideoObj->SetNumberField(TEXT("shadows"), Video.ShadowQuality);
    VideoObj->SetNumberField(TEXT("aa"), Video.AntiAliasing);
    VideoObj->SetNumberField(TEXT("draw_distance"), Video.DrawDistance);
    Root->SetObjectField(TEXT("video"), VideoObj);

    TSharedPtr<FJsonObject> AudioObj = MakeShared<FJsonObject>();
    AudioObj->SetNumberField(TEXT("master"), Audio.MasterVolume);
    AudioObj->SetNumberField(TEXT("music"), Audio.MusicVolume);
    AudioObj->SetNumberField(TEXT("sfx"), Audio.SFXVolume);
    AudioObj->SetNumberField(TEXT("engine"), Audio.EngineVolume);
    AudioObj->SetBoolField(TEXT("mute_on_focus"), Audio.bMuteOnFocusLost);
    Root->SetObjectField(TEXT("audio"), AudioObj);

    TSharedPtr<FJsonObject> ControlObj = MakeShared<FJsonObject>();
    ControlObj->SetNumberField(TEXT("steer_sens"), Controls.SteeringSensitivity);
    ControlObj->SetNumberField(TEXT("throttle_sens"), Controls.ThrottleSensitivity);
    ControlObj->SetBoolField(TEXT("invert_steer"), Controls.bInvertSteering);
    ControlObj->SetBoolField(TEXT("invert_throttle"), Controls.bInvertThrottle);
    ControlObj->SetBoolField(TEXT("manual_trans"), Controls.bManualTransmission);
    ControlObj->SetBoolField(TEXT("toggle_handbrake"), Controls.bToggleHandbrake);
    Root->SetObjectField(TEXT("controls"), ControlObj);

    FString Content;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Content);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    FFileHelper::SaveStringToFile(Content, *GetSettingsPath());
}

void USettingsSystem::ApplyVideoSettings()
{
    UGameUserSettings* Settings = GEngine->GetGameUserSettings();
    if (!Settings)
        return;

    Settings->SetScreenResolution(FIntPoint(Video.ResolutionX, Video.ResolutionY));
    Settings->SetFullscreenMode(Video.bFullscreen ? EWindowMode::Fullscreen : EWindowMode::Windowed);
    Settings->SetVSyncEnabled(Video.bVSync);
    Settings->ApplySettings(false);
}

void USettingsSystem::ApplyAudioSettings()
{
    // Audio settings applied via VehicleAudioComponent or SoundMix
}

void USettingsSystem::ApplyControlSettings(AChaosVehiclePawn* Vehicle)
{
    if (!Vehicle)
        return;

    Vehicle->SteeringSensitivity = Controls.bInvertSteering ? -Controls.SteeringSensitivity : Controls.SteeringSensitivity;
    Vehicle->ThrottleSensitivity = Controls.bInvertThrottle ? -Controls.ThrottleSensitivity : Controls.ThrottleSensitivity;

    if (Vehicle->AudioComponent)
    {
        Vehicle->AudioComponent->SetMasterVolume(Audio.MasterVolume);
    }
}

void USettingsSystem::ResetToDefaults()
{
    Video = FVideoSettings();
    Audio = FAudioSettings();
    Controls = FControlSettings();
    ApplyVideoSettings();
}

FString USettingsSystem::GetSettingsPath() const
{
    return FPaths::ProjectSavedDir() / TEXT("settings.json");
}
