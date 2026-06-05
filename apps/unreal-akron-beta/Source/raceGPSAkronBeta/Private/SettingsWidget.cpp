#include "SettingsWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/CheckBox.h"
#include "SettingsSystem.h"
#include "Kismet/GameplayStatics.h"
#include "ChaosVehiclePawn.h"

void USettingsWidget::NativeConstruct()
{
    Super::NativeConstruct();

    Settings = NewObject<USettingsSystem>(this);
    Settings->LoadSettings();
    LoadSettingsToUI();

    if (ApplyButton)
    {
        ApplyButton->OnClicked.AddDynamic(this, &USettingsWidget::OnApplyClicked);
    }
    if (ResetButton)
    {
        ResetButton->OnClicked.AddDynamic(this, &USettingsWidget::OnResetClicked);
    }
    if (BackButton)
    {
        BackButton->OnClicked.AddDynamic(this, &USettingsWidget::OnBackClicked);
    }
}

void USettingsWidget::LoadSettingsToUI()
{
    if (!Settings)
        return;

    if (MasterVolumeSlider)
        MasterVolumeSlider->SetValue(Settings->Audio.MasterVolume);
    if (MusicVolumeSlider)
        MusicVolumeSlider->SetValue(Settings->Audio.MusicVolume);
    if (SFXVolumeSlider)
        SFXVolumeSlider->SetValue(Settings->Audio.SFXVolume);
    if (SteeringSensitivitySlider)
        SteeringSensitivitySlider->SetValue(Settings->Controls.SteeringSensitivity);
    if (InvertSteerCheck)
        InvertSteerCheck->SetCheckedState(Settings->Controls.bInvertSteering ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
    if (FullscreenCheck)
        FullscreenCheck->SetCheckedState(Settings->Video.bFullscreen ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
    if (VSyncCheck)
        VSyncCheck->SetCheckedState(Settings->Video.bVSync ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
}

void USettingsWidget::SaveUItoSettings()
{
    if (!Settings)
        return;

    if (MasterVolumeSlider)
        Settings->Audio.MasterVolume = MasterVolumeSlider->GetValue();
    if (MusicVolumeSlider)
        Settings->Audio.MusicVolume = MusicVolumeSlider->GetValue();
    if (SFXVolumeSlider)
        Settings->Audio.SFXVolume = SFXVolumeSlider->GetValue();
    if (SteeringSensitivitySlider)
        Settings->Controls.SteeringSensitivity = SteeringSensitivitySlider->GetValue();
    if (InvertSteerCheck)
        Settings->Controls.bInvertSteering = InvertSteerCheck->IsChecked();
    if (FullscreenCheck)
        Settings->Video.bFullscreen = FullscreenCheck->IsChecked();
    if (VSyncCheck)
        Settings->Video.bVSync = VSyncCheck->IsChecked();
}

void USettingsWidget::OnApplyClicked()
{
    SaveUItoSettings();
    Settings->SaveSettings();
    Settings->ApplyVideoSettings();

    AChaosVehiclePawn* Vehicle = Cast<AChaosVehiclePawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
    if (Vehicle)
    {
        Settings->ApplyControlSettings(Vehicle);
    }
}

void USettingsWidget::OnResetClicked()
{
    Settings->ResetToDefaults();
    LoadSettingsToUI();
}

void USettingsWidget::OnBackClicked()
{
    RemoveFromParent();
}

void USettingsWidget::OnTabChanged(int32 TabIndex)
{
    if (TabSwitcher)
    {
        TabSwitcher->SetActiveWidgetIndex(TabIndex);
    }
}
