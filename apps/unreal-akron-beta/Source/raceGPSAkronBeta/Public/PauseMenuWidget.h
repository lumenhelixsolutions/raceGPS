#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PauseMenuWidget.generated.h"

UCLASS()
class RACEGPSAKRONBETA_API UPauseMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Menu")
    void OnResumeClicked();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Menu")
    void OnRestartClicked();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Menu")
    void OnSettingsClicked();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Menu")
    void OnQuitToMenuClicked();

    UPROPERTY(meta = (BindWidget))
    class UButton* ResumeButton;

    UPROPERTY(meta = (BindWidget))
    class UButton* RestartButton;

    UPROPERTY(meta = (BindWidget))
    class UButton* SettingsButton;

    UPROPERTY(meta = (BindWidget))
    class UButton* QuitToMenuButton;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* RaceTimeText;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* CheckpointText;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Menu")
    FString MenuLevelName = TEXT("MainMenu");
};
