#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DeveloperConsole.generated.h"

UCLASS()
class RACEGPSAKRONBETA_API UDeveloperConsole : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual void NativeConstruct() override;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Debug")
    void ToggleVisibility();

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* FPSText;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* FrameTimeText;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* PingText;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* MemoryText;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* PositionText;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* VelocityText;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* RoadCountText;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Debug")
    float UpdateInterval = 0.5f;

protected:
    float AccumulatedTime = 0.0f;
    int32 FrameCount = 0;
    float AvgFPS = 0.0f;
    float AvgFrameTime = 0.0f;

    void UpdateMetrics(float DeltaTime);
    FString FormatBytes(int64 Bytes) const;
};
