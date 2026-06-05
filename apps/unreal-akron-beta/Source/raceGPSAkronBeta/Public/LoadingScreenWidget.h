#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoadingScreenWidget.generated.h"

UCLASS()
class RACEGPSAKRONBETA_API ULoadingScreenWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Loading")
    void SetProgress(float Percent);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Loading")
    void SetStatusText(const FString& Text);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Loading")
    void StartLoading();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Loading")
    void FinishLoading();

    UPROPERTY(meta = (BindWidget))
    class UProgressBar* ProgressBar;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* StatusText;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* CityNameText;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* TipText;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Loading")
    TArray<FString> LoadingTips;

protected:
    float TargetProgress = 0.0f;
    float DisplayProgress = 0.0f;
    float TipTimer = 0.0f;
    int32 CurrentTipIndex = 0;

    void UpdateTip(float DeltaTime);
};
