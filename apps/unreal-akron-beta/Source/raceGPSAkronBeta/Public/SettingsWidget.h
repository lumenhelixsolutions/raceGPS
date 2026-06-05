#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SettingsWidget.generated.h"

UCLASS()
class RACEGPSAKRONBETA_API USettingsWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Settings")
    void OnApplyClicked();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Settings")
    void OnResetClicked();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Settings")
    void OnBackClicked();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Settings")
    void OnTabChanged(int32 TabIndex);

    UPROPERTY(meta = (BindWidget))
    class UWidgetSwitcher* TabSwitcher;

    UPROPERTY(meta = (BindWidget))
    class UButton* ApplyButton;

    UPROPERTY(meta = (BindWidget))
    class UButton* ResetButton;

    UPROPERTY(meta = (BindWidget))
    class UButton* BackButton;

    UPROPERTY(meta = (BindWidget))
    class USlider* MasterVolumeSlider;

    UPROPERTY(meta = (BindWidget))
    class USlider* MusicVolumeSlider;

    UPROPERTY(meta = (BindWidget))
    class USlider* SFXVolumeSlider;

    UPROPERTY(meta = (BindWidget))
    class USlider* SteeringSensitivitySlider;

    UPROPERTY(meta = (BindWidget))
    class UCheckBox* InvertSteerCheck;

    UPROPERTY(meta = (BindWidget))
    class UCheckBox* FullscreenCheck;

    UPROPERTY(meta = (BindWidget))
    class UCheckBox* VSyncCheck;

protected:
    UPROPERTY()
    class USettingsSystem* Settings;

    void LoadSettingsToUI();
    void SaveUItoSettings();
};
